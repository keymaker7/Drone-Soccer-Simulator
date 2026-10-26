<#
.SYNOPSIS
Creates a source-only ZIP without Unreal/Python build output or downloaded references.
.DESCRIPTION
Does not require Git and does not change the index, commits, remotes, or source files.
Keeps authored Content assets and ArtSource textures. Every copied file is hashed;
the completed ZIP is reopened and verified against its SHA256SUMS.txt manifest.
.EXAMPLE
.\Tests\package-source.ps1
.EXAMPLE
.\Tests\package-source.ps1 -OutputPath .\Saved\SourcePackages\Drone-Soccer-Simulator-source.zip
#>
[CmdletBinding()]
param(
    [string]$ProjectRoot = (Split-Path -Parent $PSScriptRoot),
    [string]$OutputPath = ''
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression

$sourceRoot = [IO.Path]::GetFullPath($ProjectRoot).TrimEnd([IO.Path]::DirectorySeparatorChar)
if (-not (Test-Path -LiteralPath (Join-Path $sourceRoot 'ImperialWingsSimulator.uproject') -PathType Leaf)) {
    throw 'ProjectRoot must contain ImperialWingsSimulator.uproject.'
}
if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $OutputPath = Join-Path $sourceRoot ('Saved\SourcePackages\Drone-Soccer-Simulator-source-' + [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmssfffZ') + '.zip')
}
$archivePath = [IO.Path]::GetFullPath($OutputPath)
if ([IO.Path]::GetExtension($archivePath) -ne '.zip') { throw 'OutputPath must end with .zip.' }
if (Test-Path -LiteralPath $archivePath) { throw 'OutputPath already exists; choose a new archive name.' }
$prefix = 'Drone-Soccer-Simulator/'
$rootPrefix = $sourceRoot + [IO.Path]::DirectorySeparatorChar
$sourceFolders = @('Source','Config','Content','ArtSource','Bridge','Scripts','Tests','Plugins','docs','.github')
$excludedDirectories = @('.git','.venv','.vs','.idea','Binaries','Build','Intermediate','DerivedDataCache','Saved','dist','node_modules','__pycache__','References','Verification')
$excludedExtensions = @('.exe','.dll','.pdb','.obj','.lib','.exp','.ilk','.idb','.iobj','.ipdb','.pyc','.pyo','.pyd','.log','.pdf','.zip','.7z','.rar','.pem','.pfx','.p12','.key','.blend1')
$rootExtensions = @('.uproject','.cmd','.bat','.ps1','.py','.md','.txt','.json','.yml','.yaml','.toml','.ini')
$rootSpecialNames = @('.gitignore','.gitattributes','.editorconfig','.env.example','LICENSE','NOTICE')
$sensitivePattern = '(gh[pousr]_[A-Za-z0-9_]{20,}|github_pat_[A-Za-z0-9_]{20,}|sk-(?:proj-)?[A-Za-z0-9_-]{20,}|AKIA[0-9A-Z]{16}|-----BEGIN [A-Z ]*PRIVATE KEY-----)'
$textExtensions = @('.h','.hpp','.cpp','.c','.cs','.py','.ps1','.cmd','.bat','.md','.txt','.json','.ini','.yml','.yaml','.toml','.uproject')
$files = [Collections.Generic.List[IO.FileInfo]]::new()
$pending = [Collections.Generic.Stack[string]]::new()
$pending.Push($sourceRoot)
while ($pending.Count -gt 0) {
    $directory = $pending.Pop()
    foreach ($item in Get-ChildItem -LiteralPath $directory -Force) {
        # Never traverse or archive junctions/symlinks into an external tree.
        if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { continue }
        $absolute = [IO.Path]::GetFullPath($item.FullName)
        if (-not $absolute.StartsWith($rootPrefix,[StringComparison]::OrdinalIgnoreCase)) {
            throw 'Source enumeration escaped ProjectRoot.'
        }
        $relative = $absolute.Substring($rootPrefix.Length)
        if ($item.PSIsContainer) {
            if ($excludedDirectories -contains $item.Name) { continue }
            if ($directory -eq $sourceRoot -and $sourceFolders -notcontains $item.Name) { continue }
            if ($relative -match '^Tests[\\/]build(?:-rules|-radio|-physics)?$') { continue }
            if ($relative -match '^Content[\\/]Developers(?:[\\/]|$)') { continue }
            $pending.Push($absolute)
            continue
        }
        if ($excludedExtensions -contains $item.Extension) { continue }
        if (($item.Name -eq '.env' -or $item.Name -like '.env.*') -and $item.Name -ne '.env.example') { continue }
        if ($item.Name -in @('credentials.json','id_rsa','id_ed25519','SimulatorEnvironment.local.json','SHA256SUMS.txt')) { continue }
        if ($directory -eq $sourceRoot -and $rootExtensions -notcontains $item.Extension -and $rootSpecialNames -notcontains $item.Name) { continue }
        if ($textExtensions -contains $item.Extension) {
            $text = [IO.File]::ReadAllText($absolute)
            if ([regex]::IsMatch($text,$sensitivePattern)) {
                # Report the file only, never any matching credential content.
                throw "Possible credential/private-key material in source file: $relative"
            }
        }
        $files.Add($item)
    }
}
$orderedFiles = @($files | Sort-Object FullName)
if ($orderedFiles.Count -eq 0) { throw 'No project source files were found.' }
[void][IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($archivePath))
$manifest = [Collections.Generic.Dictionary[string,string]]::new([StringComparer]::Ordinal)
$archiveCreated = $false
try {
    $archiveStream = [IO.File]::Open($archivePath,[IO.FileMode]::CreateNew,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)
    $archiveCreated = $true
    try {
        $zip = [IO.Compression.ZipArchive]::new($archiveStream,[IO.Compression.ZipArchiveMode]::Create,$true)
        try {
            $buffer = [byte[]]::new(65536)
            foreach ($file in $orderedFiles) {
                $relative = $file.FullName.Substring($rootPrefix.Length).Replace('\','/')
                $entry = $zip.CreateEntry($prefix+$relative,[IO.Compression.CompressionLevel]::Optimal)
                $input = [IO.File]::Open($file.FullName,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::ReadWrite)
                $output = $entry.Open()
                $hash = [Security.Cryptography.SHA256]::Create()
                try {
                    while (($count = $input.Read($buffer,0,$buffer.Length)) -gt 0) {
                        $output.Write($buffer,0,$count)
                        [void]$hash.TransformBlock($buffer,0,$count,$buffer,0)
                    }
                    [void]$hash.TransformFinalBlock([byte[]]::new(0),0,0)
                    $manifest.Add($relative,[BitConverter]::ToString($hash.Hash).Replace('-','').ToLowerInvariant())
                } finally { $hash.Dispose();$output.Dispose();$input.Dispose() }
            }
            $manifestEntry = $zip.CreateEntry($prefix+'SHA256SUMS.txt',[IO.Compression.CompressionLevel]::Optimal)
            $manifestStream = $manifestEntry.Open()
            try {
                $lines = @($manifest.Keys | Sort-Object | ForEach-Object { $manifest[$_]+'  '+$_ })
                $bytes = [Text.UTF8Encoding]::new($false).GetBytes(($lines -join "`n")+"`n")
                $manifestStream.Write($bytes,0,$bytes.Length)
            } finally { $manifestStream.Dispose() }
        } finally { $zip.Dispose() }
    } finally { $archiveStream.Dispose() }

    $readStream = [IO.File]::OpenRead($archivePath)
    try {
        $readZip = [IO.Compression.ZipArchive]::new($readStream,[IO.Compression.ZipArchiveMode]::Read,$true)
        try {
            if ($readZip.Entries.Count -ne $manifest.Count+1) { throw 'ZIP entry count does not match the manifest.' }
            foreach ($pair in $manifest.GetEnumerator()) {
                $entry = $readZip.GetEntry($prefix+$pair.Key)
                if (-not $entry) { throw "Missing ZIP entry: $($pair.Key)" }
                $stream = $entry.Open();$hash = [Security.Cryptography.SHA256]::Create()
                try { $actual = [BitConverter]::ToString($hash.ComputeHash($stream)).Replace('-','').ToLowerInvariant() }
                finally { $hash.Dispose();$stream.Dispose() }
                if ($actual -cne $pair.Value) { throw "ZIP hash mismatch: $($pair.Key)" }
            }
        } finally { $readZip.Dispose() }
    } finally { $readStream.Dispose() }
} catch {
    if ($archiveCreated -and (Test-Path -LiteralPath $archivePath)) { Remove-Item -LiteralPath $archivePath -Force }
    throw
}
$archiveItem = Get-Item -LiteralPath $archivePath
[pscustomobject]@{
    Archive = $archivePath
    FileCount = $manifest.Count
    Bytes = $archiveItem.Length
    SHA256 = (Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash.ToLowerInvariant()
    AuthoredContentFiles = @($manifest.Keys | Where-Object { $_ -like 'Content/*' }).Count
    ArtSourceFiles = @($manifest.Keys | Where-Object { $_ -like 'ArtSource/*' }).Count
    HashVerification = 'All source entries verified after reopening ZIP.'
}
