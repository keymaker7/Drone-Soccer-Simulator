[CmdletBinding()]
param([switch]$Packaged,[string]$PackageRoot='',[switch]$RenderCapture,[switch]$ExpectFailure,[int]$FrameRate=60)
$ErrorActionPreference='Stop'
if($FrameRate -notin @(20,30,60,120,240)){throw 'Use a supported test frame rate.'}
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
. (Join-Path $root 'Scripts\Environment.ps1')
$environment=Get-ImperialEnvironment -ProjectRoot $root -PackageRoot $PackageRoot
$output=Join-Path $root ('Saved\Verification\power-'+[DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fff')+'-'+[Guid]::NewGuid().ToString('N').Substring(0,8))
New-Item -ItemType Directory -Path $output | Out-Null
$resultFile=Join-Path $output 'result.json'
$arguments=@('-game','-nullrhi','-nosound','-unattended','-nosplash','-ImperialNoWelcome','-ImperialPowerSmoke','-UseFixedTimeStep',('-FPS='+$FrameRate),'-ImperialBridgeUrl=http://127.0.0.1:1',('-ImperialPowerResult='+$resultFile),('-ImperialDataDir='+(Join-Path $output 'userdata')),('-abslog='+(Join-Path $output 'runtime.log')))
if($RenderCapture){$arguments=@($arguments | Where-Object {$_ -ne '-nullrhi'})+@('-RenderOffscreen','-ResX=1600','-ResY=1000',('-ImperialPowerCapture='+(Join-Path $output 'power-limit.png')))}
if($Packaged){$exe=Join-Path $environment.PackageRoot 'ImperialWingsSimulator\Binaries\Win64\ImperialWingsSimulator-Win64-Shipping.exe'}
else{$exe=Join-Path $environment.EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe';$arguments=@((Join-Path $root 'ImperialWingsSimulator.uproject'))+$arguments}
$quoted=@($arguments | ForEach-Object {'"'+$_+'"'})
$p=Start-Process -FilePath $exe -ArgumentList $quoted -WindowStyle Hidden -PassThru
if(-not $p.WaitForExit(60000)){Stop-Process -Id $p.Id;throw 'Power fixture timed out'}
if(-not(Test-Path -LiteralPath $resultFile)){throw ('No power result: '+$output)}
$result=Get-Content -LiteralPath $resultFile -Raw | ConvertFrom-Json
foreach($field in @('actual_min_game_dt_s','actual_max_game_dt_s')){
 $reported=$result.$field
 if($null -eq $reported){throw ('Power result lacks measured game Tick interval: '+$field)}
 $actual=[double]$reported
 if([double]::IsNaN($actual) -or [double]::IsInfinity($actual) -or [Math]::Abs($actual-1.0/$FrameRate) -gt .00001){throw ('Actual game Tick interval does not match '+$FrameRate+' FPS: '+$field+'='+$reported)}
}
$identity=& (Join-Path $root 'Scripts\Get-SourceIdentity.ps1') -ProjectRoot $root
if($result.build.source_sha256 -ne $identity.source_sha256){throw 'Power fixture/source identity mismatch'}
$failed=@($result.scenarios | Where-Object {-not $_.ok})
if($ExpectFailure){if($result.ok -or $failed.Count -eq 0){throw 'Expected power-budget failure not reproduced'};Write-Output ('Expected baseline failures: '+($failed.scenario -join ', '))}
else{
 if($p.ExitCode -ne 0 -or -not $result.ok -or $result.scenarios.Count -ne 6){$failed | Format-Table;throw ('Power verification failed: '+$resultFile)}
 if($RenderCapture -and -not(Test-Path -LiteralPath (Join-Path $output 'power-limit.png'))){throw 'No rendered power warning'}
 Write-Output ('PASS 6 actual-body power budget scenarios at verified actual game '+$FrameRate+' FPS')
}
Write-Output $resultFile
