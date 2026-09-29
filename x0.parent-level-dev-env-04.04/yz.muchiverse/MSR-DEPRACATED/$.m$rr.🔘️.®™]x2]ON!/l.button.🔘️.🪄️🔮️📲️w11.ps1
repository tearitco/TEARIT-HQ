# l.button.<emoji>.w11.ps1 - Windows orchestrator, PowerShell twin of l.button.<emoji>.g11.c
#
# WHY THIS IS POWERSHELL AND NOT A PORTED .c
#   l.button...g11.c uses pthread_tryjoin_np and pthread_timedjoin_np.
#   Those are GNU extensions that mingw-w64 does not provide (it ships
#   winpthreads, which has neither), so the C file cannot be compiled for
#   Windows as-is. The logic itself is small - read a list, run the
#   commands, watch one "killer" command, shut down when it exits - so it
#   is reimplemented here in ~80 lines instead of rewriting the join
#   machinery in C.
#
# BEHAVIOUR KEPT IDENTICAL TO THE C VERSION
#   - locations.txt lives next to this script, and is read BEFORE the
#     chdir. The C version does the same, then chdir("../") so every
#     command in the list is relative to the PROJECT root, not this dir.
#   - a line starting with '^' marks a "killer" program: when it exits,
#     the whole orchestrator shuts down.
#   - a line's SECOND token being '&' means "detached": discard output.
#     Otherwise the output is both shown and appended to gl_cli_out.txt.
#   - gl_cli_out.txt is truncated at startup.
#   - the `stdbuf -oL ` prefix the C version prepends is dropped: it is a
#     GNU coreutils line-buffering hack with no Windows equivalent, and
#     these are line-oriented console programs, so buffering is not the
#     problem it was guarding against.
#   - MAX_THREADS was 10. Kept.
#
# WHY EACH CHILD GETS ITS OWN LOG FILE
#   The C version runs each command through `2>&1 | tee -a gl_cli_out.txt`,
#   i.e. every child has its own pipe and tee does the appending. An
#   earlier version of this script pointed every child's stdout at ONE
#   shared file and then had the watch loop read that file and truncate it
#   between ticks. That is not equivalent: two children hold independent
#   write handles on the same file and interleave at their own offsets,
#   and the loop's truncate races them for the same bytes. So each child
#   now owns a private log, and the loop tails each one by byte offset -
#   which is what tee was doing, just done safely.

$ErrorActionPreference = "Continue"
$SCRIPT_DIR = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location -LiteralPath $SCRIPT_DIR

$MAX_THREADS = 10
$LOCATIONS = Join-Path $SCRIPT_DIR "locations.txt"
$OUT_LOG = "gl_cli_out.txt"

if (-not (Test-Path -LiteralPath $LOCATIONS)) {
    Write-Error "locations.txt not found next to this script."
    exit 1
}

# chdir("../") so every command in the list is relative to the PROJECT
# root, not this directory. The C version reads locations.txt first and
# then chdirs, and opens gl_cli_out.txt afterwards - so the log belongs in
# the project root too, which is why it is truncated there and not here.
$ProjectRoot = Split-Path -Parent $SCRIPT_DIR
Set-Location -LiteralPath $ProjectRoot
$logPath = Join-Path $ProjectRoot $OUT_LOG

# Truncate the log at startup, same as the C fopen("w").
Set-Content -LiteralPath $logPath -Value "" -NoNewline

$entries = @()
foreach ($raw in (Get-Content -LiteralPath $LOCATIONS)) {
    $line = $raw.Trim()
    if ($line.Length -eq 0 -or $line.StartsWith("#")) { continue }

    $isKiller = $false
    if ($line.StartsWith("^")) {
        $isKiller = $true
        $line = $line.Substring(1).Trim()
    }
    if ($line.Length -eq 0) { continue }

    # Second token '&' = detached (discard output), matching the C parser.
    $tokens = $line -split '\s+', 2
    $command = $tokens[0]
    $second = if ($tokens.Count -gt 1) { $tokens[1].Trim() } else { "" }
    $detached = ($second -eq "&")

    $entries += [pscustomobject]@{
        Command  = $command
        Detached = $detached
        Killer   = $isKiller
        Proc     = $null
        OutLog   = $null
        ErrLog   = $null
        OutOff   = [long]0
        ErrOff   = [long]0
    }
    if ($entries.Count -ge $MAX_THREADS) { break }
}

if ($entries.Count -eq 0) {
    Write-Host "No valid commands found in locations.txt"
    exit 1
}

# A bare command like "+x/main.+x" is not directly executable by
# Start-Process, and the +x binaries have no .exe suffix. Resolve each to
# a real runnable file, or return $null so the caller can report it.
#
# The path is anchored to $ProjectRoot rather than used as-is. That is not
# defensive styling: PowerShell's Set-Location does NOT move the process
# CWD ([System.IO.Directory]::GetCurrentDirectory() keeps pointing at
# wherever the shell started), and Test-Path resolves a relative path
# against that stale CWD. So Test-Path "+x/main.+x" returned False for a
# file that plainly exists. It went unnoticed because this function used to
# fall through and hand the raw string to Start-Process, which resolves
# against PowerShell's location and so happened to work - the lookup was
# dead code that only became load-bearing when a real error check was added
# on top of it.
function Resolve-Command([string]$cmd) {
    $abs = $cmd
    if (-not [System.IO.Path]::IsPathRooted($cmd)) {
        $abs = Join-Path $ProjectRoot $cmd
    }
    if (Test-Path -LiteralPath $abs) { return (Get-Item -LiteralPath $abs).FullName }
    $withExe = "$abs.exe"
    if (Test-Path -LiteralPath $withExe) { return (Get-Item -LiteralPath $withExe).FullName }
    return $null
}

# Returns everything appended to $Path after byte $Offset, plus the new
# offset. Opened with FileShare.ReadWrite so the child keeps writing to it
# while it is being read.
function Read-New([string]$Path, [long]$Offset) {
    $res = [pscustomobject]@{ Text = ""; Offset = $Offset }
    if (-not (Test-Path -LiteralPath $Path)) { return $res }
    $fs = [System.IO.File]::Open($Path, [System.IO.FileMode]::Open,
                                 [System.IO.FileAccess]::Read,
                                 [System.IO.FileShare]::ReadWrite)
    try {
        $len = $fs.Length
        if ($len -le $Offset) { return $res }
        [void]$fs.Seek($Offset, [System.IO.SeekOrigin]::Begin)
        $buf = New-Object byte[] ($len - $Offset)
        $read = $fs.Read($buf, 0, $buf.Length)
        if ($read -gt 0) {
            $res.Text = [System.Text.Encoding]::UTF8.GetString($buf, 0, $read)
            $res.Offset = $Offset + $read
        }
    } finally { $fs.Close() }
    return $res
}

# Per-run token so two orchestrators (or a leftover from a crashed run)
# never share a temp log.
$runToken = "$PID-" + (Get-Random -Maximum 100000)
$tempLogs = New-Object System.Collections.ArrayList
$missing = 0

for ($i = 0; $i -lt $entries.Count; $i++) {
    $e = $entries[$i]
    $exe = Resolve-Command $e.Command

    # A command that cannot be resolved is fatal, and is reported BEFORE
    # anything is started. Previously this fell through to Start-Process,
    # which returned no process object, and the watch loop then dereferenced
    # a null Proc forever - an orchestrator that hangs with no output
    # instead of saying "you have not built yet". The usual cause is
    # running the launcher before xsh.compile-all.+x.ps1 has populated +x/.
    if (-not $exe) {
        Write-Error ("Cannot run '$($e.Command)': not found. Build first with xsh.compile-all.+x.ps1" -f $e.Command)
        $missing++
        continue
    }

    # Indexed, NOT named after the command: a command is "+x/main.+x" and
    # its "/" would be read as a directory separator, so the temp path
    # pointed at a directory that does not exist.
    $e.OutLog = Join-Path $env:TEMP ("mars-orch-${runToken}-$i.out")
    $e.ErrLog = Join-Path $env:TEMP ("mars-orch-${runToken}-$i.err")
    $null = $tempLogs.Add($e.OutLog)
    $null = $tempLogs.Add($e.ErrLog)
    Set-Content -LiteralPath $e.OutLog -Value "" -NoNewline
    Set-Content -LiteralPath $e.ErrLog -Value "" -NoNewline

    Write-Host ("  start{0} {1}" -f $(if ($e.Killer) { " [killer]" } else { "" }), $e.Command)
    $e.Proc = Start-Process -FilePath $exe -WorkingDirectory $ProjectRoot `
        -NoNewWindow -PassThru `
        -RedirectStandardOutput $e.OutLog -RedirectStandardError $e.ErrLog
}

# Anything that could not be started is never watched: the loop below
# requires a live process object for every entry.
$entries = @($entries | Where-Object { $_.Proc -ne $null })
if ($missing -gt 0) {
    foreach ($t in $tempLogs) { Remove-Item -LiteralPath $t -Force -EA SilentlyContinue }
    Write-Error "$missing command(s) in locations.txt could not be started; aborting."
    exit 1
}

# Tee: forward each command's output to the console AND to gl_cli_out.txt
# as it appears, which is what `2>&1 | tee -a` did.
$seen = @{}

while ($true) {
    $allExited = $true
    $killerExited = $false

    foreach ($e in $entries) {
        $id = $e.Proc.Id

        if ($e.Proc.HasExited) {
            if (-not $seen.ContainsKey($id)) {
                $seen[$id] = $true
                $tag = if ($e.Killer) { "KILLER" } else { "cmd" }
                Write-Host "[$tag] '$($e.Command)' exited with $($e.Proc.ExitCode) (pid $id)"
                if ($e.Killer) { $killerExited = $true }
            }
        } else {
            $allExited = $false
        }

        # Detached output is discarded, matching the C version, so it is
        # never tailed - but the offset is still advanced so the file does
        # not grow without bound over a long session.
        $r = Read-New $e.OutLog $e.OutOff
        $e.OutOff = $r.Offset
        $er = Read-New $e.ErrLog $e.ErrOff
        $e.ErrOff = $er.Offset

        if (-not $e.Detached) {
            $chunk = $r.Text + $er.Text
            if ($chunk.Trim().Length -gt 0) {
                Write-Host $chunk.TrimEnd()
                [System.IO.File]::AppendAllText($logPath, $chunk)
            }
        }
    }

    if ($killerExited) {
        Write-Host "Killer thread exited, shutting down orchestrator"
        break
    }
    if ($allExited) { break }
    Start-Sleep -Milliseconds 200
}

# Final drain so nothing written just before exit is lost.
foreach ($e in $entries) {
    if ($e.Detached) { continue }
    $r = Read-New $e.OutLog $e.OutOff
    $er = Read-New $e.ErrLog $e.ErrOff
    $chunk = $r.Text + $er.Text
    if ($chunk.Trim().Length -gt 0) {
        Write-Host $chunk.TrimEnd()
        [System.IO.File]::AppendAllText($logPath, $chunk)
    }
}

# Clean up anything still running.
foreach ($e in $entries) {
    if ($e.Proc -and -not $e.Proc.HasExited) {
        try { Stop-Process -Id $e.Proc.Id -Force -ErrorAction Stop } catch { }
    }
}

foreach ($t in $tempLogs) { Remove-Item -LiteralPath $t -Force -EA SilentlyContinue }
exit 0
