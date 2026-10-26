[CmdletBinding()]
param([Parameter(Mandatory)][string]$EngineRoot,[Parameter(Mandatory)][string]$Python,[Parameter(Mandatory)][string]$RepoRoot,[int]$BridgePort=8767)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
. (Join-Path $PSScriptRoot 'Environment.ps1')
$environment=Get-ImperialEnvironment -ProjectRoot $root -EngineRoot $EngineRoot -Python $Python -RepoRoot $RepoRoot -BridgePort $BridgePort
if(-not(Test-Path -LiteralPath (Join-Path $environment.EngineRoot 'Engine\Build\BatchFiles\Build.bat'))){throw 'Unreal Engine 설치 폴더를 확인해 주세요.'}
if(-not(Test-Path -LiteralPath $environment.Python -PathType Leaf)){throw 'Python 실행 파일을 확인해 주세요.'}
$value=[ordered]@{engine_root=$environment.EngineRoot;python_exe=$environment.Python;sdacs_root=$environment.RepoRoot;package_root='dist/Integrated/Windows';bridge_port=$environment.BridgePort}
$value | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $root 'SimulatorEnvironment.local.json') -Encoding utf8
Write-Output '이 PC의 실행 경로를 저장했습니다. 다음부터 Build.ps1 / Launch.ps1이 같은 설정을 사용합니다.'
