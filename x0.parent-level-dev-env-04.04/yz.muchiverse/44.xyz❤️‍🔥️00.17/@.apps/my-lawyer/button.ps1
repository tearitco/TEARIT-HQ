# button.ps1 - Windows launcher for my-lawyer. Linux: button.sh
param([Parameter(Position=0)][string]$Action="help")
$ErrorActionPreference="Continue"
$SCRIPT_DIR=Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location -LiteralPath $SCRIPT_DIR
$MSYS="C:\msys64\mingw64\bin"; if(Test-Path $MSYS){ if($env:Path -notlike "*$MSYS*"){ $env:Path="$MSYS;$env:Path" } }
$HOUSE_DIR=Split-Path (Split-Path $SCRIPT_DIR -Parent) -Parent
$PROJECT_ID="my-lawyer"
$LAYOUT="pieces/chtpm/layouts/main.chtpm"
$PROJDIR="my-lawyer"

function Get-Bin([string]$rel){
  foreach($c in @(($rel+".exe"),$rel)){ $p=Join-Path $SCRIPT_DIR $c; if(Test-Path -LiteralPath $p){
    try{ $fs=[IO.File]::OpenRead($p); $b0=$fs.ReadByte(); $b1=$fs.ReadByte(); $fs.Close()
      if($b0-eq 0x4D -and $b1-eq 0x5A){ return $p } }catch{} } }
  return $null
}
function Write-NoBom([string]$Path,[string]$Text){
  $d=Split-Path $Path -Parent; if($d -and -not(Test-Path -LiteralPath $d)){ New-Item -ItemType Directory -Path $d -Force|Out-Null }
  [IO.File]::WriteAllText($Path,$Text,(New-Object System.Text.UTF8Encoding $false))
}
function Find-Wsr([string]$name){
  $wsr=Get-ChildItem -LiteralPath $HOUSE_DIR -Directory -EA SilentlyContinue|Where-Object{$_.Name -like '014.wsr*'}|Select-Object -First 1
  if(-not $wsr){return $null}
  foreach($c in @((Join-Path $wsr.FullName "system\$name.exe"),(Join-Path $wsr.FullName "system\$name"))){
    if(Test-Path -LiteralPath $c){ try{ $fs=[IO.File]::OpenRead($c); $b0=$fs.ReadByte(); $b1=$fs.ReadByte(); $fs.Close()
      if($b0-eq 0x4D -and $b1-eq 0x5A){ return $c } }catch{} } }
  return $null
}
function Ensure-WinBins{
  New-Item -ItemType Directory -Path (Join-Path $SCRIPT_DIR "system") -Force|Out-Null
  foreach($n in @("keyboard_input","gl_mirror","chtpm_parser_pal","chtpm_rgb_render","prisc+x","renderer")){
    if(Get-Bin "system\$n"){ Write-Host "OK   system/$n"; continue }
    $d=Find-Wsr $n
    if($d){ Copy-Item -LiteralPath $d -Destination (Join-Path $SCRIPT_DIR "system\$n.exe") -Force; Write-Host "OK   system/$n.exe (wsr donor)" }
    else{ Write-Host "MISS system/$n" -ForegroundColor Yellow }
  }
  foreach($glut in @("$MSYS\libfreeglut.dll","$MSYS\freeglut.dll")){
    if(Test-Path $glut){ $dest=Join-Path $SCRIPT_DIR "system\$(Split-Path $glut -Leaf)"; if(-not(Test-Path $dest)){ Copy-Item $glut $dest -Force }; break }
  }
}
function Invoke-Kill{
  foreach($n in @("keyboard_input","renderer","prisc+x","chtpm_parser_pal","chtpm_rgb_render","gl_mirror","orchestrator")){
    Get-Process -EA SilentlyContinue|Where-Object{ $_.ProcessName -eq $n -or $_.ProcessName -like "$n*" }|ForEach-Object{ try{Stop-Process -Id $_.Id -Force}catch{} }
    taskkill /F /IM "$n.exe" 2>$null|Out-Null
  }
  Write-Host "done"
}
function Copy-TreeLink([string]$src,[string]$dst){
  if(-not(Test-Path -LiteralPath $src)){return}
  if(Test-Path -LiteralPath $dst){return}
  try{ New-Item -ItemType Junction -Path $dst -Target $src -EA Stop|Out-Null }catch{
    try{ New-Item -ItemType SymbolicLink -Path $dst -Target $src -EA Stop|Out-Null }catch{
      if((Get-Item $src).PSIsContainer){ Copy-Item -LiteralPath $src -Destination $dst -Recurse -Force }
      else{ Copy-Item -LiteralPath $src -Destination $dst -Force }
    }
  }
}
function Invoke-Run{
  Write-Host "=== my-lawyer Win ($LAYOUT) ===" -ForegroundColor Cyan
  try{ chcp 65001|Out-Null; [Console]::OutputEncoding=[Text.Encoding]::UTF8 }catch{}
  Ensure-WinBins
  $parser=Get-Bin "system\chtpm_parser_pal"; $kb=Get-Bin "system\keyboard_input"
  if(-not $parser -or -not $kb){ Write-Error "MISSING parser/keyboard PE"; return 1 }
  Invoke-Kill
  $sid="{0}-{1}" -f [int][double]::Parse((Get-Date -UFormat %s)),$PID
  $SESSION=Join-Path $SCRIPT_DIR "pieces\sessions\$sid"
  foreach($d in @("pieces\system","pieces\display","pieces\apps\player_app","pieces\keyboard","pieces\os","projects\$PROJDIR\manager","projects\$PROJDIR\pieces\main","projects\$PROJDIR\pieces\docket","projects\$PROJDIR\pieces\case","projects\$PROJDIR\pieces\mylawyer_menu")){
    New-Item -ItemType Directory -Path (Join-Path $SESSION $d) -Force|Out-Null
  }
  New-Item -ItemType Directory -Path (Join-Path $SCRIPT_DIR "data\corpus") -Force|Out-Null
  New-Item -ItemType Directory -Path (Join-Path $SCRIPT_DIR "data\cases") -Force|Out-Null
  foreach($pair in @(
    @("system","system"),@("ops","ops"),@("pal","pal"),@("default_op.txt","default_op.txt"),
    @("pieces\chtpm","pieces\chtpm"),@("pieces\registry","pieces\registry"),
    @("projects\$PROJDIR\pieces","projects\$PROJDIR\pieces"),@("data","data")
  )){ Copy-TreeLink (Join-Path $SCRIPT_DIR $pair[0]) (Join-Path $SESSION $pair[1]) }
  foreach($f in @("pieces\apps\player_app\interact_relay.txt","pieces\keyboard\history.txt","pieces\system\quit_flag.txt","pieces\display\mylawyer_screen_changed.txt","pieces\display\frame_changed.txt","projects\$PROJDIR\manager\gui_state.txt")){
    Write-NoBom (Join-Path $SESSION $f) ""
  }
  $realSys=Join-Path $SCRIPT_DIR "pieces\system"; New-Item -ItemType Directory -Path $realSys -Force|Out-Null
  $cfg=Join-Path $realSys "config.txt"
  if(-not(Test-Path $cfg)){
    Write-NoBom $cfg "game_id=my-lawyer-001`nplayer_name=Adam Chen`nday=1`nmax_days=10`nmoney=500`nactive_case_id=0`ngame_state=playing`n"
  }
  Copy-TreeLink $cfg (Join-Path $SESSION "pieces\system\config.txt")
  Write-NoBom (Join-Path $SESSION "pieces\apps\player_app\state.txt") "module_path=system/prisc+x pal/main_loop_chtpm.pal`nproject_id=$PROJECT_ID`nactive_target_id=main`n"
  $env:PRISC_PROJECT_ROOT="."; $env:PRISC_PROJECT_ID=$PROJECT_ID; $env:NO_NET="1"; $env:PAL_LAYOUT=$LAYOUT; $env:SKIP_ORCH_COMPILE="1"
  $pParser=Start-Process -FilePath $parser -ArgumentList $LAYOUT -WorkingDirectory $SESSION -WindowStyle Hidden -PassThru
  Write-Host "parser pid=$($pParser.Id)"
  $frame=Join-Path $SESSION "pieces\display\current_frame.txt"
  for($i=0;$i -lt 40;$i++){ if((Test-Path $frame) -and ((Get-Item $frame).Length -gt 0)){ break }; Start-Sleep -Milliseconds 100 }
  $rgb=Get-Bin "system\chtpm_rgb_render"; $gl=Get-Bin "system\gl_mirror"; $rend=Get-Bin "system\renderer"
  $pRgb=$null;$pGl=$null;$pRend=$null
  if($env:NO_GL -ne "1"){
    if($rgb){ $pRgb=Start-Process $rgb -WorkingDirectory $SESSION -WindowStyle Hidden -PassThru }
    if($gl){ $pGl=Start-Process $gl -WorkingDirectory $SESSION -WindowStyle Normal -PassThru; Start-Sleep -Milliseconds 300 }
  }
  if($rend -and $env:NO_TERM -ne "1"){ $pRend=Start-Process $rend -WorkingDirectory $SESSION -WindowStyle Normal -PassThru }
  Write-Host "Keys: THIS console. Ctrl+C quits."
  Push-Location $SESSION
  try{ & $kb } finally {
    Pop-Location
    foreach($p in @($pGl,$pRgb,$pRend,$pParser)){ if($p -and -not $p.HasExited){ try{Stop-Process -Id $p.Id -Force}catch{} } }
    Get-Process -EA SilentlyContinue|Where-Object{$_.ProcessName -match 'prisc'}|Stop-Process -Force -EA SilentlyContinue
    Invoke-Kill
    if(Test-Path -LiteralPath $SESSION){ Remove-Item -LiteralPath $SESSION -Recurse -Force -EA SilentlyContinue }
  }
  return 0
}
function Invoke-Check{
  foreach($b in @("system\chtpm_parser_pal","system\keyboard_input","system\gl_mirror","system\chtpm_rgb_render","system\prisc+x")){
    if(Get-Bin $b){ Write-Host "OK   $b" } else { Write-Host "MISSING $b" }
  }
}
$a=$Action.ToLowerInvariant()
switch($a){
  {$_ -in @("compile","c","build")}{ Ensure-WinBins; exit 0 }
  {$_ -in @("run","r","start")}{ exit (Invoke-Run) }
  {$_ -in @("kill","k","stop")}{ Invoke-Kill; exit 0 }
  {$_ -in @("check","verify")}{ Invoke-Check; exit 0 }
  default{ Write-Host "my-lawyer button.ps1: compile|run|kill|check|help  (wsr PE donor)"; exit 0 }
}
