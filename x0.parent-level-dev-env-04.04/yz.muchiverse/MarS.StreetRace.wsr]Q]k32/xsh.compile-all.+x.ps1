# xsh.compile-all.+x.ps1 - Windows twin of xsh.compile-all.+x.sh
#
# WHAT CHANGED FROM THE LINUX SCRIPT, AND WHY
#   The .sh links this flag set for every source file:
#     -pthread -lm -lssl -lcrypto -lGL -lGLU -lglut -lGLEW -lfreetype
#     -lavcodec -lavformat -lavutil -lswscale -lX11 -lassimp -lOpenCL
#   None of that is real. Across all 47 .c files in this project the only
#   system headers used are stdio/stdlib/string/time/ctype/unistd/signal/
#   stdbool/sys/stat/termios/dirent/sys/select/sys/wait/errno/math/
#   pthread/Availability. There is no X11, no GL, no GLUT, no GLEW, no
#   ffmpeg, no assimp, no OpenCL, no freetype, no libpng, no OpenSSL
#   anywhere. The link line is copy-paste boilerplate that happened to
#   link cleanly on a machine that had all of it installed.
#
#   So this script links nothing. The only library actually required is
#   ws2_32, because win32-compat/sys/select.h includes <winsock2.h> for
#   its fd_set layout. Drop that shim and the dependency goes with it.
#
# PLATFORM GAPS BRIDGED BY -I win32-compat
#   termios.h      9 files  - raw console mode over Get/SetConsoleMode
#   sys/select.h   8 files  - non-blocking stdin poll over WaitForSingleObject
#   sys/stat.h    10 files  - POSIX 2-arg mkdir -> CreateDirectoryA
#   dirent.h       2 files  - d_type/DT_DIR, which mingw's lacks entirely
#   The shims are passed via -I on Windows only. No .c file is edited, so
#   the Linux build is bit-for-bit unaffected.
#
# Usage: powershell -ExecutionPolicy Bypass -File .\xsh.compile-all.+x.ps1

$ErrorActionPreference = "Continue"
$SCRIPT_DIR = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location -LiteralPath $SCRIPT_DIR

$OUT = Join-Path $SCRIPT_DIR "+x"
$SHIM = Join-Path $SCRIPT_DIR "win32-compat"

$MSYS_BIN = "C:\msys64\mingw64\bin"
if (Test-Path -LiteralPath $MSYS_BIN) {
    if ($env:Path -notlike "*$MSYS_BIN*") { $env:Path = "$MSYS_BIN;$env:Path" }
}
if (-not (Get-Command gcc -ErrorAction SilentlyContinue)) {
    Write-Error "gcc not found. Install MSYS2 MinGW64 (mingw-w64-x86_64-gcc)."
    exit 1
}

if (-not (Test-Path -LiteralPath $SHIM)) {
    Write-Error "win32-compat/ shim dir missing next to this script."
    exit 1
}

# gcc does not add .exe when the output name is an odd extension, and the
# house names here are full of ] and emoji. The orchestrator that later
# LAUNCHES these artifacts therefore needs Start-Process, because
# PowerShell will not run an extensionless PE through a pipeline, and
# cmd /c cannot be used at all: cmd treats '&' as a command separator and
# this project's sibling paths are full of it. Compiling does not have
# that constraint, and uses the plain call operator.
if (-not (Test-Path -LiteralPath $OUT)) { New-Item -ItemType Directory -Force -Path $OUT | Out-Null }

$sources = @(Get-ChildItem -LiteralPath $SCRIPT_DIR -File -Filter "*.c" | Sort-Object Name)
if ($sources.Count -eq 0) { Write-Error "No .c files found."; exit 1 }

# Every temp name carries a per-run token. An earlier version of this
# script named the stderr file mars-gcc-err-<index>.txt and reached gcc via
# Start-Process -RedirectStandardError. That intermittently reported FAIL
# for game.c and game_setup.c on a run where an earlier gcc still held the
# redirect target: the sources compile cleanly, verified by hand and by
# two subsequent full runs. A reused temp path plus Start-Process's
# redirect handling is the cause, so both are gone - the token makes every
# path unique to this run, and the call operator has no redirect race.
$runToken = "$PID-" + (Get-Random -Maximum 100000)
$tempFiles = New-Object System.Collections.ArrayList

Write-Host "building $($sources.Count) sources -> +x\"
$failed = New-Object System.Collections.ArrayList
$renamed = 0
$tmpSeq = 0
$retried = 0

for ($i = 0; $i -lt $sources.Count; $i++) {
    $src = $sources[$i]
    $finalExe = Join-Path $OUT ($src.BaseName + ".+x")
    $isAscii = -not ($src.BaseName -match '[^\x00-\x7F]')

    # ld.exe cannot emit a non-ASCII output filename on this toolchain: it
    # receives the name through the ANSI codepage, so the emoji in
    # 0.platform.orb.args]<emoji>... arrives as "??????" and the open fails
    # with "Invalid argument". `chcp 65001` does NOT help - the child
    # still goes through ANSI. So such sources are linked to an ASCII temp
    # name and then moved to the house name, which PowerShell does with
    # real Unicode. The finished binary keeps its proper name.
    $linkTo = $finalExe
    $needsMove = $false
    if (-not $isAscii) {
        $linkTo = Join-Path $OUT ("wb_tmp_${runToken}_${tmpSeq}.+x")
        $tmpSeq++
        $needsMove = $true
    }

    # -include mars_system.h rewrites the system() calls for cmd.exe and
    # restores POSIX return-status semantics. Force-included rather than
    # #included by each .c so the Linux build is byte-identical and no
    # source is edited.
    $errFile = Join-Path $env:TEMP ("mars-gcc-err-${runToken}-$i.txt")
    $null = $tempFiles.Add($errFile)
    if (-not $isAscii) { $null = $tempFiles.Add($linkTo) }

    # The call operator, not Start-Process: gcc is an ordinary console exe
    # and PowerShell runs it fine. Start-Process is only needed to LAUNCH
    # the built .+x artifacts later, because PowerShell will not execute an
    # extensionless PE from a pipeline and cmd /c cannot be used at all
    # (cmd treats '&' as a command separator and these sibling paths are
    # full of it).
    # Retried on ANY nonzero exit, not on a matched error string.
    #
    # An earlier version retried only when the text contained "cannot execute"
    # or "ld returned", and that was too clever: the failures do not present a
    # stable signature. Observed across consecutive passes, all transient, all
    # on files that compile cleanly in isolation to the same output path:
    #     gcc.exe: fatal error: cannot execute '.../collect2.exe'
    #     collect2.exe: error: ld returned 15 exit status
    #     gcc exit 15, with empty stderr
    # alongside the host-side "Win32 internal error 'No process is on the
    # other end of the pipe' 0xE9". Spawning 31 compilers back to back from
    # one PowerShell process on a OneDrive-synced path exhausts something the
    # session does not get back.
    #
    # Matching on text is unnecessary because a GENUINE compile error is
    # deterministic: it fails all three attempts and is then reported with its
    # real message. Only the transient ones pass on a retry.
    $attempt = 0
    do {
        & gcc -include (Join-Path $SHIM "mars_system.h") $src.Name -I $SHIM -o $linkTo -lws2_32 2> $errFile
        $rc = $LASTEXITCODE
        $attempt++
        if ($rc -ne 0 -and $attempt -lt 3) {
            $retried++
            Start-Sleep -Milliseconds 400
        }
    } while ($rc -ne 0 -and $attempt -lt 3)

    if ($rc -eq 0 -and $needsMove) {
        try {
            if (Test-Path -LiteralPath $finalExe) { Remove-Item -LiteralPath $finalExe -Force }
            Move-Item -LiteralPath $linkTo -Destination $finalExe -Force
            $renamed++
        } catch {
            Write-Host "  FAIL $($src.Name) (linked, but rename to the house name failed)" -ForegroundColor Red
            Write-Host "       $($_.Exception.Message)" -ForegroundColor Red
            $null = $failed.Add($src.Name)
            continue
        }
    }

    if ($rc -eq 0) {
        # Exit status alone is not enough to claim success: confirm the
        # artifact actually landed under its house name and is non-empty.
        if (-not (Test-Path -LiteralPath $finalExe) -or
            (Get-Item -LiteralPath $finalExe -EA SilentlyContinue).Length -eq 0) {
            Write-Host "  FAIL $($src.Name) (gcc reported 0 but no artifact at the house name)" -ForegroundColor Red
            $null = $failed.Add($src.Name)
            continue
        }
        Write-Host "  OK   $($src.Name)"
    } else {
        $err = (Get-Content -LiteralPath $errFile -Raw -EA SilentlyContinue)
        $first = ($err -split "`n" | Select-String -Pattern 'error:' | Select-Object -First 1)
        Write-Host "  FAIL $($src.Name) (gcc exit $rc)" -ForegroundColor Red
        if ($first) { Write-Host "       $($first.Line.Trim())" -ForegroundColor Red }
        elseif ($err) { Write-Host "       $(($err -split "`n" | Select-Object -First 1).Trim())" -ForegroundColor Red }
        $null = $failed.Add($src.Name)
    }
}

foreach ($t in $tempFiles) { Remove-Item -LiteralPath $t -Force -EA SilentlyContinue }

Write-Host ""
if ($failed.Count -gt 0) {
    Write-Host "FAILED: $($failed.Count) of $($sources.Count)" -ForegroundColor Red
    foreach ($f in $failed) { Write-Host "  - $f" -ForegroundColor Red }
    exit 1
}
if ($renamed -gt 0) {
    Write-Host "($renamed non-ASCII artifact name(s) linked via ASCII temp and moved into place)"
}
if ($retried -gt 0) {
    Write-Host "($retried link step(s) hit a transient host failure and succeeded on retry)"
}
Write-Host "Compilation complete. ($($sources.Count)/$($sources.Count))"
exit 0
