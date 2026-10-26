[CmdletBinding()]
param([string]$EngineRoot='E:\Program Files\UE_5.8',[int[]]$FrameRates=@(30,60,120),[switch]$Packaged,[string]$ExecutablePath='')
$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$output=Join-Path $projectRoot ('Saved\Verification\runtime-'+[DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fff'))
New-Item -ItemType Directory -Path $output -Force | Out-Null
$allResults=@()
$sourceIdentity=& (Join-Path $projectRoot 'Scripts\Get-SourceIdentity.ps1') -ProjectRoot $projectRoot
foreach($rate in $FrameRates){
    if($rate -lt 20 -or $rate -gt 240){throw 'FrameRates must be between 20 and 240.'}
    $resultFile=Join-Path $output "physics-$rate.json"
    $logFile=Join-Path $output "physics-$rate.log"
    $gameArgs=@('/Game/Maps/L_DroneSoccerArena','-game','-nullrhi','-nosound','-unattended','-nosplash','-ImperialPhysicsSmoke','-UseFixedTimeStep',"-FPS=$rate","-ImperialResult=$resultFile","-abslog=$logFile")
    if($Packaged){. (Join-Path $projectRoot 'Scripts\Environment.ps1');$environment=Get-ImperialEnvironment -ProjectRoot $projectRoot;$exe=Join-Path $environment.PackageRoot 'ImperialWingsSimulator\Binaries\Win64\ImperialWingsSimulator.exe'}
    else{$exe=Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe';$gameArgs=@((Join-Path $projectRoot 'ImperialWingsSimulator.uproject'))+$gameArgs}
    if($ExecutablePath){if(-not $Packaged){throw 'ExecutablePath requires -Packaged.'};$exe=[IO.Path]::GetFullPath($ExecutablePath)}
    if(-not(Test-Path -LiteralPath $exe)){throw "Runtime executable not found: $exe"}
    $quotedArgs=@($gameArgs | ForEach-Object { '"'+$_+'"' })
    $process=Start-Process -FilePath $exe -ArgumentList $quotedArgs -Wait -PassThru -WindowStyle Hidden -RedirectStandardOutput (Join-Path $output "console-$rate.log") -RedirectStandardError (Join-Path $output "stderr-$rate.log")
    $runExit=$process.ExitCode
    if(-not(Test-Path -LiteralPath $resultFile)){throw "No new physics result was produced at $rate FPS; inspect $logFile"}
    $result=Get-Content -LiteralPath $resultFile -Raw | ConvertFrom-Json
    if([Math]::Abs([double]$result.actual_min_game_dt_s-1.0/$rate) -gt .00001 -or [Math]::Abs([double]$result.actual_max_game_dt_s-1.0/$rate) -gt .00001){throw "Actual engine Tick interval did not match requested $rate FPS."}
    if(-not $result.build -or $result.build.source_sha256 -ne $sourceIdentity.source_sha256){throw "Runtime build does not match current source inputs: $resultFile"}
    $allResults+=[ordered]@{fixed_game_fps=$rate;exit_code=$runExit;result=$result;log=$logFile}
    if($runExit -ne 0 -or -not $result.ok){$allResults|ConvertTo-Json -Depth 8|Set-Content (Join-Path $output 'results.json');throw "Physics scenario failed at $rate FPS: $resultFile"}
    Write-Output "PASS $rate FPS: hover error $($result.hover_error_cm) cm, net $($result.net_peak_m) m, physical goal $($result.striker_physical_goal)"
}
$allResults|ConvertTo-Json -Depth 8|Set-Content (Join-Path $output 'results.json')
Write-Output "Runtime verification: $output"
