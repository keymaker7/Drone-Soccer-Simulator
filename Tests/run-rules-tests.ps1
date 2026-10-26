$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$testBuild = Join-Path $PSScriptRoot 'build-rules'
New-Item -ItemType Directory -Path $testBuild -Force | Out-Null
$vswhere = 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
$visualStudio = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $visualStudio) { throw 'Visual Studio C++ Build Tools are required.' }
$developerEnvironment = Join-Path $visualStudio 'VC\Auxiliary\Build\vcvars64.bat'
$sourceRoot = Join-Path $projectRoot 'Source\ImperialWingsSimulator'
$compilerCommand = 'call "{0}" >nul && cl.exe /nologo /std:c++17 /EHsc /W4 /O2 /I"{1}" "{2}" "{3}" /Fe:"{4}" /Fo:"{5}\\" /Fd:"{6}"' -f $developerEnvironment,$sourceRoot,(Join-Path $sourceRoot 'DroneSoccerRules.cpp'),(Join-Path $PSScriptRoot 'soccer_rules_tests.cpp'),(Join-Path $testBuild 'soccer_rules_tests.exe'),$testBuild,(Join-Path $testBuild 'soccer_rules_tests.pdb')
& cmd.exe /d /s /c $compilerCommand
if ($LASTEXITCODE -ne 0) { throw "Rules tests failed to compile (exit $LASTEXITCODE)." }
& (Join-Path $testBuild 'soccer_rules_tests.exe')
if ($LASTEXITCODE -ne 0) { throw "Rules tests failed (exit $LASTEXITCODE)." }
