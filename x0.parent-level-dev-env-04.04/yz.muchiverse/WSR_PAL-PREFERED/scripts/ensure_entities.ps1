# ensure_entities.ps1 - Windows parity with ensure_entities.sh
# IDEMPOTENT: only creates a piece if its state.txt does not already exist.

$ErrorActionPreference = "Continue"
$SCRIPT_DIR = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
# The entity source trees are SIBLINGS of this project, not children of it.
# SCRIPT_DIR is WSR_PAL-PREFERED, so the data lives one level up in
# ../MSR-DEPRACATED. The old path pointed INSIDE this project at
# 'MarS.StreetRace.wsr]Q]k32\corporations\generated', a directory that was
# renamed away on 2026-09-26 (it used to carry a $ metacharacter) and never
# existed here afterwards. The failure was silent: the script created zero
# corporations, printed "0 created, 0 already existed", and the world came up
# empty, so no playthrough was possible and nothing reported an error. The
# sibling MarS.StreetRace.wsr]Q]k32 tree still exists but its generated/
# subtrees are empty; the 50 corporations and 7 governments live in
# MSR-DEPRACATED.
$CORP_SRC = Join-Path (Split-Path -Parent $SCRIPT_DIR) 'MSR-DEPRACATED\corporations\generated'
$GOV_SRC  = Join-Path (Split-Path -Parent $SCRIPT_DIR) 'MSR-DEPRACATED\governments\generated'
$DEST     = Join-Path $SCRIPT_DIR "projects\wsr-pal\pieces"

# Fail loudly rather than silently producing an empty world again.
foreach ($src in @($CORP_SRC, $GOV_SRC)) {
    if (-not (Test-Path $src)) {
        Write-Error "entity source missing: $src - the world would come up EMPTY. Fix the path above."
        exit 1
    }
}

function Get-FirstDecimal([string]$text, [string]$label) {
    if (-not $text) { return $null }
    $lines = $text -split "`r?`n" | Where-Object { $_ -like "*${label}*" }
    foreach ($line in $lines) {
        if ($line -match '(-?\d+\.\d+)') { return $Matches[1] }
    }
    return $null
}

function Get-FirstInt([string]$text, [string]$label) {
    if (-not $text) { return $null }
    $lines = $text -split "`r?`n" | Where-Object { $_ -like "*${label}*" }
    foreach ($line in $lines) {
        if ($line -match '(\d+)') { return $Matches[1] }
    }
    return $null
}

$corp_created = 0
$corp_skipped = 0
if (Test-Path $CORP_SRC) {
    Get-ChildItem $CORP_SRC -Directory | ForEach-Object {
        $ticker = $_.Name
        $profile = Join-Path $_.FullName "$ticker.txt"
        $weights = Join-Path $_.FullName "weights.txt"
        if (-not (Test-Path $profile)) { return }

        $piece_dir = Join-Path $DEST "corp_$ticker"
        $state = Join-Path $piece_dir "state.txt"
        if (Test-Path $state) { $script:corp_skipped++; return }

        $ptext = Get-Content $profile -Raw
        $cash = Get-FirstDecimal $ptext "Free Cash and Equivalents:"
        $stock_price = Get-FirstDecimal $ptext "Current Stock Price:"
        $book_value = Get-FirstDecimal $ptext "Equity (Net Worth):"
        $shares_outstanding = Get-FirstDecimal $ptext "Shares of Stock Outstanding:"
        $market_cap = Get-FirstDecimal $ptext "Total Stock Capitalization:"
        $debt_to_equity = Get-FirstDecimal $ptext "Debt to Equity Ratio:"
        $risk_bias = 50
        if (Test-Path $weights) {
            $w = Get-Content $weights -Raw
            $rb = Get-FirstInt $w "risk"
            if ($rb) { $risk_bias = $rb }
        }
        if (-not $cash -or -not $stock_price -or -not $book_value -or
            -not $shares_outstanding -or -not $market_cap -or -not $debt_to_equity) {
            return
        }

        New-Item -ItemType Directory -Force -Path $piece_dir | Out-Null
        @"
current_state=0
decision_mode=1
cash=$cash
stock_price=$stock_price
book_value=$book_value
shares_outstanding=$shares_outstanding
market_cap=$market_cap
debt_to_equity=$debt_to_equity
risk_bias=$risk_bias
shares_held=0
pending_action=
last_action=
human_decision=
owned_by=
"@ | Set-Content -Path $state -NoNewline
        $script:corp_created++
    }
}

$gov_created = 0
$gov_skipped = 0
if (Test-Path $GOV_SRC) {
    Get-ChildItem $GOV_SRC -Directory | ForEach-Object {
        $name = $_.Name
        $profile = Join-Path $_.FullName "financial_profile.txt"
        if (-not (Test-Path $profile)) { return }

        $safe_name = ($name -replace ' ', '_')
        $piece_dir = Join-Path $DEST "gov_$safe_name"
        $state = Join-Path $piece_dir "state.txt"
        if (Test-Path $state) { $script:gov_skipped++; return }

        $ptext = Get-Content $profile -Raw
        $cash = Get-FirstDecimal $ptext "Cash and Cash Equivalents:"
        $revenue = Get-FirstDecimal $ptext "Total Revenue:"
        $spending = Get-FirstDecimal $ptext "Net Cost of Operations:"
        $net_operating = Get-FirstDecimal $ptext "Net Operating (Cost)/Revenue:"
        $gdp = Get-FirstDecimal $ptext "GDP (Nominal"
        $debt_to_gdp = Get-FirstDecimal $ptext "Debt-to-GDP Ratio:"
        if (-not $cash -or -not $revenue -or -not $spending -or
            -not $net_operating -or -not $gdp -or -not $debt_to_gdp) {
            return
        }

        New-Item -ItemType Directory -Force -Path $piece_dir | Out-Null
        @"
current_state=0
decision_mode=1
cash=$cash
revenue=$revenue
spending=$spending
net_operating=$net_operating
gdp=$gdp
debt_to_gdp=$debt_to_gdp
tax_rate_adj=0.0
pending_action=
last_action=
human_decision=
"@ | Set-Content -Path $state -NoNewline
        $script:gov_created++
    }
}

Write-Host "corporations: $corp_created created, $corp_skipped already existed"
Write-Host "governments:  $gov_created created, $gov_skipped already existed"

# ---------------------------------------------------------------------------
# HOUSEHOLDS
#
# The household layer did not exist in a live world at all. pop_tick_idle.c,
# pop_update.c and market_quote.c all DISCOVER pop_* pieces, and there was one
# template (pop_downtown) that nothing ever instantiated - so every one of those
# ops had nothing to act on. It is the missing half of the economy: a goods
# market needs buyers with cash, wages need recipients, and the population
# feedback loop needs pieces to grow.
#
# A household is a pop_* piece representing a district (total_population), not
# an individual, so it buys in bulk and holds aggregate savings.
#
# cash is seeded, not derived: the legacy specifies no starting household
# wealth, exactly as it specifies none for share ownership. This default is a
# NEW rule and is meant to be replaced by seed_cash_household from the scenario
# file. It is a parameter so the complexity tiers can drive it later without
# editing this script.
#
# Idempotent, like the rest: a household whose state.txt exists is left alone.
# ---------------------------------------------------------------------------
$pop_src  = Join-Path $SCRIPT_DIR "projects\wsr-pal\pieces_template\pop_downtown\state.txt"
$pop_count   = 24
$pop_cash    = 5000
if ($env:WSR_PAL_HOUSEHOLDS)    { $pop_count = [int]$env:WSR_PAL_HOUSEHOLDS }
if ($env:WSR_PAL_HOUSEHOLD_CASH){ $pop_cash  = [double]$env:WSR_PAL_HOUSEHOLD_CASH }

if (-not (Test-Path $pop_src)) {
    Write-Error "household template missing: $pop_src"
    exit 1
}

$pop_created = 0; $pop_skipped = 0
for ($i = 1; $i -le $pop_count; $i++) {
    $piece_dir = Join-Path $DEST ("pop_household{0:d2}" -f $i)
    $state     = Join-Path $piece_dir "state.txt"
    if (Test-Path $state) { $pop_skipped++; continue }

    New-Item -ItemType Directory -Force -Path $piece_dir | Out-Null
    # Start from the template so every household carries the same field set the
    # pop ops already read (food_supply, food_demand, unemployment_rate,
    # avg_wage, ...), then override the one seeded value.
    #
    # The source is the template's state.txt, NOT the directory: Get-Content on
    # a directory throws, and because ErrorActionPreference is Continue that
    # throw was swallowed - the first run happily reported "24 created" having
    # written 24 EMPTY state files. Read the file, and verify it parsed.
    $tmpl = @(Get-Content -Path $pop_src -ErrorAction Stop)
    if ($tmpl.Count -lt 3) {
        Write-Error "household template looks wrong ($($tmpl.Count) lines): $pop_src"
        exit 1
    }
    # -NoNewline is WRONG here. Piping an ARRAY to Set-Content -NoNewline
    # concatenates every element with no separator, which produced a single
    # unparseable line - "current_state=0decision_mode=1cash=5000..." - while
    # still reporting success. The corp and government writers above get away
    # with -NoNewline because they pass ONE here-string containing real
    # newlines; an array needs a real newline between elements.
    $tmpl -replace '^cash=.*$', "cash=$pop_cash" | Set-Content -Path $state
    $pop_created++
}
Write-Host "households:   $pop_created created, $pop_skipped already existed"
