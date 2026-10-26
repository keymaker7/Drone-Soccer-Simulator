[CmdletBinding()]
param([string]$EngineRoot='',[string]$RepoRoot='',[string]$PackageRoot='',[int]$BridgePort=0,
      [switch]$Demo,[switch]$AllowStaleBuild,[switch]$ManualOnly,[switch]$Inspect)
$ErrorActionPreference='Stop'
$ProjectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
. (Join-Path $PSScriptRoot 'Environment.ps1')
$environment=Get-ImperialEnvironment -ProjectRoot $ProjectRoot -EngineRoot $EngineRoot -RepoRoot $RepoRoot -PackageRoot $PackageRoot -BridgePort $BridgePort
$package=$environment.PackageRoot
$executable=Join-Path $package 'ImperialWingsSimulator.exe'
$manifestPath=Join-Path $package 'ImperialWingsSimulator\Binaries\Win64\ImperialBuildIdentity-ImperialWingsSimulator.json'
$source=& (Join-Path $PSScriptRoot 'Get-SourceIdentity.ps1') -ProjectRoot $ProjectRoot
$manifest=$null;$matches=$false
if(Test-Path -LiteralPath $manifestPath){
    try{$manifest=Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json;$matches=$manifest.source_sha256 -eq $source.source_sha256}catch{}
}
$inspection=[ordered]@{executable=$executable;exists=(Test-Path -LiteralPath $executable);matches_source=$matches;source_sha256=$source.source_sha256;build=$manifest;bridge_url=$environment.BridgeUrl;manual_only=[bool]$ManualOnly}
if($Inspect){[pscustomobject]$inspection;return}
if(-not(Test-Path -LiteralPath $executable)){throw '통합 실행 파일이 없습니다. Scripts\Build.ps1 -Package를 먼저 실행해 주세요.'}
if(-not $matches){
    $message='현재 소스와 통합 실행 파일이 다릅니다. Scripts\Build.ps1 -Package로 다시 빌드해 주세요.'
    if(-not $AllowStaleBuild){throw $message};Write-Warning $message
}
New-Item -ItemType Directory -Path (Join-Path $ProjectRoot 'Saved') -Force | Out-Null
# Copy only an existing real profile. Activation is never persisted.
$profileTarget=Join-Path $package 'ImperialWingsSimulator\Saved\Config\RadioProfile.json'
if(-not(Test-Path -LiteralPath $profileTarget)){
    foreach($candidate in @((Join-Path $ProjectRoot 'dist\Windows\ImperialWingsSimulator\Saved\Config\RadioProfile.json'),(Join-Path $ProjectRoot 'Saved\Config\RadioProfile.json'))){
        if(Test-Path -LiteralPath $candidate){
            $profile=Get-Content -LiteralPath $candidate -Raw | ConvertFrom-Json
            if($profile.version -eq 1 -and $profile.profiles){
                New-Item -ItemType Directory -Path (Split-Path -Parent $profileTarget) -Force | Out-Null
                Copy-Item -LiteralPath $candidate -Destination $profileTarget;break
            }
        }
    }
}
if(-not $ManualOnly){
    $python=Join-Path $ProjectRoot '.venv\Scripts\python.exe'
    $bridge=Join-Path $ProjectRoot 'Bridge\sdacs_bridge.py'
    if(-not(Test-Path -LiteralPath $python)){throw '자율 훈련용 Python 환경이 없습니다. Setup-Bridge.ps1 또는 -ManualOnly를 사용해 주세요.'}
    if(-not(Test-Path -LiteralPath (Join-Path $environment.RepoRoot 'simulation\apf_engine\apf.py'))){throw 'SDACS 원본 폴더가 없습니다. Configure-Environment.ps1의 RepoRoot를 확인해 주세요.'}
    $healthy=$false;$occupied=$false
    try{$health=Invoke-RestMethod ($environment.BridgeUrl+'/health') -TimeoutSec 1;$occupied=$true;$healthy=$health.source -eq 'swarm-drone-atc' -and $health.protocol_version -eq 2}catch{}
    if($occupied -and -not $healthy){throw '설정된 포트에 다른 버전의 연결 프로그램이 있습니다. 수동 실행은 -ManualOnly를 사용하거나 설정에서 다른 포트를 지정해 주세요.'}
    if(-not $healthy){
        $arguments=@(('"'+$bridge+'"'),'--repo',('"'+$environment.RepoRoot+'"'),'--host','127.0.0.1','--port',([string]$environment.BridgePort))
        Start-Process -FilePath $python -ArgumentList $arguments -WindowStyle Hidden -RedirectStandardOutput (Join-Path $ProjectRoot 'Saved\bridge-integrated.log') -RedirectStandardError (Join-Path $ProjectRoot 'Saved\bridge-integrated-errors.log')
        for($attempt=0;$attempt -lt 30;$attempt++){
            try{$health=Invoke-RestMethod ($environment.BridgeUrl+'/health') -TimeoutSec 1;$healthy=$health.source -eq 'swarm-drone-atc' -and $health.protocol_version -eq 2;if($healthy){break}}catch{}
            Start-Sleep -Milliseconds 200
        }
        if(-not $healthy){throw '자율 연결 시작에 실패했습니다. Saved\bridge-integrated-errors.log를 확인해 주세요.'}
    }
}
$arguments=@('-windowed','-ResX=1600','-ResY=1000','-ForceRes','-NoScreenMessages',('-ImperialBridgeUrl='+$environment.BridgeUrl))
if($Demo){$arguments+='-ImperialDemo'}
Write-Output ('Imperial Wings v'+$manifest.version+' / '+$package)
Start-Process -FilePath $executable -ArgumentList $arguments -WorkingDirectory $package