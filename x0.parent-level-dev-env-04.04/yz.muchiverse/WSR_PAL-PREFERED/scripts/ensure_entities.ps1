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
