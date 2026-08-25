# compile-runner.ps1 - house-wide compile runner for Windows (PowerShell).
# Finds every build.ps1 / scripts/build.ps1 in the house, runs each from
# its own project dir, and drops one consolidated report.
#
# This is the Windows twin of compile-runner.sh. It does NOT invoke the
# bash build.sh scripts — those require WSL/Git Bash. This runner only
# covers projects that have a native PowerShell build script.
#
# USAGE:
#   .\compile-runner.ps1                  # run every .ps1 build found
#   .\compile-runner.ps1 <substring>      # only scripts whose path contains <substring>
#   .\compile-runner.ps1 --list           # just list what would run
#
# Per-script timeout defaults to 300s (override: $env:BUILD_TIMEOUT=600).

param(
    [string]$Filter = ""
)

$ErrorActionPreference = "Continue"
$SCRIPT_DIR = Split-Path -Parent $MyInvocation.MyCommand.Path
$HOUSE_DIR = Split-Path $SCRIPT_DIR -Parent
$TIMEOUT_SECS = if ($env:BUILD_TIMEOUT) { [int]$env:BUILD_TIMEOUT } else { 300 }

$MODE = "run"
if ($args -contains "--list") {
    $MODE = "list"
    $Filter = ""
} elseif ($args.Count -gt 0) {
    $Filter = $args[0]
}

$REPORT_DIR = Join-Path $HOUSE_DIR "`$.crypts\build-reports\$(Get-Date -Format 'yyyyMMdd-HHmmss')"

if ($MODE -ne "list") {
    New-Item -ItemType Directory -Path $REPORT_DIR -Force | Out-Null
}

Write-Host "=== compile-runner.ps1 - house-wide compile sweep ==="
Write-Host "House: $HOUSE_DIR"
if ($MODE -ne "list") {
    Write-Host "Report dir: $REPORT_DIR"
}
Write-Host ""

$RESULTS_FILE = Join-Path $REPORT_DIR "results.tsv"
if ($MODE -ne "list") {
    "" | Out-File -FilePath $RESULTS_FILE -Encoding utf8
}

$PASS = 0; $FAIL = 0; $TIMEOUT = 0

$buildScripts = Get-ChildItem -Path $HOUSE_DIR -Recurse -Filter "build.ps1" -File -ErrorAction SilentlyContinue |
    Where-Object { $_.FullName -notmatch '\\pieces\\sessions\\|_BACKUP|\.backup|\.pre-symlink-swap' } |
    Sort-Object FullName

$buildScripts += Get-ChildItem -Path $HOUSE_DIR -Recurse -Filter "build.ps1" -File -ErrorAction SilentlyContinue |
    Where-Object { $_.FullName -notmatch '\\pieces\\sessions\\|_BACKUP|\.backup|\.pre-symlink-swap' } |
    Sort-Object FullName

foreach ($bsh in $buildScripts) {
    $rel = $bsh.FullName.Substring($HOUSE_DIR.Length + 1)

    if ($Filter -and $rel -notmatch [regex]::Escape($Filter)) {
        continue
    }

    if ($MODE -eq "list") {
        Write-Host "WOULD BUILD  $rel"
        continue
    }

    $projdir = $bsh.DirectoryName
    Write-Host "--- $rel ---"
    $safeLog = Join-Path $REPORT_DIR (($rel -replace '[\/\\ ]', '__') + ".log")

    $start = Get-Date
    $ec = 0
    $output = ""

    try {
        $psi = New-Object System.Diagnostics.ProcessStartInfo
        $psi.FileName = "powershell.exe"
        $psi.Arguments = "-NoProfile -ExecutionPolicy Bypass -File `"$($bsh.FullName)`""
        $psi.WorkingDirectory = $projdir
        $psi.UseShellExecute = $false
        $psi.RedirectStandardOutput = $true
        $psi.RedirectStandardError = $true
        $psi.CreateNoWindow = $true

        $proc = [System.Diagnostics.Process]::Start($psi)
        $proc.WaitForExit($TIMEOUT_SECS * 1000) | Out-Null

        if (-not $proc.HasExited) {
            $proc.Kill()
            $ec = 124
            $output = "TIMEOUT after ${TIMEOUT_SECS}s"
        } else {
            $ec = $proc.ExitCode
            $stdout = $proc.StandardOutput.ReadToEnd()
            $stderr = $proc.StandardError.ReadToEnd()
            $output = $stdout + $stderr
        }
    } catch {
        $ec = 1
        $output = $_.Exception.Message
    }

    $elapsed = (Get-Date) - $start
    if ($ec -eq 0) {
        $status = "PASS"; $PASS++
    } elseif ($ec -eq 124) {
        $status = "TIMEOUT"; $TIMEOUT++
    } else {
        $status = "FAIL (exit $ec)"; $FAIL++
    }

    Write-Host "  -> $status   (log: $($safeLog.Substring($HOUSE_DIR.Length + 1)))"
    $output | Out-File -FilePath $safeLog -Encoding utf8
    "$status`t$rel`t$($safeLog.Substring($HOUSE_DIR.Length + 1))" | Out-File -FilePath $RESULTS_FILE -Append -Encoding utf8
}

if ($MODE -eq "list") { exit 0 }

# --- write REPORT.md ---
{
    "# House-wide compile report — $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
    ""
    "**Summary: $($PASS+$FAIL+$TIMEOUT) scripts — PASS=$PASS FAIL=$FAIL TIMEOUT=$TIMEOUT**"
    ""
    "| Status | Script | Log |"
    "|---|---|---|"
    Get-Content $RESULTS_FILE -Encoding utf8 | ForEach-Object {
        $parts = $_ -split "`t"
        if ($parts.Count -ge 3) {
            "| $($parts[0]) | $($parts[1]) | $($parts[2]) |"
        }
    }
    ""
    if ($FAIL -gt 0 -or $TIMEOUT -gt 0) {
        "## Failure log tails"
        ""
        Get-Content $RESULTS_FILE -Encoding utf8 | ForEach-Object {
            $parts = $_ -split "`t"
            if ($parts.Count -ge 3 -and $parts[0] -notmatch '^PASS') {
                "### $($parts[1]) — $($parts[0])"
                "```"
                $logPath = Join-Path $HOUSE_DIR $parts[2]
                if (Test-Path $logPath) {
                    Get-Content $logPath -Tail 15 -Encoding utf8
                }
                "```"
                ""
            }
        }
    }
} | Out-File -FilePath (Join-Path $REPORT_DIR "REPORT.md") -Encoding utf8

Write-Host ""
Write-Host "=== DONE: $($PASS+$FAIL+$TIMEOUT) scripts — PASS=$PASS FAIL=$FAIL TIMEOUT=$TIMEOUT ==="
Write-Host "Report: $($REPORT_DIR.Substring($HOUSE_DIR.Length + 1))\REPORT.md"
