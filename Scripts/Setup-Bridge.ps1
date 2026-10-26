param([string]$Python='',[string]$RepoRoot='')
$ErrorActionPreference='Stop'
$ProjectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
. (Join-Path $PSScriptRoot 'Environment.ps1')
$environment=Get-ImperialEnvironment -ProjectRoot $ProjectRoot -Python $Python -RepoRoot $RepoRoot
$Python=$environment.Python;$RepoRoot=$environment.RepoRoot
if(-not $Python -or -not(Test-Path -LiteralPath $Python)){throw 'Python 경로가 없습니다. Configure-Environment.ps1로 설정해 주세요.'}
if(-not(Test-Path -LiteralPath (Join-Path $RepoRoot '.git'))){
    git clone 'https://github.com/sun475300-sudo/swarm-drone-atc.git' $RepoRoot
    if($LASTEXITCODE -ne 0){throw 'Unable to clone SDACS source.'}
}
& $Python -m venv (Join-Path $ProjectRoot '.venv')
if($LASTEXITCODE -ne 0){throw 'Unable to create Python environment.'}
& (Join-Path $ProjectRoot '.venv\Scripts\python.exe') -m pip install -r (Join-Path $ProjectRoot 'Bridge\requirements.txt')
if($LASTEXITCODE -ne 0){throw 'Unable to install bridge dependencies.'}
