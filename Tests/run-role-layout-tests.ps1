[CmdletBinding()]
param([string]$EngineRoot='E:\Program Files\UE_5.8',[string]$PackageRoot)
$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$resultDirectory=Join-Path $projectRoot ('Saved\Verification\role-layout-'+[DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fff'))
[IO.Directory]::CreateDirectory($resultDirectory) | Out-Null
$resultFile=Join-Path $resultDirectory 'result.json'
$logFile=Join-Path $resultDirectory 'runtime.log'
$arguments=@('-game','-nullrhi','-nosound','-unattended','-nosplash','-ImperialLayoutSmoke',"-ImperialLayoutResult=$resultFile","-abslog=$logFile")
if($PackageRoot){$exe=Join-Path $PackageRoot 'ImperialWingsSimulator\Binaries\Win64\ImperialWingsSimulator.exe';if(-not(Test-Path -LiteralPath $exe)){$exe=Join-Path $PackageRoot 'ImperialWingsSimulator\Binaries\Win64\ImperialWingsSimulator-Win64-Shipping.exe'}}
else{$exe=Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe';$arguments=@((Join-Path $projectRoot 'ImperialWingsSimulator.uproject'))+$arguments}
$quoted=@($arguments | ForEach-Object {'"'+$_+'"'})
$run=Start-Process -FilePath $exe -ArgumentList $quoted -WindowStyle Hidden -PassThru
if(-not $run.WaitForExit(60000)){Stop-Process -Id $run.Id -ErrorAction SilentlyContinue;throw 'Role-layout verification timed out'}
if($run.ExitCode -ne 0 -or -not(Test-Path -LiteralPath $resultFile)){throw 'Role-layout verification did not complete'}
$result=Get-Content -LiteralPath $resultFile -Raw | ConvertFrom-Json
if(-not $result.ok -or @($result.aircraft).Count -ne 10){throw "Role-layout verification failed: $resultFile"}
Write-Output "PASS ten aircraft: centred Strikers, actual pads/reset/LOS stations, four lower ribbons per Striker/Guide. $resultFile"
