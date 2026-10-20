param([Parameter(Mandatory)][string]$Exe,[Parameter(Mandatory)][string]$Screenshot)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class NativeUi {
 [StructLayout(LayoutKind.Sequential)] public struct Rect {public int Left,Top,Right,Bottom;}
 [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h,out Rect r);
 [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr h,int id);
 [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h,uint m,IntPtr w,IntPtr l);
 [DllImport("user32.dll")] public static extern int GetWindowLong(IntPtr h,int index);
 [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h,IntPtr dc,uint flags);
}
'@
$p = Start-Process -FilePath (Resolve-Path $Exe) -ArgumentList '--ui-preview' -PassThru
try {
 for ($i=0; $i -lt 100; $i++) {
  $p.Refresh()
  if ($p.HasExited) { throw 'Console exited before showing a window' }
  if ($p.MainWindowHandle -ne [IntPtr]::Zero) { break }
  Start-Sleep -Milliseconds 100
 }
 $h = $p.MainWindowHandle
 if ($h -eq [IntPtr]::Zero) { throw 'Console window did not appear' }
 $r = New-Object NativeUi+Rect
 [void][NativeUi]::GetWindowRect($h,[ref]$r)
 $width=$r.Right-$r.Left; $height=$r.Bottom-$r.Top
 if ([Math]::Abs($width/$height-360.0/174.0) -gt 0.03) { throw "Unexpected compact window size: $width x $height" }
 foreach ($id in @(101,102,103,110,111,112,120,130,131,132,133)) {
  if ([NativeUi]::GetDlgItem($h,$id) -eq [IntPtr]::Zero) { throw "Missing control $id" }
 }
 $bitmap = New-Object System.Drawing.Bitmap($width,$height)
 $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
 $dc = $graphics.GetHdc()
 try { if (-not [NativeUi]::PrintWindow($h,$dc,2)) { throw 'Window rendering failed' } }
 finally { $graphics.ReleaseHdc($dc) }
 $bitmap.Save($Screenshot,[System.Drawing.Imaging.ImageFormat]::Png)
 $graphics.Dispose(); $bitmap.Dispose()
 foreach ($id in @(101,102,103)) { [void][NativeUi]::SendMessage([NativeUi]::GetDlgItem($h,$id),0xF5,[IntPtr]::Zero,[IntPtr]::Zero) }
 [void][NativeUi]::SendMessage([NativeUi]::GetDlgItem($h,111),0xF5,[IntPtr]::Zero,[IntPtr]::Zero)
 [void][NativeUi]::GetWindowRect($h,[ref]$r)
 if ($r.Bottom-$r.Top -le $height) { throw 'Setup panel did not expand' }
 [void][NativeUi]::SendMessage([NativeUi]::GetDlgItem($h,110),0xF5,[IntPtr]::Zero,[IntPtr]::Zero)
 if (([NativeUi]::GetWindowLong($h,-20) -band 8) -eq 0) { throw 'PIN did not enable topmost' }
 [void][NativeUi]::GetWindowRect($h,[ref]$r)
 if ($r.Bottom-$r.Top -ne $height) { throw 'PIN did not return to compact size' }
 [void][NativeUi]::SendMessage([NativeUi]::GetDlgItem($h,110),0xF5,[IntPtr]::Zero,[IntPtr]::Zero)
 if (([NativeUi]::GetWindowLong($h,-20) -band 8) -ne 0) { throw 'PIN did not disable topmost' }
 [void][NativeUi]::SendMessage($h,0x10,[IntPtr]::Zero,[IntPtr]::Zero)
 if (-not $p.WaitForExit(5000) -or $p.ExitCode -ne 0) { throw 'Console did not close cleanly' }
 Write-Host 'PASS: Windows compact UI launch, source buttons in preview, setup panel, topmost toggle and clean exit. Audio was disabled.'
} finally { if (-not $p.HasExited) { $p.Kill() } }
