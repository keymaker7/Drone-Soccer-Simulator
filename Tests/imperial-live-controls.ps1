[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][int]$ProcessId,
    [string]$PackageRoot='',
    [ValidateSet('F5','F6','F8','Space','Home','1','2','3','C','Escape')][string[]]$Keys=@(),
    [switch]$Activate,
    [switch]$Screenshot
)
$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$process=Get-Process -Id $ProcessId
$allowedRoot=$projectRoot
if($PackageRoot){
 $allowedRoot=[IO.Path]::GetFullPath($PackageRoot).TrimEnd('\')
 $manifest=Get-Content -LiteralPath (Join-Path $allowedRoot 'release-manifest.json') -Raw | ConvertFrom-Json
 $source=& (Join-Path $projectRoot 'Scripts\Get-SourceIdentity.ps1') -ProjectRoot $projectRoot
 if($manifest.build.source_sha256 -ne $source.source_sha256){throw 'Portable package does not match this source snapshot.'}
}
if(-not $process.Path.StartsWith($allowedRoot+'\',[StringComparison]::OrdinalIgnoreCase) -or $process.ProcessName -notlike 'ImperialWingsSimulator*'){throw 'Target is not the explicitly selected project game.'}
if($PackageRoot){
 $relative=$process.Path.Substring($allowedRoot.Length+1).Replace('\','/')
 $entry=@(Get-Content -LiteralPath (Join-Path $allowedRoot 'SHA256SUMS.txt') | Where-Object {($_ -split '  ',2)[1] -eq $relative})
 if($entry.Count -ne 1 -or (Get-FileHash -LiteralPath $process.Path -Algorithm SHA256).Hash.ToLowerInvariant() -ne ($entry[0] -split '  ',2)[0]){throw 'Selected portable game executable failed release checksum verification.'}
}
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
namespace ImperialLive {
 [StructLayout(LayoutKind.Sequential)] public struct Rect { public int Left,Top,Right,Bottom; }
 [StructLayout(LayoutKind.Sequential)] public struct Point { public int X,Y; }
 public static class Native {
 [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
 [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr w);
 [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr w);
 [DllImport("user32.dll")] public static extern bool IsIconic(IntPtr w);
 [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr w,int command);
 [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr w,out uint p);
 [DllImport("kernel32.dll")] public static extern uint GetCurrentThreadId();
 [DllImport("user32.dll")] public static extern bool AttachThreadInput(uint a,uint b,bool c);
 [DllImport("user32.dll")] public static extern void keybd_event(byte k,byte s,uint f,UIntPtr x);
 [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr w,out Rect r);
 [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr w,ref Point p);
 [DllImport("user32.dll")] public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr c);
 }
}
'@
$window=$process.MainWindowHandle
if($window -eq [IntPtr]::Zero){throw 'No game window.'}
function Assert-Focus {
 if([ImperialLive.Native]::GetForegroundWindow() -ne $window){throw 'Game is not in the foreground. No further input sent.'}
}
if($Activate){
 if([ImperialLive.Native]::IsIconic($window)){[void][ImperialLive.Native]::ShowWindow($window,9)}
 [uint32]$owner=0
 $thread=[ImperialLive.Native]::GetWindowThreadProcessId([ImperialLive.Native]::GetForegroundWindow(),[ref]$owner)
 $caller=[ImperialLive.Native]::GetCurrentThreadId()
 [void][ImperialLive.Native]::AttachThreadInput($caller,$thread,$true)
 try{[void][ImperialLive.Native]::BringWindowToTop($window);[void][ImperialLive.Native]::SetForegroundWindow($window)}
 finally{[void][ImperialLive.Native]::AttachThreadInput($caller,$thread,$false)}
 Start-Sleep -Milliseconds 350
}
Assert-Focus
$map=@{F5=0x74;F6=0x75;F8=0x77;Space=0x20;Home=0x24;'1'=0x31;'2'=0x32;'3'=0x33;C=0x43;Escape=0x1b}
$actions=@()
foreach($key in $Keys){
 Assert-Focus
 $flag=if($key -eq 'Home'){1}else{0}
 [ImperialLive.Native]::keybd_event([byte]$map[$key],0,$flag,[UIntPtr]::Zero)
 try{Start-Sleep -Milliseconds 70}finally{[ImperialLive.Native]::keybd_event([byte]$map[$key],0,($flag -bor 2),[UIntPtr]::Zero)}
 $actions+=@{key=$key;utc=[DateTime]::UtcNow.ToString('o')}
 Start-Sleep -Milliseconds 300
}
$run=Join-Path $projectRoot ('Saved\Verification\live-'+[DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fff'))
[void][IO.Directory]::CreateDirectory($run)
@{process=$ProcessId;actions=$actions} | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $run 'actions.json') -Encoding UTF8
if($Screenshot){
 Assert-Focus
 Add-Type -AssemblyName System.Drawing
 $previous=[ImperialLive.Native]::SetThreadDpiAwarenessContext([IntPtr](-4))
 try{
  $rect=[ImperialLive.Rect]::new();$point=[ImperialLive.Point]::new()
  if(-not [ImperialLive.Native]::GetClientRect($window,[ref]$rect) -or -not [ImperialLive.Native]::ClientToScreen($window,[ref]$point)){throw 'Cannot read game bounds.'}
  $bitmap=[Drawing.Bitmap]::new(($rect.Right-$rect.Left),($rect.Bottom-$rect.Top))
  $graphics=[Drawing.Graphics]::FromImage($bitmap)
  try{$graphics.CopyFromScreen($point.X,$point.Y,0,0,$bitmap.Size);$bitmap.Save((Join-Path $run 'screen.png'),[Drawing.Imaging.ImageFormat]::Png)}
  finally{$graphics.Dispose();$bitmap.Dispose()}
 }finally{[void][ImperialLive.Native]::SetThreadDpiAwarenessContext($previous)}
}
Write-Output $run
