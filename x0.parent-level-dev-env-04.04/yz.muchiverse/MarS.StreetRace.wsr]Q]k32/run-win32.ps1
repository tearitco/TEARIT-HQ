# run-win32.ps1 - build (if needed) and play MarS StreetRace on Windows.
#
# ONE COMMAND. From this directory:
#
#     powershell -ExecutionPolicy Bypass -File .\run-win32.ps1
#
# Optional:
#     .\run-win32.ps1 -Force      rebuild even if the binaries look current
#     .\run-win32.ps1 -NoBuild    skip straight to playing
#
# WHY THIS FILE EXISTS
#   The Windows port is three pieces that have to line up: the compatibility
#   headers in win32-compat/, the build driver xsh.compile-all.+x.ps1, and
#   the orchestrator launched by _.start.<emoji>.ps1. Requiring all of that
#   to be typed by hand is a good way to never run it.
#
# WHY IT REBUILDS BY ITSELF
#   gcc is not incremental here - every one of the 31 sources is a separate
#   compile+link - so the build is compared against the sources by timestamp
#   and skipped when nothing changed. That keeps the common case to a few
#   seconds instead of a couple of minutes.
#
# NOTE ON THE SWITCHES: this file USED to reference $NoBuild and $Force
#   without ever declaring a param() block. PowerShell therefore treated
#   them as empty variables and the switches did nothing - `-NoBuild` still
#   triggered a full rebuild. The param block above is what makes the
#   documented behaviour real; do not remove it.
param(
    [switch]$Force,     # rebuild even if the binaries look current
    [switch]$NoBuild    # skip straight to playing
)

$ErrorActionPreference = "Continue"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location -LiteralPath $here

$buildScript = Join-Path $here "xsh.compile-all.+x.ps1"
$outDir      = Join-Path $here "+x"

# The launcher name carries an emoji, so it is found by shape rather than
# typed. Matching "_.start.*.ps1" cannot hit the Linux .sh or the macOS
# binary, which are the only other _.start.* files here.
$launcher = Get-ChildItem -LiteralPath $here -File -Filter "_.start.*.ps1" -EA SilentlyContinue |
    Select-Object -First 1

if (-not (Test-Path -LiteralPath $buildScript)) {
    Write-Error "xsh.compile-all.+x.ps1 is missing next to this script."
    exit 1
}
if (-not $launcher) {
    Write-Error "No _.start.*.ps1 launcher found next to this script."
    exit 1
}

# ---- decide whether a build is needed -------------------------------------
# The inputs to every artifact are the .c file AND the compatibility headers
# force-included into it, not just the source. Checking only the .c mtime is
# how editing win32-compat/mars_system.h could leave stale binaries in place
# behind a cheerful "Binaries are current" - which is exactly what happened,
# and why the blank-screen fix needed a manual -Force to take effect.
$stale = New-Object System.Collections.ArrayList
$sources = @(Get-ChildItem -LiteralPath $here -File -Filter "*.c" | Sort-Object Name)

if ($sources.Count -eq 0) {
    Write-Error "No .c sources found in $here - is this the right directory?"
    exit 1
}

# Headers and the build driver are inputs to every artifact, not just to the
# sources that happen to include them.
$inputs = @($sources)
$inputs += @(Get-ChildItem -LiteralPath (Join-Path $here "win32-compat") -Recurse -File -Filter "*.h" -EA SilentlyContinue)
$inputs += @(Get-Item -LiteralPath $buildScript -EA SilentlyContinue)

if (-not (Test-Path -LiteralPath $outDir)) {
    $null = $stale.Add("+x/ does not exist")
} else {
    $artifacts = @(Get-ChildItem -LiteralPath $outDir -File -Filter "*.+x" -EA SilentlyContinue)
    if ($artifacts.Count -ne $sources.Count) {
        $null = $stale.Add("expected $($sources.Count) artifacts, found $($artifacts.Count)")
    } else {
        # Every artifact depends on every header, so if ANY input is newer
        # than the OLDEST binary then at least one binary is out of date.
        # Newest input vs oldest artifact is therefore the correct test -
        # a per-file comparison would be both more complex and wrong.
        $oldestArtifact = ($artifacts | Sort-Object LastWriteTime | Select-Object -First 1).LastWriteTime
        $newestInput = ($inputs | Sort-Object LastWriteTime -Descending | Select-Object -First 1)
        if ($newestInput -and $newestInput.LastWriteTime -gt $oldestArtifact) {
            $null = $stale.Add("$($newestInput.Name) is newer than the oldest binary")
        }
    }
}

$needBuild = $stale.Count -gt 0

if ($NoBuild) {
    if ($needBuild) {
        Write-Warning "-NoBuild given but $($stale.Count) source(s) have no current binary; the game may fail to start."
    }
} elseif ($needBuild -or $Force) {
    $why = if ($Force -and -not $needBuild) { "(-Force)" } else { "$($stale.Count) source(s) out of date" }
    Write-Host "Building $($sources.Count) sources, $why ..." -ForegroundColor Cyan
    & powershell -ExecutionPolicy Bypass -File $buildScript
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Build failed (exit $LASTEXITCODE). Nothing will be started."
        exit 1
    }
} else {
    Write-Host "Binaries are current, skipping build. (-Force to rebuild)" -ForegroundColor DarkGray
}

# ---- play -----------------------------------------------------------------
# stdin must be a real console. If it is redirected the game reads EOF on
# its first prompt and exits immediately, which looks like a crash but is
# just the input model - the key-wait poll in win32-compat/sys/select.h only
# has a live console to wait on.
if ([Console]::IsInputRedirected) {
    Write-Warning "stdin is redirected, so the game will exit at its first prompt. Run this in a normal console window to actually play."
}

Write-Host ""
& $launcher.FullName
exit $LASTEXITCODE
