# scripts/test_invariants.ps1 - the regression net. Assert, do not just print.
#
# Every serious defect in this project so far was a SILENT one: a flat damper
# that emptied the market by tick 6, a payroll rate that bankrupted every corp
# while the market looked busy, a "rebuild" that compiled nothing and exited 0,
# a format string that dropped columns without complaint, and a CRLF that
# silently classified 22 of 29 goods. Not one of them crashed, and not one
# would have been caught by a test that only checks exit codes.
#
# The common shape is: output that looks reasonable and is wrong. So this file
# asserts INVARIANTS over the world state and FAILS LOUDLY, because printing a
# table is how the bugs got in.
#
# Run with -Reset to rebuild a world first, or against a live one:
#   powershell -File scripts/test_invariants.ps1 -Reset
#   powershell -File scripts/test_invariants.ps1 -Ticks 12 -Reset
param(
    [switch]$Reset,
    [int]$Ticks = 8
)

$ErrorActionPreference = "Stop"
$SCRIPT_DIR = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
Set-Location $SCRIPT_DIR

$PIECES = Join-Path $SCRIPT_DIR "projects\wsr-pal\pieces"
$DATA   = Join-Path $SCRIPT_DIR "projects\wsr-pal\data"
$failures = @()

function Fail([string]$msg, [object[]]$fmtArgs) {
    $script:failures += ($msg -f $fmtArgs)
    Write-Host ("  FAIL  " + ($msg -f $fmtArgs)) -ForegroundColor Red
}
# $fmtArgs exists so callers can use -f style. Without it, `Pass "x {0}" -f 5`
# silently printed the literal `{0}` - a test that reports a format placeholder
# instead of a number is the same silent-wrongness class as everything else this
# file exists to catch.
function Pass([string]$msg, [object[]]$fmtArgs) {
    Write-Host ("  ok    " + ($msg -f $fmtArgs)) -ForegroundColor DarkGreen
}

function Get-Entities([string]$Filter) {
    Get-ChildItem $PIECES -Directory -Filter $Filter -EA SilentlyContinue | ForEach-Object {
        $st = Join-Path $_.FullName "state.txt"
        if (-not (Test-Path $st)) { return }
        $raw = Get-Content $st -Raw
        $m = [regex]::Match($raw, '(?m)^cash=([\d.\-]+)\s*$')
        if (-not $m.Success) { throw "no cash= field in $st" }
        [pscustomobject]@{
            Id   = $_.Name
            Cash = [double]$m.Groups[1].Value
            Raw  = $raw
        }
    }
}

function Get-TotalUnits {
    $tot = 0L
    Get-ChildItem $PIECES -Recurse -Filter "goods.txt" -EA SilentlyContinue | ForEach-Object {
        Get-Content $_.FullName | ForEach-Object {
            if ($_ -match '^([A-Z_]+)\|(\d+)\s*$') { $tot += [long]$Matches[2] }
        }
    }
    $tot
}

function Get-LedgerTotals {
    $lf = Join-Path $DATA "market_ledger.txt"
    if (-not (Test-Path $lf)) { return $null }
    # Only rows from this run's window matter; the file is append-only and large.
    $buy = 0.0; $sell = 0.0; $wages = 0.0; $n = 0
    Get-Content $lf -Tail 20000 | ForEach-Object {
        if ($_ -notmatch 'Amount: ([\d.]+) Dollars') { return }
        $amt = [double]$Matches[1]
        if ($_ -match 'Event: goods_settle') { $buy += $amt; $sell += $amt; $n++ }
        elseif ($_ -match 'Event: corp_payroll') { $wages += $amt; $n++ }
    }
    [pscustomobject]@{ Goods = $n; GoodsValue = $sell; Wages = $wages; Rows = $n }
}

Write-Host "building goods ops..." -ForegroundColor DarkGray
$env:Path = "C:\msys64\mingw64\bin;$env:Path"
foreach ($op in @("goods_quote", "goods_settle", "corp_payroll", "goods_sink")) {
    cmd /c "gcc -Wall -Wextra -O2 ops\$op.c -o ops\+x\$op.+x 2>&1" | ForEach-Object {
        if ($_ -match 'error') { throw "BUILD FAILED $op : $_" }
    }
    if ($LASTEXITCODE -ne 0) { throw "BUILD FAILED: $op" }
}

if ($Reset) {
    Write-Host "resetting world..." -ForegroundColor DarkGray
    Get-ChildItem $PIECES -Directory | ForEach-Object {
        Remove-Item (Join-Path $_.FullName "state.txt") -Force -EA SilentlyContinue
        Remove-Item (Join-Path $_.FullName "goods.txt") -Force -EA SilentlyContinue
    }
    # Delete GENERATED market state ONLY, by exact prefix. A loose "g*.txt" glob
    # also matches goods_kind.txt and goods_input.txt, which are COMMITTED SOURCE,
    # not runtime state - and deleting those silently disables every sink, so
    # goods_sink fails each tick and units run away. Caught by invariant 1 and
    # the runaway check on this harness's own first run. Never glob here.
    foreach ($pat in @("gbook_*.txt", "gsold_*.txt", "gobs_*.txt")) {
        Get-ChildItem $DATA -Filter $pat -EA SilentlyContinue | Remove-Item -Force
    }
    Remove-Item (Join-Path $DATA "goods_period.txt") -Force -EA SilentlyContinue
    cmd /c "powershell -ExecutionPolicy Bypass -File scripts\ensure_entities.ps1" | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "ensure_entities.ps1 failed" }
}

Write-Host ""
Write-Host "INVARIANTS" -ForegroundColor Cyan

# ---- 1. every produced good has a sink classification ---------------------
# The CRLF bug dropped 7 of 29 goods here and nothing reported it.
$kindFile = Join-Path $DATA "goods_kind.txt"
if (-not (Test-Path $kindFile)) {
    Fail "goods_kind.txt missing - no sink, no obsolescence, no cycle"
} else {
    $classified = @{}
    Get-Content $kindFile | ForEach-Object {
        if ($_ -match '^([A-Z_]+)\|(CONSUMABLE|DEPRECIATING|OBSOLESCENT)') { $classified[$Matches[1]] = $Matches[2] }
    }
    $produced = (Get-Entities "corp_*" | Where-Object { $_.Raw -match '(?m)^produces=(.+)$' } |
                 ForEach-Object { [regex]::Match($_.Raw, '(?m)^produces=(.+)$').Groups[1].Value.Trim() } | Sort-Object -Unique)
    $unclassified = @($produced | Where-Object { -not $classified.ContainsKey($_) })
    if ($unclassified.Count -gt 0) {
        Fail "$($unclassified.Count) produced good(s) have no goods_kind row: $($unclassified -join ',')"
    } else {
        Pass "all $($produced.Count) produced good(s) are classified for a sink ($($classified.Count) classified)"
    }
}

# ---- 2. no entity is invented, and roster count is discovered ------------
$corps = @(Get-Entities "corp_*")
$govs  = @(Get-Entities "gov_*")
$pops  = @(Get-Entities "pop_*")
if ($corps.Count -eq 0) { Fail "no corp entities found - the world is empty" }
else { Pass "discovered $($corps.Count) corps, $($govs.Count) governments, $($pops.Count) households (discovered, not assumed)" }

# ---- 3. no negative cash anywhere ---------------------------------------
# A negative balance means an op paid out money that was not there. The single
# most damaging silent failure this sim could have.
$neg = @(Get-Entities "*" | Where-Object { $_.Cash -lt -0.001 })
if ($neg.Count -gt 0) {
    Fail "$($neg.Count) entity(ies) have NEGATIVE cash: " + (($neg | Select-Object -First 5 | ForEach-Object { "$($_.Id)=$($_.Cash)" }) -join ', ')
} else {
    Pass "no entity has negative cash"
}

# ---- 4. every household can still buy, or we know it ---------------------
# Not a failure by itself (a bust economy is legal), but it must be REPORTED
# rather than discovered 30 ticks later.
$broke = @(Get-Entities "pop_*" | Where-Object { $_.Cash -le 1.0 })
if ($broke.Count -gt 0) {
    Write-Host ("  WARN  {0}/{1} households cannot bid (cash <= 1)" -f $broke.Count, $pops.Count) -ForegroundColor Yellow
} else {
    Pass "every household can still bid"
}

# ---- 5. the goods market is actually two-sided ---------------------------
$books = @(Get-ChildItem $DATA -Filter "gbook_*.txt" -EA SilentlyContinue)
if ($books.Count -eq 0) {
    Write-Host "  WARN  no goods books - market has not run yet" -ForegroundColor Yellow
} else {
    $twoSided = 0
    foreach ($b in $books) {
        $a = 0; $d = 0
        Get-Content $b.FullName | ForEach-Object {
            if ($_ -match '\|-1\|') { $a++ } elseif ($_ -match '\|1\|') { $d++ }
        }
        if ($a -gt 0 -and $d -gt 0) { $twoSided++ }
    }
    if ($twoSided -eq 0) { Fail "no goods book is two-sided - nothing can ever match" }
    else { Pass "$twoSided/$($books.Count) goods book(s) are two-sided" }
}

# ---- 6. units in circulation are bounded, not monotonically exploding -----
# Before the sink existed, units went 1,000 -> 104,881 over 16 ticks.
if ($Ticks -gt 0) {
    Write-Host ""
    Write-Host ("running {0} tick(s) and watching for runaway..." -f $Ticks) -ForegroundColor Cyan
    # Track the unit trajectory, not just its endpoints.
    $u0 = Get-TotalUnits
    $c0 = (Get-Entities "corp_*" | Measure-Object Cash -Sum).Sum
    $sunkTotal = 0L
    $series = @($u0)

    for ($i = 1; $i -le $Ticks; $i++) {
        foreach ($op in @("goods_quote", "goods_settle", "corp_payroll", "goods_sink")) {
            $out = cmd /c "ops\+x\$op.+x 2>&1"
            if ($LASTEXITCODE -ne 0) { Fail "$op failed on tick $i" }
            if ($op -eq "goods_sink") {
                $m = [regex]::Match(($out -join "`n"), 'consumed (\d+)')
                if ($m.Success) { $sunkTotal += [long]$m.Groups[1].Value }
                else { Fail "goods_sink printed no consumption figure on tick $i" }
            }
        }
        $negNow = @(Get-Entities "*" | Where-Object { $_.Cash -lt -0.001 })
        if ($negNow.Count -gt 0) { Fail "negative cash appeared on tick $i" }
        $series += (Get-TotalUnits)
    }

    $u1 = Get-TotalUnits
    $cNow = (Get-Entities "corp_*" | Measure-Object Cash -Sum).Sum
    $p1 = @(Get-Entities "pop_*" | Where-Object { $_.Cash -le 1.0 })

    # ---- THE SINK IS THE INVARIANT, not a magic growth multiple ----------
    # A fixed multiple is the wrong test: healthy early growth is linear (+1,200
    # units/tick here) and no multiple separates that from the pre-sink
    # exponential run without also firing on startup. What actually matters is
    # that the sink is alive and PROPORTIONAL to the stock it is draining -
    # that is the property that bounds supply no matter how fast producers run.
    if ($sunkTotal -le 0) {
        Fail "the sink consumed nothing across $Ticks ticks - nothing bounds supply"
    } else {
        $window = ($series | Measure-Object -Maximum).Maximum
        $frac = $sunkTotal / [double]$window
        Write-Host ("  sink removed {0} unit(s), {1:N1}% of peak stock {2}" -f $sunkTotal, ($frac * 100), $window)
        if ($frac -lt 0.02) {
            Fail ("sink removed only {0:N2}% of peak stock - too weak to bound supply" -f ($frac * 100))
        } else {
            Pass "sink is active and proportional ({0:N1}% of peak stock removed)" -f ($frac * 100)
        }
    }

    # Runaway = ACCELERATING, not merely large. Compare early and late
    # per-tick growth; linear growth keeps them flat, exponential separates them.
    $incs = @()
    for ($i = 1; $i -lt $series.Count; $i++) { $incs += ($series[$i] - $series[$i - 1]) }
    if ($incs.Count -ge 6) {
        $half = [int]($incs.Count / 2)
        $early = ($incs[0..($half - 1)] | Measure-Object -Average).Average
        $late  = ($incs[-$half..-1]        | Measure-Object -Average).Average
        Write-Host ("  unit growth per tick: early {0:N0} -> late {1:N0}  (series {2} -> {3})" -f `
            $early, $late, $u0, $u1)
        if ($late -gt ([math]::Max($early, 1) * 3) -and $late -gt 2000) {
            Fail "unit growth is ACCELERATING (early {0:N0}/tick -> late {1:N0}/tick) - supply is not converging" -f $early, $late
        } else {
            Pass "unit growth is not accelerating (early {0:N0}/tick, late {1:N0}/tick)" -f $early, $late
        }
    }
}

Write-Host ""
if ($failures.Count -eq 0) {
    Write-Host "ALL INVARIANTS HELD" -ForegroundColor Green
    exit 0
} else {
    Write-Host "$($failures.Count) INVARIANT FAILURE(S)" -ForegroundColor Red
    exit 1
}
