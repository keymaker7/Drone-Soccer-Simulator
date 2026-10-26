[CmdletBinding()]
param([int[]]$FrameRates=@(20,30,60,120,240),[string]$PackageRoot='')
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
. (Join-Path $root 'Scripts\Environment.ps1')
$environment=Get-ImperialEnvironment -ProjectRoot $root -PackageRoot $PackageRoot
$exe=Join-Path $environment.PackageRoot 'ImperialWingsSimulator\Binaries\Win64\ImperialWingsSimulator.exe'
    if(-not(Test-Path -LiteralPath $exe)){$exe=$exe.Replace('ImperialWingsSimulator.exe','ImperialWingsSimulator-Win64-Shipping.exe')}
$output=Join-Path $root ('Saved\Verification\angle-'+[DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fff'))
New-Item -ItemType Directory -Path $output | Out-Null
$identity=& (Join-Path $root 'Scripts\Get-SourceIdentity.ps1') -ProjectRoot $root
$buildPath=Join-Path (Split-Path -Parent $exe) 'ImperialBuildIdentity-ImperialWingsSimulator.json'
if(-not(Test-Path -LiteralPath $buildPath) -or (Get-Content -LiteralPath $buildPath -Raw | ConvertFrom-Json).source_sha256 -ne $identity.source_sha256){throw 'Selected package differs from the current source. Pass -PackageRoot pointing to the newly built Windows directory.'}
$results=@()
foreach($rate in $FrameRates){
    if($rate -lt 20 -or $rate -gt 240){throw 'FrameRates must be 20..240.'}
    $resultPath=Join-Path $output "angle-$rate.json"
    $arguments=@('-nullrhi','-nosound','-unattended','-nosplash','-ImperialAngleSmoke','-UseFixedTimeStep',"-FPS=$rate","-ImperialResult=$resultPath",('-abslog='+(Join-Path $output "angle-$rate.log")))
    $quoted=@($arguments | ForEach-Object {'"'+$_+'"'})
    $run=Start-Process -FilePath $exe -ArgumentList $quoted -PassThru -Wait -WindowStyle Hidden -RedirectStandardOutput (Join-Path $output "console-$rate.log") -RedirectStandardError (Join-Path $output "stderr-$rate.log")
    if(-not(Test-Path -LiteralPath $resultPath)){throw "No new angle result: $resultPath"}
    $result=Get-Content -LiteralPath $resultPath -Raw | ConvertFrom-Json
    if([Math]::Abs([double]$result.actual_min_game_dt_s-1.0/$rate) -gt .00001 -or [Math]::Abs([double]$result.actual_max_game_dt_s-1.0/$rate) -gt .00001){throw "Actual engine Tick interval did not match requested $rate FPS."}
    $results+=[ordered]@{fixed_game_fps=$rate;exit_code=$run.ExitCode;result=$result}
    $results | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath (Join-Path $output 'results.json')
    if($run.ExitCode -ne 0 -or -not $result.ok -or $result.build.source_sha256 -ne $identity.source_sha256){throw "Angle physics failed or source mismatch at $rate FPS; inspect $resultPath"}
    Write-Output "PASS $rate FPS: ten manual ANGLE bodies, initial recovery=$($result.initial_self_level), released worst=$($result.released_worst_tilt_deg) deg."
}
Write-Output "Actual angle physics results: $output"
