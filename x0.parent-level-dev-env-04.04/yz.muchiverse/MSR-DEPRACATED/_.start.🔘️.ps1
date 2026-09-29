# _.start.<emoji>.ps1 - Windows twin of _.start.<emoji>.sh
#
# The .sh does exactly two things:
#     cd $.<emoji>/
#     if darwin -> ./+x/m.button...mac.+x
#     else      -> ./+x/l.button...g11.+x
# i.e. pick the orchestrator that matches this OS and hand the terminal
# over to it. This is the Windows arm of that same three-way choice.
#
# Linux and macOS keep their own script and binary, untouched.
#
# WHY NOTHING HERE IS GLOB-FILLED OR CODE-POINT-BUILT
#   An earlier version assembled the orchestrator directory name from
#   literal code points (U+1F518, U+00AE, U+2122). It failed. The real
#   directory name is "<emoji> U+FE0F" - the button is followed by
#   VARIATION SELECTOR-16 - and omitting that one code point produces a
#   name that Test-Path rejects, so the launcher aborted with
#   "orchestrator dir not found" on a directory that plainly exists.
#
#   The house naming scheme is not stable enough to re-derive by hand:
#   it already varies in emoji, variation selectors, registered/trademark
#   marks and brackets. So the name is discovered, not constructed. If the
#   directory is ever renamed, this script keeps working with no edit.

$ErrorActionPreference = "Continue"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path

# Find the Windows orchestrator by shape, not by name. The w11.ps1 suffix
# is unique to the PowerShell build - the Linux sibling is g11.c and the
# macOS one is i13 - so this cannot collide with either.
$orch = Get-ChildItem -LiteralPath $here -Recurse -File -Filter "l.button.*w11.ps1" -EA SilentlyContinue |
    Select-Object -First 1

if (-not $orch) {
    Write-Error "No l.button.*w11.ps1 orchestrator found under $here"
    exit 1
}

Write-Host "=== MarS StreetRace (Windows) ==="

# Run the orchestrator IN THIS PROCESS via the call operator, not by
# spawning `powershell -File <path>`.
#
# This is not a style choice. A child process receives its command line
# through the ANSI codepage, so the emoji in this directory name arrives as
# "<mojibake>" and the child cannot find the orchestrator. That was
# observed, not theorised: the same mangling is why _.start.<emoji>.sh in
# the repo has a mojibake directory name baked into it, and it is also why
# the build script links non-ASCII outputs to an ASCII temp name (ld.exe
# has the identical limitation).
#
# The call operator hands the path straight to PowerShell as a .NET string,
# with no code-page round trip, so the real name survives. The cost is that
# the orchestrator's `exit 0` ends this script too - which is exactly the
# behaviour the .sh has, where control is handed to the orchestrator and
# nothing runs after it.
& $orch.FullName
exit $LASTEXITCODE
