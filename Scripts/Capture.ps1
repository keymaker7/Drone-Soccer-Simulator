param([int]$ProcessId=0,[string]$OutputFile='')
$ErrorActionPreference='Stop'
$ProjectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if(-not $OutputFile){$OutputFile=Join-Path $ProjectRoot 'Saved\Verification\arena.png'}
if(-not $ProcessId){$ProcessId=(Get-Process UnrealEditor,ImperialWingsSimulator -ErrorAction SilentlyContinue | Where-Object {$_.MainWindowTitle -like '*Imperial*' -and $_.MainWindowHandle -ne 0} | Select-Object -First 1).Id}
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class IWNativeCapture {
 [StructLayout(LayoutKind.Sequential)]public struct Rect{public int L,T,R,B;}
 [StructLayout(LayoutKind.Sequential)]public struct Point{public int X,Y;}
 [DllImport("user32.dll")]public static extern bool GetClientRect(IntPtr h,out Rect r);
 [DllImport("user32.dll")]public static extern bool ClientToScreen(IntPtr h,ref Point p);
 [DllImport("user32.dll")]public static extern bool SetForegroundWindow(IntPtr h);
 [DllImport("user32.dll")]public static extern IntPtr GetForegroundWindow();
 [DllImport("user32.dll")]public static extern bool SetWindowPos(IntPtr h,IntPtr after,int x,int y,int cx,int cy,uint flags);
 [DllImport("user32.dll")]public static extern uint GetWindowThreadProcessId(IntPtr h,out uint p);
 [DllImport("user32.dll")]public static extern bool AttachThreadInput(uint a,uint b,bool attach);
 [DllImport("kernel32.dll")]public static extern uint GetCurrentThreadId();
 [DllImport("user32.dll")]public static extern bool ShowWindow(IntPtr h,int c);
 [DllImport("user32.dll")]public static extern bool SetProcessDPIAware();
}
'@
[void][IWNativeCapture]::SetProcessDPIAware()
$Process=Get-Process -Id $ProcessId
$Handle=$Process.MainWindowHandle
if($Handle -eq 0){throw 'The selected process has no visible game window.'}
[void][IWNativeCapture]::ShowWindow($Handle,9)
[void][IWNativeCapture]::SetWindowPos($Handle,[IntPtr]::new(-1),0,0,0,0,0x43)
$Foreground=[IWNativeCapture]::GetForegroundWindow()
$ForegroundPid=[uint32]0
$ForegroundThread=[IWNativeCapture]::GetWindowThreadProcessId($Foreground,[ref]$ForegroundPid)
$CurrentThread=[IWNativeCapture]::GetCurrentThreadId()
[void][IWNativeCapture]::AttachThreadInput($CurrentThread,$ForegroundThread,$true)
[void][IWNativeCapture]::SetForegroundWindow($Handle)
[void][IWNativeCapture]::AttachThreadInput($CurrentThread,$ForegroundThread,$false)
Start-Sleep -Milliseconds 250
[void][IWNativeCapture]::SetWindowPos($Handle,[IntPtr]::new(-2),0,0,0,0,0x13)
if([IWNativeCapture]::GetForegroundWindow() -ne $Handle){throw 'The game window is not foreground; no screenshot was captured.'}
$Rect=[IWNativeCapture+Rect]::new();$Point=[IWNativeCapture+Point]::new()
[void][IWNativeCapture]::GetClientRect($Handle,[ref]$Rect)
[void][IWNativeCapture]::ClientToScreen($Handle,[ref]$Point)
$Bitmap=[Drawing.Bitmap]::new($Rect.R,$Rect.B)
$Graphics=[Drawing.Graphics]::FromImage($Bitmap)
$Graphics.CopyFromScreen($Point.X,$Point.Y,0,0,[Drawing.Size]::new($Rect.R,$Rect.B))
New-Item -ItemType Directory -Path (Split-Path -Parent $OutputFile) -Force | Out-Null
$Bitmap.Save($OutputFile,[Drawing.Imaging.ImageFormat]::Png)
$Graphics.Dispose();$Bitmap.Dispose()
Write-Output $OutputFile
