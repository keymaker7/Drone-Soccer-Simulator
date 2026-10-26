param([string]$EngineRoot='E:\Program Files\UE_5.8',[string]$RepoRoot='E:\GitHub\swarm-drone-atc',[switch]$Demo)
$ErrorActionPreference='Stop'
$ProjectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
New-Item -ItemType Directory -Path (Join-Path $ProjectRoot 'Saved') -Force | Out-Null
$BridgePython=Join-Path $ProjectRoot '.venv\Scripts\python.exe'
$BridgeScript=Join-Path $ProjectRoot 'Bridge\sdacs_bridge.py'
$BridgeHealthy=$false
try { $Health=Invoke-RestMethod 'http://127.0.0.1:8766/health' -TimeoutSec 1; $BridgeHealthy=($Health.source -eq 'swarm-drone-atc') } catch {}
if(-not $BridgeHealthy -and (Test-Path -LiteralPath $BridgePython) -and (Test-Path -LiteralPath $BridgeScript)){
    $Arguments=@(('"'+$BridgeScript+'"'),'--repo',('"'+$RepoRoot+'"'),'--host','127.0.0.1','--port','8766')
    Start-Process -FilePath $BridgePython -ArgumentList $Arguments -WindowStyle Hidden -RedirectStandardOutput (Join-Path $ProjectRoot 'Saved\bridge.log') -RedirectStandardError (Join-Path $ProjectRoot 'Saved\bridge-errors.log')
}
$Arguments=@('-windowed','-ResX=1600','-ResY=1000','-ForceRes','-NoScreenMessages')
if($Demo){$Arguments+='-ImperialDemo'}
$Packaged=Join-Path $ProjectRoot 'dist\Windows\ImperialWingsSimulator.exe'
if(Test-Path -LiteralPath $Packaged){Start-Process -FilePath $Packaged -ArgumentList $Arguments}
else{
    $Editor=Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
    $Arguments=@(('"'+(Join-Path $ProjectRoot 'ImperialWingsSimulator.uproject')+'"'),'-game')+$Arguments
    Start-Process -FilePath $Editor -ArgumentList $Arguments
}
