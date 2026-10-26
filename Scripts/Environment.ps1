function Get-ImperialEnvironment {
    param([string]$ProjectRoot,[string]$EngineRoot,[string]$Python,[string]$RepoRoot,[string]$PackageRoot,[int]$BridgePort=0)
    $root=[IO.Path]::GetFullPath($ProjectRoot)
    $settings=@{}
    foreach($file in @((Join-Path $root 'Config\Environment.example.json'),(Join-Path $root 'SimulatorEnvironment.local.json'))){
        if(Test-Path -LiteralPath $file){
            $value=Get-Content -LiteralPath $file -Raw | ConvertFrom-Json
            foreach($item in $value.PSObject.Properties){$settings[$item.Name]=$item.Value}
        }
    }
    if(-not $EngineRoot){$EngineRoot=$env:IMPERIAL_UE_ROOT}
    if(-not $EngineRoot){$EngineRoot=$settings.engine_root}
    if(-not $Python){$Python=$env:IMPERIAL_PYTHON}
    if(-not $Python){$Python=$settings.python_exe}
    if(-not $RepoRoot){$RepoRoot=$env:SDACS_REPO}
    if(-not $RepoRoot){$RepoRoot=$settings.sdacs_root}
    if(-not $PackageRoot){$PackageRoot=$settings.package_root}
    if(-not $BridgePort){$BridgePort=[int]$settings.bridge_port}
    if(-not $BridgePort){$BridgePort=8767}
    if($BridgePort -lt 1024 -or $BridgePort -gt 65535){throw '자율 연결 포트는 1024~65535여야 합니다.'}
    $resolve={param($path) if([string]::IsNullOrWhiteSpace($path)){return ''};[IO.Path]::GetFullPath($(if([IO.Path]::IsPathRooted($path)){$path}else{Join-Path $root $path}))}
    [pscustomobject]@{
        ProjectRoot=$root;EngineRoot=(& $resolve $EngineRoot);Python=(& $resolve $Python)
        RepoRoot=(& $resolve $RepoRoot);PackageRoot=(& $resolve $PackageRoot);BridgePort=$BridgePort
        BridgeUrl="http://127.0.0.1:$BridgePort"
    }
}
