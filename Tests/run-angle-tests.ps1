[CmdletBinding()]
param([int[]]$FrameRates=@(20,30,60,120,240))
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$exe=Join-Path $root 'dist\Windows\ImperialWingsSimulator\Binaries\Win64\ImperialWingsSimulator.exe'
$output=Join-Path $root ('Saved\Verification\angle-'+[DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fff'))
New-Item -ItemType Directory -Path $output | Out-Null
$identity=& (Join-Path $root 'Scripts\Get-SourceIdentity.ps1') -ProjectRoot $root
$results=@()
foreach($rate in $FrameRates){
    if($rate -lt 20 -or $rate -gt 240){throw 'FrameRates must be 20..240.'}
    $resultPath=Join-Path $output "angle-$rate.json"
    $arguments=@('-nullrhi','-nosound','-unattended','-nosplash','-ImperialAngleSmoke','-UseFixedTimeStep',"-FPS=$rate","-ImperialResult=$resultPath",('-abslog='+(Join-Path $output "angle-$rate.log")))
    $quoted=@($arguments | ForEach-Object {'"'+$_+'"'})
    $run=Start-Process -FilePath $exe -ArgumentList $quoted -PassThru -Wait -WindowStyle Hidden -RedirectStandardOutput (Join-Path $output "console-$rate.log") -RedirectStandardError (Join-Path $output "stderr-$rate.log")
    if(-not(Test-Path -LiteralPath $resultPath)){throw "No new angle result: $resultPath"}
    $result=Get-Content -LiteralPath $resultPath -Raw | ConvertFrom-Json
    $results+=[ordered]@{fixed_game_fps=$rate;exit_code=$run.ExitCode;result=$result}
    $results | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath (Join-Path $output 'results.json')
    if($run.ExitCode -ne 0 -or -not $result.ok -or $result.build.source_sha256 -ne $identity.source_sha256){throw "Angle physics failed or source mismatch at $rate FPS; inspect $resultPath"}
    Write-Output "PASS $rate FPS: ten manual ANGLE bodies, initial recovery=$($result.initial_self_level), released worst=$($result.released_worst_tilt_deg) deg."
}
Write-Output "Actual angle physics results: $output"
