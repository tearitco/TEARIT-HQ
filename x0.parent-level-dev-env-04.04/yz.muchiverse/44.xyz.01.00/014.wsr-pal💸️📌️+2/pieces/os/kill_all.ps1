# kill_all.ps1 - Windows parity with kill_all.sh (surgical cleanup)
# Do NOT access Process.Path - that hangs on some Windows setups.

Write-Host "=== wsr-pal kill_all.ps1 - surgical cleanup ==="

$names = @(
    "orchestrator", "renderer", "keyboard_input",
    "chtpm_parser_pal", "chtpm_rgb_render", "gl_mirror", "prisc+x"
)

foreach ($n in $names) {
    $procs = Get-Process -ErrorAction SilentlyContinue |
        Where-Object { $_.ProcessName -eq $n -or $_.ProcessName -like "$n*" }
    if ($procs) {
        Write-Host "Killing $n..."
        foreach ($p in $procs) {
            try { Stop-Process -Id $p.Id -Force -ErrorAction Stop } catch { }
        }
    }
    & taskkill /F /IM "$n.exe" 2>$null | Out-Null
}

Get-Process -ErrorAction SilentlyContinue |
    Where-Object { $_.ProcessName -like "prisc*" } |
    ForEach-Object {
        try { Stop-Process -Id $_.Id -Force -ErrorAction Stop } catch { }
    }

Start-Sleep -Milliseconds 200

$SCRIPT_DIR = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$quit = Join-Path $SCRIPT_DIR "pieces\system\quit_flag.txt"
$procList = Join-Path $SCRIPT_DIR "pieces\os\proc_list.txt"
if (Test-Path $quit) { Remove-Item $quit -Force -ErrorAction SilentlyContinue }
if (Test-Path $procList) { Remove-Item $procList -Force -ErrorAction SilentlyContinue }

Write-Host "Cleanup complete."
