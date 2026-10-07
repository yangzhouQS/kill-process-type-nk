# closex_test.ps1 — 恢复窗口显示，真实鼠标点击 X，验证驻留托盘
$ErrorActionPreference = 'Continue'
Add-Type -TypeDefinition @'
using System;
using System.Text;
using System.Runtime.InteropServices;
public class CX {
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
  public delegate bool P(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] public static extern bool EnumWindows(P cb, IntPtr l);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint p);
  [DllImport("user32.dll")] public static extern int GetWindowTextW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int cmd);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint f, int dx, int dy, int d, UIntPtr e);
  [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref PT p);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RC r);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint f);
  public struct PT { public int X, Y; }
  public struct RC { public int L, T, R, B; }
}
'@
Add-Type -AssemblyName System.Drawing
[CX]::SetProcessDPIAware() | Out-Null
$real = Get-Process -Name app | Select-Object -First 1
if (-not $real) { Write-Host 'app not running'; exit 1 }
$wantPid = $real.Id
$found = [IntPtr]::Zero
$cb = [CX+P]{ param($hh, $l)
  $p2 = 0
  [CX]::GetWindowThreadProcessId($hh, [ref]$p2) | Out-Null
  if ($p2 -eq $wantPid) {
    $sb = New-Object System.Text.StringBuilder 256
    [CX]::GetWindowTextW($hh, $sb, 256) | Out-Null
    if ($sb.ToString().StartsWith('kill-process-type-nk')) { $script:found = $hh }
  }
  return $true }
[CX]::EnumWindows($cb, [IntPtr]::Zero) | Out-Null
if ($found -eq [IntPtr]::Zero) { Write-Host 'window not found'; exit 1 }
[CX]::ShowWindow($found, 9) | Out-Null   # SW_RESTORE
[CX]::SetForegroundWindow($found) | Out-Null
Start-Sleep 1
$vis1 = [CX]::IsWindowVisible($found)

# 客户区坐标 -> 屏幕：X 按钮 ≈ 客户区 (W-40, 15)
$r = New-Object CX+RC
[CX]::GetWindowRect($found, [ref]$r) | Out-Null
$w = $r.R - $r.L
$pt = New-Object CX+PT
$pt.X = $w - 45; $pt.Y = 16
[CX]::ClientToScreen($found, [ref]$pt) | Out-Null
[CX]::SetCursorPos($pt.X, $pt.Y) | Out-Null
Start-Sleep 200
[CX]::mouse_event(2, 0, 0, 0, [UIntPtr]::Zero)
Start-Sleep 60
[CX]::mouse_event(4, 0, 0, 0, [UIntPtr]::Zero)
Start-Sleep 1500

$alive = Get-Process -Id $wantPid -ErrorAction SilentlyContinue
$vis2 = [CX]::IsWindowVisible($found)
Write-Host "before: vis=$vis1  after X click: alive=$($null -ne $alive) vis=$vis2"
Write-Host "(alive=True + vis=False = 成功驻留托盘)"

# 恢复显示供后续验证
[CX]::ShowWindow($found, 9) | Out-Null
[CX]::SetForegroundWindow($found) | Out-Null
