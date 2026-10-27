[CmdletBinding()]
param([switch]$Packaged,[string]$PackageRoot='',[int]$FrameRate=60)
$ErrorActionPreference='Stop'
if($FrameRate -notin @(20,30,60,120,240)){throw 'Use a supported test frame rate: 20, 30, 60, 120 or 240.'}
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
. (Join-Path $root 'Scripts\Environment.ps1')
$environment=Get-ImperialEnvironment -ProjectRoot $root -PackageRoot $PackageRoot
if($Packaged){$exe=Join-Path $environment.PackageRoot 'ImperialWingsSimulator\Binaries\Win64\ImperialWingsSimulator-Win64-Shipping.exe'}
else{$exe=Join-Path $environment.EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'}
if(-not(Test-Path -LiteralPath $exe -PathType Leaf)){throw ('Runtime executable not found: '+$exe)}
$identity=& (Join-Path $root 'Scripts\Get-SourceIdentity.ps1') -ProjectRoot $root
$output=Join-Path $root ('Saved\Verification\throttle-response-'+[DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fff')+'-'+[Guid]::NewGuid().ToString('N').Substring(0,8))
New-Item -ItemType Directory -Path $output | Out-Null
$userData=Join-Path $output 'userdata'
New-Item -ItemType Directory -Path $userData | Out-Null
$preferences=Join-Path $userData 'Config\PilotControls.json'
$radioProfile=Join-Path $userData 'Config\RadioProfile.json'

function Assert-SavedPrecision {
    if(-not(Test-Path -LiteralPath $preferences -PathType Leaf)){throw 'PilotControls.json was not saved in the isolated user directory.'}
    $saved=Get-Content -LiteralPath $preferences -Raw | ConvertFrom-Json
    if($saved.version -ne 1 -or $saved.precision_throttle -isnot [bool] -or $saved.precision_throttle -ne $true){throw 'Saved precision_throttle must be the boolean true, with version 1.'}
    if(Test-Path -LiteralPath $radioProfile){throw 'Throttle preference created an unexpected USB calibration profile.'}
}

function Assert-FixtureChecks {
    param($Result,[bool]$Reload,[int]$ExitCode)
    if($ExitCode -ne 0 -or $Result.ok -isnot [bool] -or $Result.ok -ne $true){throw 'Throttle verifier failed; inspect its result JSON and runtime log.'}
    if($Result.reloaded -isnot [bool] -or $Result.reloaded -ne $Reload){throw 'Throttle verifier ran the wrong launch/reload phase.'}
    foreach($field in @('actual_min_game_dt_s','actual_max_game_dt_s')){
        $reported=$Result.$field
        if($null -eq $reported){throw ('Throttle result lacks measured game Tick interval: '+$field)}
        $actual=[double]$reported
        if([double]::IsNaN($actual) -or [double]::IsInfinity($actual) -or [Math]::Abs($actual-1.0/$FrameRate) -gt .00001){throw ('Actual game Tick interval does not match '+$FrameRate+' FPS: '+$field+'='+$reported)}
    }
    if($Result.build.source_sha256 -ne $identity.source_sha256){throw 'Throttle fixture/source identity mismatch.'}
    $rows=@($Result.checks)
    if(@($rows | Where-Object {$_.ok -isnot [bool] -or $_.ok -ne $true}).Count){throw 'Throttle verifier JSON contains a failed or incomplete check.'}
    $required=[ordered]@{'cold launch remains keyboard and disarmed'=1}
    if($Reload){
        $required['precision restored in a second process']=1
        $required['curve change succeeds only while disarmed']=6
        $required['explicit zero raw throttle ARM']=6
        $required['raw throttle remains unmodified']=6
        $required['actual force command follows selected curve']=6
        $required['motor force remains within battery power']=6
        $required['precision reduces actual thrust change around the chosen middle']=1
        $required['precision preserves actual full-throttle output']=1
        $required['high raw throttle still blocks ARM']=1
        $required['native radio never enabled by curve preference']=1
    }else{
        $required['new user defaults to direct throttle']=1
        $required['HUD precision button saves enabled setting']=1
        $required['preference does not create USB calibration']=1
        $required['precision keeps zero and full endpoints']=1
        $required['armed manual aircraft blocks curve change']=1
    }
    foreach($name in $required.Keys){
        if(@($rows | Where-Object {$_.check -eq $name}).Count -ne $required[$name]){throw ('Missing throttle check or scenario coverage: '+$name)}
    }
}

function Get-ExpectedThrottle {
    param([double]$Raw,[bool]$Precision)
    if(-not $Precision){return $Raw}
    $offset=$Raw-.55
    $span=if($Raw -lt .55){.55}else{.45}
    return [Math]::Max(0.0,[Math]::Min(1.0,.55+.65*$offset+.35*$offset*$offset*$offset/($span*$span)))
}

function Assert-FlightTelemetry {
    $flightDirectory=Join-Path $userData 'FlightLogs'
    $files=@(Get-ChildItem -LiteralPath $flightDirectory -File -Filter '*.jsonl' -ErrorAction SilentlyContinue)
    if($files.Count -ne 1){throw 'Expected exactly one F6 flight recording from the reload process.'}
    $records=@(Get-Content -LiteralPath $files[0].FullName | ForEach-Object {if(-not [string]::IsNullOrWhiteSpace($_)){$_ | ConvertFrom-Json}})
    if($records.Count -lt 3 -or $records[0].event -ne 'start' -or $records[-1].event -ne 'stop' -or @($records | Where-Object {$_.event -eq 'sample'}).Count -eq 0){throw 'F6 recording lacks complete start/sample/stop events.'}
    $observed=@{}
    $fixtureSamples=0
    foreach($record in $records){
        if($record.source_sha256 -ne $identity.source_sha256){throw 'F6 recording/source identity mismatch.'}
        if($record.radio_enabled -isnot [bool] -or $record.radio_enabled -ne $false){throw 'A non-USB throttle fixture unexpectedly enabled USB.'}
        if(@($record.aircraft).Count -ne 10){throw 'F6 recording must retain all ten actual aircraft.'}
        foreach($drone in @($record.aircraft | Where-Object {$_.input_source -eq 'VERIFICATION_FIXTURE'})){
            if($drone.precision_throttle -isnot [bool] -or $null -eq $drone.throttle -or $null -eq $drone.throttle_command){throw 'F6 raw throttle, command or precision flag is missing.'}
            $raw=[double]$drone.throttle
            $command=[double]$drone.throttle_command
            if([double]::IsNaN($raw) -or [double]::IsInfinity($raw) -or [double]::IsNaN($command) -or [double]::IsInfinity($command) -or $raw -lt 0 -or $raw -gt 1 -or $command -lt 0 -or $command -gt 1){throw 'F6 throttle values must be finite and within [0,1].'}
            if($drone.autonomous -isnot [bool] -or $drone.autonomous -ne $false){throw 'Manual throttle fixture unexpectedly recorded an autonomous aircraft.'}
            $expected=Get-ExpectedThrottle -Raw $raw -Precision $drone.precision_throttle
            if([Math]::Abs($command-$expected) -gt .00001){throw 'F6 throttle command does not match its unchanged raw input and selected curve.'}
            if($record.event -eq 'sample' -and $drone.armed -eq $true){
                $fixtureSamples++
                foreach($sampleInput in @(.54,.56,1.0)){
                    if([Math]::Abs($raw-$sampleInput) -lt .00001){$observed[([string]$drone.precision_throttle)+'/'+$sampleInput.ToString('F2',[Globalization.CultureInfo]::InvariantCulture)]=$true}
                }
            }
        }
    }
    foreach($precision in @($false,$true)){
        foreach($sampleInput in @(.54,.56,1.0)){
            $key=([string]$precision)+'/'+$sampleInput.ToString('F2',[Globalization.CultureInfo]::InvariantCulture)
            if(-not $observed.ContainsKey($key)){throw ('F6 recording did not observe the armed raw/command scenario '+$key+'; inspect wall-time sample coverage.')}
        }
    }
    return [ordered]@{file=$files[0].FullName;records=$records.Count;armed_fixture_samples=$fixtureSamples;covered_scenarios=$observed.Count}
}

$runs=@()
foreach($reload in @($false,$true)){
    $phase=if($reload){'reload'}else{'default'}
    $resultFile=Join-Path $output ($phase+'-result.json')
    $logFile=Join-Path $output ($phase+'-runtime.log')
    $arguments=@('-game','-nullrhi','-nosound','-unattended','-nosplash','-ImperialNoWelcome','-ImperialThrottleSmoke','-UseFixedTimeStep',('-FPS='+$FrameRate),('-ExecCmds=t.MaxFPS '+$FrameRate),'-ImperialBridgeUrl=http://127.0.0.1:1',('-ImperialThrottleResult='+$resultFile),('-ImperialDataDir='+$userData),('-abslog='+$logFile))
    if($reload){Assert-SavedPrecision;$arguments+='-ImperialThrottleReload'}
    if(-not $Packaged){$arguments=@((Join-Path $root 'ImperialWingsSimulator.uproject'))+$arguments}
    $quoted=@($arguments | ForEach-Object {'"'+$_+'"'})
    $process=Start-Process -FilePath $exe -ArgumentList $quoted -WindowStyle Hidden -PassThru
    if(-not $process.WaitForExit(60000)){
        Stop-Process -Id $process.Id -ErrorAction SilentlyContinue
        [void]$process.WaitForExit(5000)
        throw ('Throttle '+$phase+' fixture timed out: '+$output)
    }
    if(-not(Test-Path -LiteralPath $resultFile -PathType Leaf)){throw ('No '+$phase+' throttle result: '+$output)}
    $result=Get-Content -LiteralPath $resultFile -Raw | ConvertFrom-Json
    Assert-FixtureChecks -Result $result -Reload $reload -ExitCode $process.ExitCode
    Assert-SavedPrecision
    $runs+=[ordered]@{phase=$phase;exit_code=$process.ExitCode;checks=@($result.checks).Count;actual_min_game_dt_s=$result.actual_min_game_dt_s;actual_max_game_dt_s=$result.actual_max_game_dt_s;result=$resultFile;log=$logFile}
    Write-Output ('PASS '+$phase+' throttle preference/actual-body checks: '+@($result.checks).Count+' at verified actual game '+$FrameRate+' FPS')
}
$telemetry=Assert-FlightTelemetry
$finalIdentity=& (Join-Path $root 'Scripts\Get-SourceIdentity.ps1') -ProjectRoot $root
if($finalIdentity.source_sha256 -ne $identity.source_sha256){throw 'Source inputs changed during the two-process throttle test.'}
$summary=[ordered]@{ok=$true;frame_rate=$FrameRate;source_sha256=$identity.source_sha256;user_data=$userData;saved_precision_throttle=$true;runs=$runs;telemetry=$telemetry;scope='Two isolated sequential Unreal processes and explicit non-USB raw throttle fixture; not physical-controller flight, latency or altitude-hold validation.'}
$summaryFile=Join-Path $output 'results.json'
$summary | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $summaryFile -Encoding utf8
Write-Output ('PASS six direct/precision raw-to-command F6 scenarios at verified actual game '+$FrameRate+' FPS')
Write-Output $summaryFile
