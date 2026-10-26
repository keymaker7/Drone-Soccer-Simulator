param([string]$EngineRoot='E:\Program Files\UE_5.8',[string]$RepoRoot='E:\GitHub\swarm-drone-atc',[switch]$Demo,[switch]$AllowStaleBuild)
$ErrorActionPreference='Stop'
$ProjectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$Packaged=Join-Path $ProjectRoot 'dist\Windows\ImperialWingsSimulator.exe'
$BuildIdentityPath=if(Test-Path -LiteralPath $Packaged){Join-Path $ProjectRoot 'dist\Windows\ImperialWingsSimulator\Binaries\Win64\ImperialBuildIdentity-ImperialWingsSimulator.json'}else{Join-Path $ProjectRoot 'Binaries\Win64\ImperialBuildIdentity-ImperialWingsSimulatorEditor.json'}
$SourceIdentity=& (Join-Path $PSScriptRoot 'Get-SourceIdentity.ps1') -ProjectRoot $ProjectRoot
$BuildMatches=$false
if(Test-Path -LiteralPath $BuildIdentityPath){
    try{$BuiltIdentity=Get-Content -LiteralPath $BuildIdentityPath -Raw | ConvertFrom-Json;$BuildMatches=$BuiltIdentity.source_sha256 -eq $SourceIdentity.source_sha256}catch{}
}
if(-not $BuildMatches){
    $Message='현재 소스와 실행 파일의 버전이 일치하지 않거나 빌드 정보가 없습니다. Scripts\Build.ps1 -Package로 다시 빌드해 주세요.'
    if(-not $AllowStaleBuild){throw $Message}
    Write-Warning ($Message+' 이전 실행 파일을 명시적으로 선택해 실행합니다.')
}
New-Item -ItemType Directory -Path (Join-Path $ProjectRoot 'Saved') -Force | Out-Null
$BridgePython=Join-Path $ProjectRoot '.venv\Scripts\python.exe'
$BridgeScript=Join-Path $ProjectRoot 'Bridge\sdacs_bridge.py'
$BridgeHealthy=$false
$BridgeNeedsRestart=$false
try { $Health=Invoke-RestMethod 'http://127.0.0.1:8766/health' -TimeoutSec 1; $BridgeHealthy=($Health.source -eq 'swarm-drone-atc' -and $Health.protocol_version -eq 2);$BridgeNeedsRestart=($Health.source -eq 'swarm-drone-atc' -and -not $BridgeHealthy) } catch {}
if($BridgeNeedsRestart){throw '이전 버전의 자율 연결 프로그램이 실행 중입니다. 해당 프로그램을 종료하고 다시 실행해 주세요. 수동 조종에는 자율 연결 프로그램이 필요하지 않습니다.'}
if(-not $BridgeHealthy -and (Test-Path -LiteralPath $BridgePython) -and (Test-Path -LiteralPath $BridgeScript)){
    $Arguments=@(('"'+$BridgeScript+'"'),'--repo',('"'+$RepoRoot+'"'),'--host','127.0.0.1','--port','8766')
    Start-Process -FilePath $BridgePython -ArgumentList $Arguments -WindowStyle Hidden -RedirectStandardOutput (Join-Path $ProjectRoot 'Saved\bridge.log') -RedirectStandardError (Join-Path $ProjectRoot 'Saved\bridge-errors.log')
}
$Arguments=@('-windowed','-ResX=1600','-ResY=1000','-ForceRes','-NoScreenMessages')
if($Demo){$Arguments+='-ImperialDemo'}
if(Test-Path -LiteralPath $Packaged){Start-Process -FilePath $Packaged -ArgumentList $Arguments}
else{
    $Editor=Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
    $Arguments=@(('"'+(Join-Path $ProjectRoot 'ImperialWingsSimulator.uproject')+'"'),'-game')+$Arguments
    Start-Process -FilePath $Editor -ArgumentList $Arguments
}
