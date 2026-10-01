# scripts/test_goods_loop.ps1 - run the goods market for N ticks and report.
#
# Answers the question ad-hoc one-liners kept failing to answer: does the
# produce/sell/buy loop CONVERGE, or does it run away / die? Production
# steering is only correct if there is a steady state to converge to.
#
# Every read here FAILS LOUDLY. A reset that silently finds no `produces=`
# and writes nothing leaves the market empty with no error, which is a full
# diagnostic cycle lost. See KNOWN-ISSUES.md, house-level traps.
#
# IMPORTANT: rebuild first with scripts/build.ps1. Running an op is not
# building it, and a stale binary makes this report a lie.
#
# Usage: powershell -File scripts/test_goods_loop.ps1 -Ticks 12 [-Reset]

param(
    [int]$Ticks = 12,
    [switch]$Reset
)

$ErrorActionPreference = "Stop"
$SCRIPT_DIR = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$env:Path = "C:\msys64\mingw64\bin;$env:Path"
Set-Location $SCRIPT_DIR

$PIECES = Join-Path $SCRIPT_DIR "projects\wsr-pal\pieces"
$DATA   = Join-Path $SCRIPT_DIR "projects\wsr-pal\data"

# --- build (the op that this whole file exists to test) --------------------
Write-Host "building goods ops..." -ForegroundColor DarkGray
foreach ($op in @("goods_quote", "goods_settle", "corp_payroll", "goods_sink")) {
    $out = "ops\+x\$op.+x"
    cmd /c "gcc -Wall -Wextra -O2 ops\$op.c -o $out 2>&1" | ForEach-Object {
        if ($_ -match 'warning|error') { Write-Host "  $_" -ForegroundColor Yellow }
    }
    if ($LASTEXITCODE -ne 0) { throw "BUILD FAILED: $op" }
}

function Get-CashOf {
    param([string]$Filter)
    $total = 0.0; $n = 0
    Get-ChildItem $PIECES -Directory -Filter $Filter -EA SilentlyContinue | ForEach-Object {
        $st = Join-Path $_.FullName "state.txt"
        if (-not (Test-Path $st)) { return }
        $m = [regex]::Match((Get-Content $st -Raw), '(?m)^cash=([\d.\-]+)\s*$')
        if (-not $m.Success) { throw "no cash= field in $st" }
        $total += [double]$m.Groups[1].Value; $n++
    }
    if ($n -eq 0) { throw "no entities matched '$Filter'" }
    [pscustomobject]@{ Total = $total; Count = $n }
}

function Get-UnitsInCirculation {
    $tot = 0L; $n = 0
    Get-ChildItem $PIECES -Recurse -Filter "goods.txt" -EA SilentlyContinue | ForEach-Object {
        Get-Content $_.FullName | ForEach-Object {
            if ($_ -match '^([A-Z_]+)\|(\d+)$') { $tot += [long]$Matches[2]; $n++ }
        }
    }
    [pscustomobject]@{ Total = $tot; Rows = $n }
}

function Get-SellThrough {
    $sum = 0L; $off = 0L; $rows = 0
    Get-ChildItem $DATA -Filter "gsold_*.txt" -EA SilentlyContinue | ForEach-Object {
        Get-Content $_.FullName | ForEach-Object {
            $p = $_ -split '\|'
            if ($p.Count -ge 3) {
                $sum += [long]$p[1]; $off += [long]$p[2]; $rows++
            }
        }
    }
    $ratio = if ($off -gt 0) { [math]::Round($sum / $off, 3) } else { "n/a" }
    [pscustomobject]@{ Sold = $sum; Offered = $off; Ratio = $ratio; Rows = $rows }
}

# --- optional reset -------------------------------------------------------
if ($Reset) {
    Write-Host "resetting world (destroying entity state)" -ForegroundColor DarkGray
    Get-ChildItem $PIECES -Directory | ForEach-Object {
        Remove-Item (Join-Path $_.FullName "state.txt") -Force -EA SilentlyContinue
        Remove-Item (Join-Path $_.FullName "goods.txt") -Force -EA SilentlyContinue
    }
    Get-ChildItem $DATA -Filter "gsold_*.txt" -EA SilentlyContinue | Remove-Item -Force
    Get-ChildItem $DATA -Filter "gbook_*.txt" -EA SilentlyContinue | Remove-Item -Force
    cmd /c "powershell -ExecutionPolicy Bypass -File scripts\ensure_entities.ps1" | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "ensure_entities.ps1 failed" }

    # Verify the reset actually produced producers with inventory, loudly.
    $withStock = 0
    Get-ChildItem $PIECES -Directory -Filter "corp_*" | ForEach-Object {
        $m = [regex]::Match((Get-Content (Join-Path $_.FullName "state.txt") -Raw), '(?m)^produces=(.+)$')
        if (-not $m.Success) { throw "corp $($_.Name) has no produces= - reset is broken" }
        $g = Join-Path $_.FullName "goods.txt"
        if ((Test-Path $g) -and (Get-Content $g -Raw).Trim()) { $withStock++ }
    }
    if ($withStock -eq 0) { throw "reset produced 0 stocked corps - refusing to run a meaningless test" }
    Write-Host "  reset ok: $withStock stocked producers" -ForegroundColor DarkGray
}

Write-Host ""
Write-Host ("{0,-5} {1,9} {2,7} {3,9} {4,11} {5,13} {6,11} {7,11} {8,8} {9,7} {10,6}" -f `
    "tick", "produced", "fills", "s-thru", "wages", "corp_cash", "pop_cash", "units", "broke?", "sunk", "rd")
Write-Host ("-" * 100)

$prevPop = $null
for ($i = 1; $i -le $Ticks; $i++) {
    $q = cmd /c "ops\+x\goods_quote.+x 2>&1"
    if ($LASTEXITCODE -ne 0) { Write-Host ($q -join "`n"); throw "goods_quote failed on tick $i" }
    $s = cmd /c "ops\+x\goods_settle.+x 2>&1"
    if ($LASTEXITCODE -ne 0) { Write-Host ($s -join "`n"); throw "goods_settle failed on tick $i" }

    # The wage leg. Order matters: payroll is sized off revenue booked THIS
    # tick, so it must run after settle or it always pays one tick behind.
    $w = cmd /c "ops\+x\corp_payroll.+x 2>&1"
    if ($LASTEXITCODE -ne 0) { Write-Host ($w -join "`n"); throw "corp_payroll failed on tick $i" }
    $wages = [regex]::Match(($w -join "`n"), 'wages ([\d.]+) total').Groups[1].Value

    # The sink. Runs AFTER settlement, so units bought this period can be
    # consumed in it.
    $k = cmd /c "ops\+x\goods_sink.+x 2>&1"
    if ($LASTEXITCODE -ne 0) { Write-Host ($k -join "`n"); throw "goods_sink failed on tick $i" }
    $consumed = [regex]::Match(($k -join "`n"), 'consumed (\d+)').Groups[1].Value
    $breaks = [regex]::Match(($k -join "`n"), '(\d+) R&D breakthrough').Groups[1].Value

    $prod = [regex]::Match(($q -join "`n"), 'produced (\d+)').Groups[1].Value
    if (-not $prod) { throw "could not parse 'produced N' from goods_quote on tick $i" }
    $fill = [regex]::Match(($s -join "`n"), '(\d+) fill\(s\) total').Groups[1].Value

    $cc = Get-CashOf "corp_*"
    $pc = Get-CashOf "pop_*"
    $un = Get-UnitsInCirculation
    $st = Get-SellThrough

    # Households with no cash can no longer bid; that is the failure signal.
    $broke = 0
    Get-ChildItem $PIECES -Directory -Filter "pop_*" | ForEach-Object {
        $st2 = Join-Path $_.FullName "state.txt"
        if (Test-Path $st2) {
            $c = [double][regex]::Match((Get-Content $st2 -Raw), '(?m)^cash=([\d.\-]+)').Groups[1].Value
            if ($c -le 1.0) { $broke++ }
        }
    }

    Write-Host ("{0,-5} {1,9} {2,7} {3,9} {4,13:N0} {5,11:N0} {6,11} {7,8}" -f `
        $i, $prod, $fill, $st.Ratio, $wages, $cc.Total, $pc.Total, $un.Total, "$broke/24", $consumed, $breaks)

    $prevPop = $pc.Total
}
Write-Host ("-" * 100)
Write-Host "broke = households at <=1 cash (cannot bid). Rising broke% means the"
Write-Host "market is consuming its only buyer base with no income to replace it."
