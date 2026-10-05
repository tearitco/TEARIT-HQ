# scripts/e2e.ps1 - end-to-end proof for HORN_CHAT on win32.
#
# WHAT THIS IS
#
# The Windows counterpart of scripts/e2e.sh. It boots the real 3-process stack
# (renderer + chtpm_parser_pal + the pal module the layout forks), drives real
# keystrokes through the same pieces/keyboard/history.txt that keyboard_input
# writes, and asserts on the real frame. No mocking: this is the whole harness,
# driven headlessly.
#
# WHAT THIS DELIBERATELY DOES NOT CLAIM
#
# scripts/e2e.sh asserts that the sandbox blocks writes outside the project,
# blocks the network, and hides the provider keys. Those three hold on Linux
# because bwrap provides namespace isolation. Windows has no equivalent, so
# those assertions are SKIPPED here with a printed reason rather than
# translated into weaker versions that would report green while the guarantee
# is absent. horn_tool_exec already refuses run_script outright on Windows
# unless HORN_WIN_SANDBOX=ack, precisely because of this; a test that quietly
# downgraded its own expectations would undo that.
#
# What IS proved here is the Windows containment that does exist: process-tree
# containment by Job Object, a pinned working directory, a restricted PATH, and
# the allowlist refusals - which are platform-independent and are the real
# security boundary for everything except run_script.
#
# Usage: powershell -File scripts/e2e.ps1 [-NoApi]

param([switch]$NoApi)

$ErrorActionPreference = "Continue"
$ROOT = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
Set-Location $ROOT

$env:PRISC_PROJECT_ROOT = $ROOT
$env:PRISC_PROJECT_ID   = "hai-horn"
$env:HORN_SESSIONS     = Join-Path $ROOT "chats/HORN_SESSIONS"

# Every path variable is f-prefixed. $history is an alias for Get-History in
# PowerShell, and assigning to it fails; $keys and $tool are close enough to
# automatic names to be worth avoiding too.
$fKeys      = Join-Path $ROOT "pieces/keyboard/history.txt"
$fFrame     = Join-Path $ROOT "pieces/display/current_frame.txt"
$fGuiState  = Join-Path $ROOT "pieces/apps/player_app/manager/gui_state.txt"
$fIsTyping  = Join-Path $ROOT "pieces/display/active_gui_is_typing.txt"
$fRelay     = Join-Path $ROOT "pieces/apps/player_app/interact_relay.txt"
$fTranscript = Join-Path $ROOT "chats/HORN_SESSIONS/transcript.txt"
$fHistory   = Join-Path $ROOT "chats/HORN_SESSIONS/chat_history.txt"
$fTool      = Join-Path $ROOT "ops/+x/horn_tool_exec.+x"

$script:Pass = 0
$script:Fail = 0
$script:Procs = @()

function Ok($m)  { Write-Host "  PASS  $m" -ForegroundColor DarkGreen; $script:Pass++ }
function Bad($m) { Write-Host "  FAIL  $m" -ForegroundColor Red;    $script:Fail++ }
function Skip($m) { Write-Host "  SKIP  $m" -ForegroundColor DarkYellow }

function Check($name, $haystack, $needle) {
    if ($haystack -and $haystack.Contains($needle)) { Ok $name }
    else {
        $s = "" + $haystack
        Bad ("{0} (want '{1}' in: {2})" -f $name, $needle, $s.Substring(0, [Math]::Min(90, $s.Length)))
    }
}

# Run a project op and capture its stdout. Ops are named +x, which PowerShell
# will not execute directly, so Start-Process is the only route. stderr is
# folded into the returned text because several assertions legitimately match
# an error message the op writes there.
function Invoke-Op {
    param([string]$Exe, [string[]]$ArgList, [int]$TimeoutSec = 90)
    $out = Join-Path $env:TEMP ("horn_op_" + [Guid]::NewGuid().ToString("N") + ".txt")
    $err = "$out.err"
    $p = Start-Process -FilePath $Exe -ArgumentList $ArgList -WorkingDirectory $ROOT `
         -NoNewWindow -PassThru -RedirectStandardOutput $out -RedirectStandardError $err
    if (-not $p.WaitForExit($TimeoutSec * 1000)) {
        try { $p.Kill() } catch {}
        return @{ code = -1; text = "TIMEOUT after ${TimeoutSec}s" }
    }
    $text = ""
    if (Test-Path $out) { $text += (Get-Content $out -Raw -EA SilentlyContinue) }
    if (Test-Path $err) { $text += (Get-Content $err -Raw -EA SilentlyContinue) }
    Remove-Item $out, $err -Force -EA SilentlyContinue
    return @{ code = $p.ExitCode; text = $text }
}

# JSON has to survive Windows argv quoting: inner quotes are backslash-escaped
# inside the outer quoted argument. Without this the op receives mangled JSON
# and every tool call reports "needs a path", which looks like a tool bug.
function Invoke-Tool {
    param([string]$ToolName, [string]$Json, [int]$TimeoutSec = 90)
    $escaped = $Json -replace '"', '\"'
    return Invoke-Op -Exe $fTool -ArgList @($ToolName, ('"' + $escaped + '"')) -TimeoutSec $TimeoutSec
}

# --- process control -------------------------------------------------------
# Windows has no pkill -f. Each process is captured by handle at launch, which
# is stronger than pattern matching: the teardown cannot silently match nothing
# and report a clean run, which is exactly how earlier runs of the bash suite
# "passed" the no-leak assertion while leaving orphans behind.
function Stop-Stack {
    foreach ($p in $script:Procs) {
        try { if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force -EA SilentlyContinue } } catch {}
    }
    $script:Procs = @()
    # Handles alone are not enough: chtpm_parser_pal FORKS the pal module, and
    # that child is prisc+x, a process this harness never launched and so never
    # captured. Stopping only the captured handles left it alive, and it then
    # held system\prisc+x.exe open so the next build died with "cannot open
    # output file: Permission denied". Name-based sweep catches the forked
    # grandchild.
    Get-Process -Name "prisc+x" -EA SilentlyContinue | Stop-Process -Force -EA SilentlyContinue
    Start-Sleep -Milliseconds 400
}

function Start-Stack {
    Stop-Stack
    Set-Content -Path $fGuiState -Value "horn_prompt=" -NoNewline -EA SilentlyContinue
    Set-Content -Path $fKeys    -Value "" -NoNewline -EA SilentlyContinue
    Set-Content -Path $fIsTyping -Value "0" -NoNewline -EA SilentlyContinue
    # NOT `& op.+x`: PowerShell refuses to activate a `+x` path through the
    # call operator ("CantActivateDocumentInPipeline"), so publishing through
    # it silently did nothing. Start-Process, which Invoke-Op already wraps,
    # is the only working route for an op on this platform.
    $null = Invoke-Op -Exe (Join-Path $ROOT "ops/+x/horn_publish.+x") -ArgList @() -TimeoutSec 30

    $script:Procs += Start-Process -FilePath (Join-Path $ROOT "system/renderer") `
        -WorkingDirectory $ROOT -WindowStyle Hidden -PassThru `
        -RedirectStandardOutput "$env:TEMP/horn_render.log" `
        -RedirectStandardError  "$env:TEMP/horn_render.err"
    $script:Procs += Start-Process -FilePath (Join-Path $ROOT "system/chtpm_parser_pal") `
        -ArgumentList "layouts/horn_chat.chtpm" `
        -WorkingDirectory $ROOT -WindowStyle Hidden -PassThru `
        -RedirectStandardOutput "$env:TEMP/horn_parser.log" `
        -RedirectStandardError  "$env:TEMP/horn_parser.err"
    Start-Sleep -Seconds 3
}

# --- keystrokes ------------------------------------------------------------
# chtpm tails the key history asynchronously, so each character must wait for
# the parser's OWN view (gui_state) to catch up before the next is sent.
# The wait has to be on the prompt's LENGTH, not merely on "horn_prompt="
# being present - that prefix is present before the first character lands, so
# waiting on it returns instantly and the next key is written while the parser
# is still draining the previous one. That race silently ate characters: a run
# typed "What is 6 ties 7?" instead of "What is 6 times 7?". A fixed sleep has
# the same failure mode with different luck.
function Type-Str([string]$s) {
    foreach ($ch in $s.ToCharArray()) {
        $before = (Get-Field $fGuiState "horn_prompt").Length
        Add-Content -Path $fKeys -Value ("KEY_PRESSED: " + [int][char]$ch)
        for ($i = 0; $i -lt 60; $i++) {
            if ((Get-Field $fGuiState "horn_prompt").Length -gt $before) { break }
            Start-Sleep -Milliseconds 150
        }
    }
    Start-Sleep -Milliseconds 300
}
function Key([int]$code) { Add-Content -Path $fKeys -Value "KEY_PRESSED: $code"; Start-Sleep -Milliseconds 250 }
function Esc()   { Key 27 }
function Enter() { Key 13 }

function Get-Field {
    param([string]$Path, [string]$Key)
    if (-not (Test-Path $Path)) { return "" }
    foreach ($l in Get-Content $Path -EA SilentlyContinue) {
        if ($l.StartsWith("$Key=")) { return $l.Substring($Key.Length + 1) }
    }
    return ""
}

function Wait-Typing {
    for ($i = 0; $i -lt 40; $i++) {
        if ((Get-Content $fIsTyping -Raw -EA SilentlyContinue).Trim() -eq "1") { return $true }
        Start-Sleep -Milliseconds 250
    }
    return $false
}

# ESC deactivates but does NOT clear the composer (chtpm's own handler), so
# leftovers survive and typing APPENDS to them - which produced doubled
# prompts. Backspace it out explicitly, one at a time, waiting for each.
function Empty-Composer {
    for ($n = 0; $n -lt 300; $n++) {
        $len = (Get-Field $fGuiState "horn_prompt").Length
        if ($len -le 0) { return $true }
        Add-Content -Path $fKeys -Value "KEY_PRESSED: 127"
        for ($i = 0; $i -lt 40; $i++) {
            if ((Get-Field $fGuiState "horn_prompt").Length -lt $len) { break }
            Start-Sleep -Milliseconds 150
        }
    }
    return $false
}

function Focus-Composer {
    Esc; Enter
    if (-not (Wait-Typing)) { return $false }
    $null = Empty-Composer
    return $true
}

function Wait-For {
    param([string]$Needle, [string]$Path, [int]$Tries = 40)
    for ($i = 0; $i -lt $Tries; $i++) {
        if (Test-Path $Path) {
            $c = Get-Content $Path -Raw -EA SilentlyContinue
            if ($c -and $c.Contains($Needle)) { return $true }
        }
        Start-Sleep -Milliseconds 400
    }
    return $false
}

function Turn-Count {
    if (-not (Test-Path $fHistory)) { return 0 }
    $n = 0
    foreach ($l in Get-Content $fHistory -EA SilentlyContinue) {
        $f = $l -split "`t"
        if ($f.Count -ge 2 -and $f[1] -eq "user") { $n++ }
    }
    return $n
}

Write-Host "=== build ==="
$env:Path = "C:\msys64\mingw64\bin;$env:Path"
$build = & powershell -File (Join-Path $ROOT "scripts/build.ps1") 2>&1
$buildText = ($build | ForEach-Object { [string]$_ }) -join "`n"
if ($LASTEXITCODE -ne 0) {
    Write-Host "BUILD FAILED"
    Write-Host $buildText
    exit 1
}
# A build that warns from a source THIS PORT TOUCHED is a failed build for
# house purposes. Warnings from untouched project sources are reported but do
# not fail the run: horn_completions.c carries a pre-existing
# -Wformat-truncation warning that only appears under MinGW, dating from the
# original v0.1 commit, and failing on it here would mean this harness could
# never pass without editing a file unrelated to the Windows port.
$portFiles = @("horn_tool_exec", "horn_turn", "keyboard_input", "renderer")
$ownWarn = @()
$preExisting = @()
foreach ($line in ($buildText -split "`n")) {
    # Only a NON-ZERO count is a warning. Matching on the word "warning"
    # alone also matches the "0 warning(s)" line that every clean target
    # prints, so the first version of this check failed the build while
    # displaying nothing but zero-warning lines.
    if ($line -notmatch '(\d+)\s+warning\(s\)') { continue }
    if ([int]$matches[1] -eq 0) { continue }
    $isPorted = $false
    foreach ($pf in $portFiles) { if ($line.Contains($pf)) { $isPorted = $true } }
    if ($isPorted) { $ownWarn += $line } else { $preExisting += $line }
}
if ($ownWarn.Count -gt 0) {
    Write-Host "BUILD FAILED: warnings in ported sources"
    Write-Host ($ownWarn -join "`n")
    exit 1
}
if ($preExisting.Count -gt 0) {
    Write-Host "  note  pre-existing warnings in untouched sources (not a failure here):"
    foreach ($l in $preExisting) { Write-Host ("        " + $l.Trim()) }
}
Write-Host "  ok"

Write-Host "=== boot ==="
Start-Stack

if (@(Get-Process -Name "prisc+x" -EA SilentlyContinue).Count -gt 0) {
    Ok "pal module forked by the parser"
} else {
    Bad "pal module not running (check $env:TEMP/horn_parser.log)"
}

$null = Wait-For "HORN_CHAT" $fFrame 20
$frameText = Get-Content $fFrame -Raw -EA SilentlyContinue
Check "first frame renders the box" $frameText "HORN_CHAT"
Check "approval buttons are present" $frameText "APPROVE"

Write-Host "=== composer accumulates typed text (offline) ==="
if (-not (Focus-Composer)) { Bad "composer never became active" }
Type-Str "What is 6 times 7?"
if (Wait-For "What is 6 times 7?" $fFrame 20) {
    Ok "typed text accumulates in the composer"
} else {
    Bad "composer did not render the typed text (gui_state had: $(Get-Field $fGuiState 'horn_prompt'))"
}

Write-Host "=== tool allowlist (offline, platform-independent) ==="
$r = Invoke-Tool "list_dir" '{"path":"ops"}'
Check "list_dir lists the project" $r.text "horn_turn.c"

$r = Invoke-Tool "read_file" '{"path":"tools/horn_tools.json","max_bytes":400}'
# The needle must be inside the FIRST 400 bytes. "horn_tools" is not: the file
# opens with a prose comment, so the first match is "_comment". Truncation is
# the tool behaving correctly - the assertion was simply looking past the cap.
Check "read_file returns file contents" $r.text "_comment"

$r = Invoke-Tool "grep_files" '{"pattern":"MAX_TOOL_ROUNDS","path":"ops/horn_turn.c"}'
Check "grep_files greps a FILE path" $r.text "MAX_TOOL_ROUNDS"

# The allowlist IS the security boundary: a tool the model invents must be
# refused, not dispatched.
$r = Invoke-Tool "definitely_not_a_real_tool" '{"path":"/"}'
Check "unregistered tool is refused" $r.text "not registered"

$r = Invoke-Tool "grep_files" '{"pattern":"tool","path":"chats"}'
Check "grep refuses the session transcript" $r.text "refusing to search"

$r = Invoke-Tool "read_file" '{"path":"C:/Windows/System32/config/SAM"}'
if ($r.text -match "error:") { Ok "unreadable file returns an error, not a crash" }
else { Bad "read_file on a protected file did not error" }

Write-Host "=== write containment (offline, platform-independent) ==="
New-Item -ItemType Directory -Force -Path (Join-Path $ROOT "config") | Out-Null
Set-Content -Path (Join-Path $ROOT "config/yolo.flag") -Value "" -NoNewline

$editFile = Join-Path $ROOT "dox/.e2e_edit.txt"
Set-Content -Path $editFile -Value "alpha`nbeta`nbeta`n"
$r = Invoke-Tool "edit_file" '{"path":"dox/.e2e_edit.txt","search":"beta","replace":"BETA"}'
Check "edit_file refuses an ambiguous match" $r.text "appears 2 times"
$r = Invoke-Tool "edit_file" '{"path":"dox/.e2e_edit.txt","search":"beta","replace":"X","replace_all":1}'
Check "edit_file honours replace_all" $r.text "2 replacements"
$r = Invoke-Tool "edit_file" '{"path":"dox/.e2e_edit.txt","search":"absent","replace":"y"}'
Check "edit_file refuses a missing match" $r.text "not found"
Remove-Item $editFile -Force -EA SilentlyContinue

$wFile = Join-Path $ROOT "dox/.e2e_w.txt"
Set-Content -Path $wFile -Value "# original"
$r = Invoke-Tool "write_file" '{"path":"dox/.e2e_w.txt","content":"# overwritten\n"}'
Check "write_file overwrites an existing file" $r.text "wrote"
# Compare TRIMMED. write_file writes the bytes verbatim in binary mode, so the
# trailing newline is an LF the tool received from the JSON and passed through.
# -Raw hands that newline back too, so an exact -eq against a string with no
# newline failed on a write that had in fact landed.
if (((Get-Content $wFile -Raw) -replace '\s+$','') -eq "# overwritten") { Ok "write landed on disk" }
else { Bad "write_file did not change the file (file holds: '$(Get-Content $wFile -Raw)')" }

# The allowlist is writable by a human but must NOT be writable by a model,
# or the gate list beside it stops meaning anything.
$r = Invoke-Tool "write_file" '{"path":"tools/horn_tools.json","content":"x"}'
Check "write_file refuses the tool allowlist itself" $r.text "protected path"
$r = Invoke-Tool "write_file" '{"path":"dox/../../../../tmp/e2e-escape","content":"x"}'
Check "write_file refuses '..' escapes" $r.text "outside the project root"
$r = Invoke-Tool "write_file" '{"path":"C:/Windows/Temp/e2e-abs","content":"x"}'
Check "write_file refuses absolute paths" $r.text "outside the project root"
$r = Invoke-Tool "write_file" '{"path":"config/raw_groq.txt","content":"x"}'
Check "write_file refuses key-shaped files" $r.text "protected path"
$r = Invoke-Tool "write_file" '{"path":"no/such/dir/x.txt","content":"x"}'
Check "write_file will not create directories" $r.text "does not exist"
Remove-Item $wFile -Force -EA SilentlyContinue

Write-Host "=== windows sandbox: refusal by default ==="
# This is the load-bearing Windows-specific assertion. Without the ack,
# run_script must refuse, because the Linux guarantees cannot be honoured.
Remove-Item Env:\HORN_WIN_SANDBOX -EA SilentlyContinue
$r = Invoke-Tool "run_script" '{"command":"echo sandbox-ok"}'
Check "run_script REFUSES without HORN_WIN_SANDBOX" $r.text "refuses to execute on Windows"
Check "the refusal names the read-only gap" $r.text "read-only filesystem"
Check "the refusal names the network gap" $r.text "no network access"
Check "the refusal names the secrets gap" $r.text "secrets directory"
if ($r.text -match "sandbox-ok") { Bad "run_script executed anyway despite refusing" }
else { Ok "no command ran while the sandbox was unacknowledged" }

Write-Host "=== windows sandbox: what IS enforced, with ack ==="
$env:HORN_WIN_SANDBOX = "ack"
$r = Invoke-Tool "run_script" '{"command":"echo sandbox-ok"}'
Check "run_script runs an acknowledged command" $r.text "sandbox-ok"
Check "exit status is reported" $r.text "[exit 0]"

$r = Invoke-Tool "run_script" '{"command":"exit 3"}'
Check "a non-zero exit is reported honestly" $r.text "[exit 3]"

# Working directory pinned: "../" must not escape the project.
$r = Invoke-Tool "run_script" '{"command":"cd"}'
Check "working directory is pinned to the project" $r.text "hai-horn"

# PATH restricted: the model cannot resolve a tool out of the project tree.
$r = Invoke-Tool "run_script" '{"command":"echo %PATH%"}'
if ($r.text -match "msys64|mingw64|Git") {
    Bad ("PATH was NOT restricted: " + ($r.text -replace '\s+', ' '))
} else {
    Ok "PATH is restricted to system directories"
}

# Job Object: a command outliving its timeout is killed.
$sw = [Diagnostics.Stopwatch]::StartNew()
$r = Invoke-Tool "run_script" '{"command":"ping -n 30 127.0.0.1","timeout_s":3}' 60
$sw.Stop()
if ($sw.Elapsed.TotalSeconds -lt 20) {
    Ok "timeout fires early ($([math]::Round($sw.Elapsed.TotalSeconds,1))s for a 30s command)"
} else {
    Bad "timeout did not fire - ran $([math]::Round($sw.Elapsed.TotalSeconds,1))s"
}
Check "timeout reports the job object killed the tree" $r.text "job object killed the tree"
Start-Sleep -Milliseconds 500
if (@(Get-Process ping -EA SilentlyContinue).Count -eq 0) {
    Ok "no orphaned process survived the timeout"
} else {
    Bad "a process outlived the timeout"
    @(Get-Process ping -EA SilentlyContinue) | Stop-Process -Force -EA SilentlyContinue
}

# Job Object containment: a grandchild must not outlive its parent.
$r = Invoke-Tool "run_script" '{"command":"start /b ping -n 45 127.0.0.1 > nul & exit 0"}' 40
Start-Sleep -Seconds 2
if (@(Get-Process ping -EA SilentlyContinue).Count -eq 0) {
    Ok "a grandchild cannot outlive its parent (KILL_ON_JOB_CLOSE)"
} else {
    Bad "a grandchild survived the parent - Job Object did not contain the tree"
    @(Get-Process ping -EA SilentlyContinue) | Stop-Process -Force -EA SilentlyContinue
}

Remove-Item Env:\HORN_WIN_SANDBOX -EA SilentlyContinue
Remove-Item (Join-Path $ROOT "config/yolo.flag") -Force -EA SilentlyContinue

Write-Host "=== guarantees this platform cannot make ==="
# Printed as SKIPs with reasons rather than translated into weaker assertions.
# A green line here would be a lie: these three hold on Linux via bwrap and
# have no Windows equivalent. The default refusal above is the mitigation.
Skip "sandbox blocks writes outside the project - NO Windows equivalent of bwrap --ro-bind / /. The pinned cwd helps, but a command can still write anywhere the user can."
Skip "sandbox blocks the network - NO Windows equivalent of bwrap --unshare-net. This is why run_script refuses by default."
Skip "sandbox hides the provider keys (&.widgits) - NO equivalent of the tmpfs blanking, so keys ARE reachable from an acknowledged run_script. This is the single most important difference."

Write-Host "=== live api ==="
$apiUp = $false
if ($NoApi) {
    Skip "-NoApi was passed; live round-trip assertions are skipped."
} else {
    $probe = Invoke-Op -Exe (Join-Path $ROOT "ops/+x/horn_chat_backend.+x") `
                       -ArgList @("Reply with the single word: ready") -TimeoutSec 60
    # Judge by exit code, not by grepping the output. An earlier version
    # grepped for "quota" while the probe prompt itself contained that word,
    # so the model's own reply tripped the check and the live path was skipped
    # while the suite still printed a green summary.
    if ($probe.code -eq 0 -or $probe.code -eq 10) { $apiUp = $true }
}
if (-not $apiUp) {
    Skip "no provider is answering. Live round-trip assertions are skipped - the harness cannot distinguish an exhausted daily quota from a broken transport mid-run."
}

Write-Host "=== teardown ==="
Stop-Stack
# The pal module runs as process name "prisc+x" - the .pal file is an ARGUMENT
# to it, never a process name. Checking for "horn_main_loop.pal" matched
# nothing, so this assertion reported "no leaked processes" while two prisc+x
# instances were still alive. They then held system\prisc+x.exe open and the
# NEXT build failed with "cannot open output file: Permission denied" - a
# cascade that looked like a compiler fault.
$stray = @(Get-Process -Name "renderer", "chtpm_parser_pal", "prisc+x" -EA SilentlyContinue)
if ($stray.Count -gt 0) {
    Bad "processes leaked after teardown: $(($stray | ForEach-Object { "$($_.Name)#$($_.Id)" }) -join ', ')"
    $stray | Stop-Process -Force -EA SilentlyContinue
} else {
    Ok "no leaked processes"
}

Write-Host ""
Write-Host "=== $($script:Pass) passed, $($script:Fail) failed ==="
if ($script:Fail -ne 0) { exit 1 }
exit 0