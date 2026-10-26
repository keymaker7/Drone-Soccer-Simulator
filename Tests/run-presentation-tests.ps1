[CmdletBinding()]
param([string]$EngineRoot='E:\Program Files\UE_5.8',[switch]$Packaged,[string]$PackageRoot)
$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$outputRoot=Join-Path $projectRoot ('Saved\Verification\presentation-'+[DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fff'))
[IO.Directory]::CreateDirectory($outputRoot) | Out-Null
$exe=Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
if($Packaged){
    if(-not $PackageRoot){$PackageRoot=Join-Path $projectRoot 'dist\Windows'}
    $exe=Join-Path $PackageRoot 'ImperialWingsSimulator\Binaries\Win64\ImperialWingsSimulator.exe'
}
foreach($quality in @('Balanced','High','Cinematic')){
    $resultFile=Join-Path $outputRoot ($quality+'.json')
    $logFile=Join-Path $outputRoot ($quality+'.log')
    $arguments=@((Join-Path $projectRoot 'ImperialWingsSimulator.uproject'),'-game','-nullrhi','-nosound','-unattended','-nosplash','-ImperialEffectsSmoke',"-ImperialVisualQuality=$quality","-ImperialEffectsResult=$resultFile","-abslog=$logFile")
    if($Packaged){$arguments=$arguments[1..($arguments.Count-1)]}
    $quoted=@($arguments | ForEach-Object {'"'+$_+'"'})
    $run=Start-Process -FilePath $exe -ArgumentList $quoted -WindowStyle Hidden -PassThru
    if(-not $run.WaitForExit(60000)){Stop-Process -Id $run.Id -ErrorAction SilentlyContinue;throw "Presentation fixture timed out: $quality"}
    if($run.ExitCode -ne 0 -or -not(Test-Path -LiteralPath $resultFile)){throw "Presentation fixture did not complete: $quality"}
    $result=Get-Content -LiteralPath $resultFile -Raw | ConvertFrom-Json
    if(-not $result.ok){throw "Presentation fixture failed: $quality"}
    Write-Output "PASS $quality cosmetic pool capacity=$($result.capacity), remaining=$($result.remaining). Not a flight-physics test."
}
Write-Output "Presentation verification: $outputRoot"
