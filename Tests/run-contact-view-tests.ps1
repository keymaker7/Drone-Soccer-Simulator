[CmdletBinding()]
param([switch]$Packaged,[string]$PackageRoot='',[ValidateSet('Contact','View')][string[]]$Scenarios=@('Contact','View'),[int[]]$FrameRates=@(30,60,120))
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
. (Join-Path $root 'Scripts\Environment.ps1')
$environment=Get-ImperialEnvironment -ProjectRoot $root -PackageRoot $PackageRoot
$source=& (Join-Path $root 'Scripts\Get-SourceIdentity.ps1') -ProjectRoot $root
$output=Join-Path $root ('Saved\Verification\contact-view-'+[DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fff'))
New-Item -ItemType Directory -Path $output | Out-Null
if($Packaged){
 $directory=Join-Path $environment.PackageRoot 'ImperialWingsSimulator\Binaries\Win64'
 $exe=Join-Path $directory 'ImperialWingsSimulator-Win64-Shipping.exe'
 if(-not(Test-Path -LiteralPath $exe)){$exe=Join-Path $directory 'ImperialWingsSimulator.exe'}
}else{$exe=Join-Path $environment.EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'}
$reports=@()
foreach($scenario in $Scenarios){
 $rates=if($scenario -eq 'View'){@(60)}else{$FrameRates}
 foreach($rate in $rates){
  if($rate -lt 20 -or $rate -gt 240){throw 'Frame rate must be 20..240'}
  $resultPath=Join-Path $output ($scenario+'-'+$rate+'.json')
  $data=Join-Path $output ($scenario+'-'+$rate+'-userdata');New-Item -ItemType Directory -Path $data | Out-Null
  $arguments=@('-game','-nullrhi','-nosound','-unattended','-nosplash','-ImperialNoWelcome',('-Imperial'+$scenario+'Smoke'),'-UseFixedTimeStep',('-FPS='+$rate),('-ImperialResult='+$resultPath),('-ImperialDataDir='+$data),('-abslog='+(Join-Path $output ($scenario+'-'+$rate+'.log'))))
  if(-not $Packaged){$arguments=@((Join-Path $root 'ImperialWingsSimulator.uproject'))+$arguments}
  $quoted=@($arguments | ForEach-Object {'"'+$_+'"'})
  $p=Start-Process -FilePath $exe -ArgumentList $quoted -WindowStyle Hidden -PassThru
  if(-not $p.WaitForExit(60000)){Stop-Process -Id $p.Id -ErrorAction SilentlyContinue;throw "Scenario timed out: $scenario $rate"}
  if(-not(Test-Path -LiteralPath $resultPath)){throw "No scenario result: $scenario $rate (exit $($p.ExitCode))"}
  $result=Get-Content -LiteralPath $resultPath -Raw | ConvertFrom-Json
  $reports+=@{scenario=$scenario;fps=$rate;result=$result;exit_code=$p.ExitCode}
  $reports | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath (Join-Path $output 'results.json') -Encoding utf8
  if($p.ExitCode -ne 0 -or -not $result.ok -or $result.build.source_sha256 -ne $source.source_sha256){throw "Scenario failed: $resultPath"}
  if([Math]::Abs([double]$result.actual_min_game_dt_s-1.0/$rate) -gt .00001 -or [Math]::Abs([double]$result.actual_max_game_dt_s-1.0/$rate) -gt .00001){throw 'Actual game Tick interval did not match the fixture rate'}
  Write-Output "PASS $scenario $rate FPS: $resultPath"
 }
}
Write-Output "Contact and camera verification: $output"
