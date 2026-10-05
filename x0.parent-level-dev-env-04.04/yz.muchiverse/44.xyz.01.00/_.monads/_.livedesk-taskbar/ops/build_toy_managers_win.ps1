# build_toy_managers_win.ps1 - Windows twins of the per-toy build_*.sh.
# ASCII only.
#
# WHY THIS FILE EXISTS: every toys dropdown row opens a toy by running the
# shared renderer on that toy's own *.xhtpm, and the renderer then starts
# each <module src=".../+x/<name>.+x"> it finds in that template. Those
# managers are ordinary C programs, but nothing on this platform was
# compiling them: their canonical build_*.sh scripts are `sh` scripts, and
# ops/+x/khtpm_core_render.exe started happily with a blank window and no
# backend, which is exactly what the toys dropdown showed before
# khtpm_win_spawn_module() existed at all.
#
# So this builds the eleven managers the toy templates actually reference.
# The binaries land next to where the xhtpm <module> tag already looks,
# because khtpm_win_spawn_module() applies the same *.+x -> *.exe rewrite
# the taskbar's own win_star_alias() applies. Nothing about the tree's
# canonical Linux names changes.
#
# TWO FLAVOURS, and which one a manager needs is not a preference:
#
#   plain       Pure-logic managers that already compile under MinGW with
#               only -I <shared-lib>. These use no POSIX process API.
#
#   win-compat  Managers written against POSIX that MinGW-w64 lacks: 2-arg
#               mkdir(p, mode), <sys/wait.h>, and an explicit kill(). They
#               get the renderer's include set (win-compat/ first, plus the
#               force-included prelude) and link the two shim objects -
#               khtpm_win_compat.c for kill/stat/fclose/rename and
#               khtpm_strip_posix_win.c for the fork/execve link-only
#               stubs. That is the same treatment the renderer gets, for
#               the same reason: the canonical sources are not edited to
#               suit the platform.
#
# FLAGS, each load-bearing:
#   -mwindows              PE subsystem GUI. Without it Windows allocates a
#                          console per manager, and there is no spawn flag
#                          that can prevent it.
#   -Wl,--stack,0x800000   Windows reserves 1 MB of stack by default against
#                          Linux's 8 MB, and these managers hold big
#                          `char paths[N][M]` arrays. Matching the Linux
#                          default is cheaper than rewriting the sources.
#   khtpm_gui_entry_win.c  -mwindows makes the CRT look for WinMain; this
#                          supplies it and forwards to the real main().
#
# Usage:
#   powershell -NoProfile -ExecutionPolicy Bypass \
#     -File build_toy_managers_win.ps1

$ErrorActionPreference = "Continue"
$SCRIPT_DIR = Split-Path -Parent $MyInvocation.MyCommand.Path

$MSYS = "C:\msys64\mingw64\bin"
if (-not (Get-Command gcc -ErrorAction SilentlyContinue)) {
    if (Test-Path -LiteralPath (Join-Path $MSYS "gcc.exe")) {
        $env:Path = "$MSYS;$env:Path"
    } else {
        Write-Error "gcc not found. Install MSYS2 MinGW64 (mingw-w64-x86_64-gcc)."
        exit 1
    }
}

# House discovery by walking UP for #.desktop/ + &.widgits/ - same test as
# build_khtpm_strip_win.ps1, so neither script hardcodes a level count.
$HOUSE = $null
$d = $SCRIPT_DIR
while ($d -and ($d -ne "/")) {
    if ((Test-Path -LiteralPath (Join-Path $d "#.desktop")) -and
        (Test-Path -LiteralPath (Join-Path $d "&.widgits"))) { $HOUSE = $d; break }
    $d = Split-Path -Parent $d
}
if (-not $HOUSE) { Write-Error "house root not found above $SCRIPT_DIR"; exit 1 }

$SHARED  = Join-Path $HOUSE "&.widgits\_shared-lib"
$WCOMPAT = Join-Path $SCRIPT_DIR "win-compat"
$GUI     = Join-Path $SCRIPT_DIR "khtpm_gui_entry_win.c"
$COMPAT  = @((Join-Path $SCRIPT_DIR "khtpm_win_compat.c"),
             (Join-Path $SCRIPT_DIR "khtpm_strip_posix_win.c"))
$INCS    = @("-I", $SCRIPT_DIR, "-I", $WCOMPAT, "-I", $SHARED,
             "-include", (Join-Path $WCOMPAT "khtpm_win_compat_prelude.h"))
$CFLAGS  = @("-std=c11", "-O2", "-w", "-mwindows", "-Wl,--stack,0x800000")

# flavor: "plain" or "wincompat"
$JOBS = @(
    @{ src = "$house\@.apps\csv-hq\ops\csv_hq_manager.c"
       out = "$house\@.apps\csv-hq\ops\+x\csv_hq_manager.exe"; flavor = "plain" }

    @{ src = "$house\@.apps\media-3d-hq\ops\media_3d_hq_manager.c"
       out = "$house\@.apps\media-3d-hq\ops\+x\media_3d_hq_manager.exe"; flavor = "plain" }

    @{ src = "$house\@.apps\media-daw-hq\ops\media_daw_hq_manager.c"
       out = "$house\@.apps\media-daw-hq\ops\+x\media_daw_hq_manager.exe"; flavor = "plain" }

    @{ src = "$house\@.apps\media-img-hq\ops\media_img_hq_manager.c"
       out = "$house\@.apps\media-img-hq\ops\+x\media_img_hq_manager.exe"; flavor = "plain" }

    @{ src = "$house\@.apps\media-vid-hq\ops\media_vid_hq_manager.c"
       out = "$house\@.apps\media-vid-hq\ops\+x\media_vid_hq_manager.exe"; flavor = "plain" }

    @{ src = "$house\@.apps\pdl-read\ops\pdl_read_manager.c"
       out = "$house\@.apps\pdl-read\ops\+x\pdl_read_manager.exe"; flavor = "plain" }

    @{ src = "$house\@.apps\screen-rec-hq\ops\screen_rec_manager.c"
       out = "$house\@.apps\screen-rec-hq\ops\+x\screen_rec_manager.exe"; flavor = "plain" }

    @{ src = "$house\@.apps\text-edit-hq\ops\text_edit_manager.c"
       out = "$house\@.apps\text-edit-hq\ops\+x\text_edit_manager.exe"; flavor = "plain" }

    # db-hq-actors-pal and db-hq-pal both <module> the shared prisc+x.
    @{ src = "$house\&.widgits\_shared-lib\system\prisc+x.c"
       out = "$house\&.widgits\_shared-lib\system\+x\prisc+x.exe"; flavor = "plain" }

    @{ src = "$house\&.hq-apps\db-hq-pal\ops\dbhq_ce_bridge.c"
       out = "$house\&.hq-apps\db-hq-pal\ops\+x\dbhq_ce_bridge.exe"; flavor = "wincompat" }

    # export-hq keeps its source at the toy root, not in ops/ - its own
    # build.sh compiles "$SDIR/export_hq_manager.c".
    @{ src = "$house\&.hq-apps\export-hq\export_hq_manager.c"
       out = "$house\&.hq-apps\export-hq\+x\export_hq_manager.exe"; flavor = "wincompat" }

    # NOT HERE, and each for a stated reason rather than by omission:
    #   media-canvas, media-daw, media-vid, dsr - their templates carry no
    #     <module> at all, so there is no manager to build.
    #   music-player-hq  - music_player_manager.c mpg_start() uses
    #     pipe()+fork()+fcntl() to drive a decoder. That is a real POSIX
    #     subprocess port (CreateProcess + pipes + non-blocking read), not
    #     a header shim, so it is left unwritten rather than faked.
    #   file-explorer    - file_explorer_manager.c fe_spawn_detached()
    #     fork()+setsid()s and its dir walk uses glibc's nlink_t. Same
    #     class: needs a real Windows spawn, not a macro.
)

$fail = 0
foreach ($j in $JOBS) {
    $src = $j.src
    $out = $j.out
    $leaf = Split-Path $out -Leaf
    if (-not (Test-Path -LiteralPath $src)) {
        Write-Host ("-- SKIP {0} (no source: {1})" -f $leaf, $src)
        $fail++
        continue
    }
    New-Item -ItemType Directory -Force -Path (Split-Path $out -Parent) | Out-Null
    # A running manager locks its own .exe; stop it so the link can replace it.
    $stem = [IO.Path]::GetFileNameWithoutExtension($out)
    Get-Process -Name $stem -ErrorAction SilentlyContinue |
        ForEach-Object { Stop-Process -Id $_.Id -Force -ErrorAction SilentlyContinue }

    $a = @($CFLAGS)
    if ($j.flavor -eq "wincompat") { $a += $INCS; $a += $COMPAT }
    else { $a += @("-I", $SHARED) }
    $a += @("-o", $out, $GUI, $src)

    $null = & gcc @a 2>&1
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $out)) {
        Write-Host ("-- FAIL {0} (exit {1})" -f $leaf, $LASTEXITCODE)
        & gcc @a 2>&1 | Select-Object -First 10 | ForEach-Object { "     $_" }
        $fail++
        continue
    }
    $len = (Get-Item -LiteralPath $out).Length
    if ($len -lt 20000) {
        Write-Host ("-- FAIL {0} (suspiciously small: {1} bytes)" -f $leaf, $len)
        $fail++
        continue
    }
    Write-Host ("-- OK   {0} ({1} bytes)" -f $leaf, $len)
}

if ($fail -gt 0) {
    Write-Host ("FAILED: {0} of {1} toy managers did not build" -f $fail, $JOBS.Count)
    exit 1
}
Write-Host ("OK: {0} toy managers built" -f $JOBS.Count)
exit 0