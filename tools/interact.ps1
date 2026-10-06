# interact.ps1 - click / rclic / type / wheel / shot on app window
param(
  [Parameter(Position=0)][string]$Action,
  [int]$X, [int]$Y,
  [string]$Text,
  [string]$Out = 'build\shot.png',
  [int]$Ticks = 3
)
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public class IAct {
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
  [DllImport("user32.dll")] public static extern bool PostMessageW(IntPtr h, uint msg, IntPtr wp, IntPtr lp);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint flags);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint flags, int dx, int dy, int data, UIntPtr extra);
  [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
  [DllImport("user32.dll")] public static extern IntPtr FindWindowA(string cls, string title);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int cmd);
  public struct RECT { public int L, T, R, B; }
  public struct POINT { public int X, Y; }
}
'@
Add-Type -AssemblyName System.Drawing
[IAct]::SetProcessDPIAware() | Out-Null
$app = Get-Process -Name app -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $app) { Write-Output 'APP NOT RUNNING'; exit 1 }
$h = $app.MainWindowHandle
if ($h -eq [IntPtr]::Zero) {
  $h = [IAct]::FindWindowA($null, 'kill-process-type-nk')
  if ($h -ne [IntPtr]::Zero) {
    [IAct]::ShowWindow($h, 9) | Out-Null  # SW_RESTORE
    Start-Sleep -Milliseconds 500
    Write-Output 'window restored from tray'
  }
}
if ($h -eq [IntPtr]::Zero) { Write-Output 'NO WINDOW'; exit 1 }

function ClickAt([int]$x, [int]$y) {
  [IntPtr]$lp = [IntPtr](($y -shl 16) -bor ($x -band 0xFFFF))
  [IAct]::PostMessageW($h, 0x0200, [IntPtr]0, $lp) | Out-Null
  Start-Sleep -Milliseconds 80
  [IAct]::PostMessageW($h, 0x0201, [IntPtr]1, $lp) | Out-Null
  Start-Sleep -Milliseconds 60
  [IAct]::PostMessageW($h, 0x0202, [IntPtr]0, $lp) | Out-Null
  Start-Sleep -Milliseconds 250
}
function Shot([string]$f) {
  $r = New-Object IAct+RECT
  [IAct]::GetWindowRect($h, [ref]$r) | Out-Null
  $w = $r.R - $r.L; $ht = $r.B - $r.T
  $bmp = New-Object System.Drawing.Bitmap($w, $ht)
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $dc = $g.GetHdc()
  [IAct]::PrintWindow($h, $dc, 2) | Out-Null
  $g.ReleaseHdc($dc); $g.Dispose()
  $bmp.Save($f, [System.Drawing.Imaging.ImageFormat]::Png)
  $bmp.Dispose()
}

switch ($Action) {
  'click'  { ClickAt $X $Y; Write-Output "click $X,$Y" }
  'rclick' {
    [IntPtr]$lp = [IntPtr](($Y -shl 16) -bor ($X -band 0xFFFF))
    [IAct]::PostMessageW($h, 0x0204, [IntPtr]2, $lp) | Out-Null
    Start-Sleep -Milliseconds 60
    [IAct]::PostMessageW($h, 0x0205, [IntPtr]0, $lp) | Out-Null
    Start-Sleep -Milliseconds 250
    Write-Output "rclick $X,$Y"
  }
  'type'   {
    foreach ($ch in $Text.ToCharArray()) {
      [IAct]::PostMessageW($h, 0x0102, [IntPtr][int]$ch, [IntPtr]::Zero) | Out-Null
      Start-Sleep -Milliseconds 40
    }
    Write-Output "typed: $Text"
  }
  'key'    {
    [IAct]::PostMessageW($h, 0x0100, [IntPtr]$Text, [IntPtr]::Zero) | Out-Null
    Start-Sleep -Milliseconds 80
    [IAct]::PostMessageW($h, 0x0101, [IntPtr]$Text, [IntPtr]::Zero) | Out-Null
    Write-Output "vk $Text"
  }
  'wheel'  {
    for ($i = 0; $i -lt $Ticks; $i++) {
      [IAct]::PostMessageW($h, 0x020A, [IntPtr](-7864320), [IntPtr]::Zero) | Out-Null
      Start-Sleep -Milliseconds 60
    }
    Write-Output "wheel -$Ticks"
  }
  'realclick' {
    [IAct]::SetForegroundWindow($h) | Out-Null
    Start-Sleep -Milliseconds 300
    $pt = New-Object IAct+POINT
    $pt.X = $X; $pt.Y = $Y
    [IAct]::ClientToScreen($h, [ref]$pt) | Out-Null
    [IAct]::SetCursorPos($pt.X, $pt.Y) | Out-Null
    Start-Sleep -Milliseconds 150
    [IAct]::mouse_event(2, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 60
    [IAct]::mouse_event(4, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 250
    Write-Output "realclick $X,$Y -> screen $($pt.X),$($pt.Y)"
  }
  'realrclick' {
    [IAct]::SetForegroundWindow($h) | Out-Null
    Start-Sleep -Milliseconds 300
    $pt = New-Object IAct+POINT
    $pt.X = $X; $pt.Y = $Y
    [IAct]::ClientToScreen($h, [ref]$pt) | Out-Null
    [IAct]::SetCursorPos($pt.X, $pt.Y) | Out-Null
    Start-Sleep -Milliseconds 150
    [IAct]::mouse_event(8, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 60
    [IAct]::mouse_event(16, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 250
    Write-Output "realrclick $X,$Y -> screen $($pt.X),$($pt.Y)"
  }
  'menushot' {
    [IAct]::SetForegroundWindow($h) | Out-Null
    Start-Sleep -Milliseconds 300
    $pt = New-Object IAct+POINT
    $pt.X = $X; $pt.Y = $Y
    [IAct]::ClientToScreen($h, [ref]$pt) | Out-Null
    [IAct]::SetCursorPos($pt.X, $pt.Y) | Out-Null
    Start-Sleep -Milliseconds 150
    [IAct]::mouse_event(8, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 60
    [IAct]::mouse_event(16, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 300
    Shot $Out
    Write-Output "menushot after rclick $X,$Y"
  }
  'shot'   { Shot $Out; Write-Output "saved $Out" }
  default  { Write-Output "usage: interact.ps1 click|rclick|type|key|wheel|shot ..." }
}
