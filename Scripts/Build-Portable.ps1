[CmdletBinding()]
param([Parameter(Mandatory)][string]$OutputDirectory,[switch]$SkipGameBuild)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
. (Join-Path $PSScriptRoot 'Environment.ps1')
$environment=Get-ImperialEnvironment -ProjectRoot $root
$package=Join-Path $root 'dist\Portable\Windows'
if(-not $SkipGameBuild){& (Join-Path $PSScriptRoot 'Build.ps1') -Package -Configuration Shipping -PackageRoot $package}
$python=Join-Path $root '.venv\Scripts\python.exe'
& $python (Join-Path $PSScriptRoot 'make-portable.py') --package $package --repo $environment.RepoRoot --engine $environment.EngineRoot --output $OutputDirectory
if($LASTEXITCODE -ne 0){throw 'Portable release build failed.'}
