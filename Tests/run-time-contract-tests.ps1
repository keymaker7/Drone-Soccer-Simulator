[CmdletBinding()]
param([switch]$Packaged,[string]$PackageRoot='',[switch]$Hitches,[switch]$Official,[switch]$RenderCapture,[switch]$ExpectFailure)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
. (Join-Path $root 'Scripts\Environment.ps1')
$environment=Get-ImperialEnvironment -ProjectRoot $root -PackageRoot $PackageRoot
$output=Join-Path $root ('Saved\Verification\time-contract-'+[DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fff')+'-'+[Guid]::NewGuid().ToString('N').Substring(0,8))
New-Item -ItemType Directory -Path $output | Out-Null
$resultFile=Join-Path $output 'result.json'
$arguments=@('-game','-nullrhi','-nosound','-unattended','-nosplash','-ImperialNoWelcome','-ImperialTimeSmoke','-ImperialBridgeUrl=http://127.0.0.1:1',('-ImperialTimeResult='+$resultFile),('-ImperialDataDir='+(Join-Path $output 'userdata')),('-abslog='+(Join-Path $output 'runtime.log')))
if($Hitches){$arguments+='-ImperialTimeHitches'}
if($Official){$arguments+='-ImperialTimeOfficial'}
if($RenderCapture){$arguments=@($arguments | Where-Object {$_ -ne '-nullrhi'})+@('-RenderOffscreen','-ResX=1600','-ResY=1000',('-ImperialTimeCapture='+(Join-Path $output 'time-warning.png')))}
if($Packaged){$exe=Join-Path $environment.PackageRoot 'ImperialWingsSimulator\Binaries\Win64\ImperialWingsSimulator-Win64-Shipping.exe'}
else{$exe=Join-Path $environment.EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe';$arguments=@((Join-Path $root 'ImperialWingsSimulator.uproject'))+$arguments}
$quoted=@($arguments | ForEach-Object {'"'+$_+'"'})
$p=Start-Process -FilePath $exe -ArgumentList $quoted -WindowStyle Hidden -PassThru
$deadline=[DateTime]::UtcNow.AddSeconds(105)
while(-not $p.HasExited -and [DateTime]::UtcNow -lt $deadline){$null=$p.WaitForExit(1000)}
if(-not $p.HasExited){Stop-Process -Id $p.Id;throw 'Time contract fixture timed out'}
if(-not(Test-Path -LiteralPath $resultFile)){throw ('No result: '+$output)}
$result=Get-Content -LiteralPath $resultFile -Raw | ConvertFrom-Json
$identity=& (Join-Path $root 'Scripts\Get-SourceIdentity.ps1') -ProjectRoot $root
if($result.build.source_sha256 -ne $identity.source_sha256){throw 'Time fixture/source identity mismatch'}
if([bool]$result.hitches -ne $Hitches.IsPresent -or [bool]$result.official_manual_match -ne $Official.IsPresent){throw 'Time fixture scenario/result mismatch'}
if(-not $ExpectFailure){
 if($Hitches -and (-not $result.injected_100ms -or -not $result.injected_250ms -or @($result.frames | Where-Object {$_.game_dt_s -gt .09}).Count -lt 2 -or @($result.frames | Where-Object {$_.game_dt_s -gt .24}).Count -lt 1)){throw 'Actual 100/250ms hitches were not observed'}
 $logs=@(Get-ChildItem -LiteralPath (Join-Path $output 'userdata\FlightLogs') -Filter '*.jsonl')
 if($logs.Count -ne 1){throw 'Expected one real flight log'}
 $lines=@(Get-Content -LiteralPath $logs[0].FullName | ForEach-Object {$_ | ConvertFrom-Json})
 if($lines[0].event -ne 'start' -or $lines[-1].event -ne 'stop' -or @($lines | Where-Object {$_.aircraft.Count -ne 10 -or -not $_.timing -or $_.timing.deferred_physics_s -ne 0 -or $_.timing.policy -ne 'sync_chaos_budget_discard_excess_no_catchup'}).Count -ne 0){throw 'Flight log timing schema failed'}
 if($lines[-1].timing.physics_budget_s - $lines[0].timing.physics_budget_s -lt 59){throw 'Flight log time did not advance'}
 if($RenderCapture -and -not(Test-Path -LiteralPath (Join-Path $output 'time-warning.png'))){throw 'No rendered time warning image'}
}
if($ExpectFailure){if($result.ok -or $result.clocks_ok){throw 'Expected clock mismatch was not reproduced'};Write-Output 'Expected time mismatch reproduced'}
elseif($p.ExitCode -ne 0 -or -not $result.ok){throw ('Time contract failed: '+$resultFile)}
else{Write-Output 'PASS 60-second ten-body real-time clock and stability audit'}
Write-Output $resultFile
