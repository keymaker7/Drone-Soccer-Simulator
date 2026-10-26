param([string]$EngineRoot = 'E:\Program Files\UE_5.8', [switch]$Package)
$ErrorActionPreference = 'Stop'
$ProjectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$ProjectFile = Join-Path $ProjectRoot 'ImperialWingsSimulator.uproject'
& (Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat') ImperialWingsSimulatorEditor Win64 Development "-Project=$ProjectFile" -WaitMutex -NoHotReloadFromIDE
if ($LASTEXITCODE -ne 0) { throw 'Editor compilation failed.' }
& (Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe') $ProjectFile -run=pythonscript "-script=$(Join-Path $PSScriptRoot 'create-content.py')" -unattended -nosplash -nullrhi
if ($LASTEXITCODE -ne 0) { throw 'Content creation failed.' }
if ($Package) {
    & (Join-Path $EngineRoot 'Engine\Build\BatchFiles\RunUAT.bat') BuildCookRun "-project=$ProjectFile" -noP4 -platform=Win64 -clientconfig=Development -build -cook -stage -pak -iostore -package -archive "-archivedirectory=$(Join-Path $ProjectRoot 'dist')" -unattended -utf8output
    if ($LASTEXITCODE -ne 0) { throw 'Packaging failed.' }
}
