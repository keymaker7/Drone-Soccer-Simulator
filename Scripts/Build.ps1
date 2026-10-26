param([string]$EngineRoot='', [switch]$Package,[switch]$GenerateContent,[string]$PackageRoot='',[ValidateSet('Development','Shipping')][string]$Configuration='Development')
$ErrorActionPreference = 'Stop'
$ProjectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$ProjectFile = Join-Path $ProjectRoot 'ImperialWingsSimulator.uproject'
. (Join-Path $PSScriptRoot 'Environment.ps1')
$environment=Get-ImperialEnvironment -ProjectRoot $ProjectRoot -EngineRoot $EngineRoot -PackageRoot $PackageRoot
$EngineRoot=$environment.EngineRoot
if(-not $EngineRoot -or -not(Test-Path -LiteralPath (Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'))){throw 'Unreal 설치 경로가 없습니다. Scripts\Configure-Environment.ps1로 이 PC의 경로를 먼저 설정해 주세요.'}
& (Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat') ImperialWingsSimulatorEditor Win64 Development "-Project=$ProjectFile" -WaitMutex -NoHotReloadFromIDE
if ($LASTEXITCODE -ne 0) { throw 'Editor compilation failed.' }
if($GenerateContent -or -not(Test-Path -LiteralPath (Join-Path $ProjectRoot 'Content\Maps\L_DroneSoccerArena.umap'))){
    & (Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe') $ProjectFile -run=pythonscript "-script=$(Join-Path $PSScriptRoot 'create-content.py')" -unattended -nosplash -nullrhi
    if ($LASTEXITCODE -ne 0) { throw '콘텐츠 생성에 실패했습니다.' }
    & (Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat') ImperialWingsSimulatorEditor Win64 Development "-Project=$ProjectFile" -WaitMutex -NoHotReloadFromIDE
    if ($LASTEXITCODE -ne 0) { throw '콘텐츠 생성 후 빌드 정보 갱신에 실패했습니다.' }
}
if ($Package) {
    if((Split-Path -Leaf $environment.PackageRoot) -ne 'Windows'){throw '패키지 폴더는 Windows로 끝나야 합니다.'}
    $archiveRoot=Split-Path -Parent $environment.PackageRoot
    & (Join-Path $EngineRoot 'Engine\Build\BatchFiles\RunUAT.bat') BuildCookRun "-project=$ProjectFile" -noP4 -platform=Win64 "-clientconfig=$Configuration" -build -cook -stage -pak -iostore -package -prereqs -archive "-archivedirectory=$archiveRoot" -unattended -utf8output
    if ($LASTEXITCODE -ne 0) { throw 'Packaging failed.' }
    Write-Output (Join-Path $environment.PackageRoot 'ImperialWingsSimulator.exe')
}
