param([string]$Python='E:\Tools\Python312\python.exe',[string]$RepoRoot='E:\GitHub\swarm-drone-atc')
$ErrorActionPreference='Stop'
$ProjectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if(-not(Test-Path -LiteralPath (Join-Path $RepoRoot '.git'))){
    git clone 'https://github.com/sun475300-sudo/swarm-drone-atc.git' $RepoRoot
    if($LASTEXITCODE -ne 0){throw 'Unable to clone SDACS source.'}
}
& $Python -m venv (Join-Path $ProjectRoot '.venv')
if($LASTEXITCODE -ne 0){throw 'Unable to create Python environment.'}
& (Join-Path $ProjectRoot '.venv\Scripts\python.exe') -m pip install -r (Join-Path $ProjectRoot 'Bridge\requirements.txt')
if($LASTEXITCODE -ne 0){throw 'Unable to install bridge dependencies.'}
