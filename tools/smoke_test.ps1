# smoke_test.ps1 — kill-process-type-nk 一键冒烟测试
# 用法: powershell -File tools\smoke_test.ps1 [-KeepOpen]
# 流程: 构建 -> 启动 -> 五页签遍历截图 -> AI面板截图 -> 树折叠点击 -> 结束
param([switch]$KeepOpen)

$ErrorActionPreference = 'Continue'
$root = Split-Path -Parent $PSScriptRoot   # tools/ 的父目录 = 项目根
Set-Location $root

Write-Host '[1/6] build...' -ForegroundColor Cyan
cmd /c build.bat 2>&1 | Out-Null
if (-not (Test-Path (Join-Path $root 'build\app.exe'))) { Write-Host 'BUILD FAILED, abort.' -ForegroundColor Red; exit 1 }

# 干净启动（StartMinimized 关闭）
$ini = Join-Path $root 'build\kill-process-type.ini'
if (Test-Path $ini) {
    (Get-Content $ini -Encoding UTF8) -replace 'StartMinimized=1', 'StartMinimized=0' |
        Set-Content $ini -Encoding ASCII
}

Write-Host '[2/6] launch...' -ForegroundColor Cyan
$proc = Start-Process -FilePath (Join-Path $root 'build\app.exe') -WorkingDirectory $root -PassThru
Start-Sleep 5

Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public class SmokeCap {
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint flags);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint f, int dx, int dy, int d, UIntPtr e);
  [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
  public struct RECT { public int L, T, R, B; }
  public struct POINT { public int X, Y; }
}
'@
Add-Type -AssemblyName System.Drawing
[SmokeCap]::SetProcessDPIAware() | Out-Null
$app = Get-Process -Name app | Select-Object -First 1
$h = $app.MainWindowHandle
[SmokeCap]::SetForegroundWindow($h) | Out-Null
Start-Sleep 1

function ClickAt([int]$cx, [int]$cy) {
    $pt = New-Object SmokeCap+POINT
    $pt.X = $cx; $pt.Y = $cy
    [SmokeCap]::ClientToScreen($h, [ref]$pt) | Out-Null
    [SmokeCap]::SetCursorPos($pt.X, $pt.Y) | Out-Null
    Start-Sleep -Milliseconds 120
    [SmokeCap]::mouse_event(2, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 60
    [SmokeCap]::mouse_event(4, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 350
}

function Shot([string]$name) {
    $r = New-Object SmokeCap+RECT
    [SmokeCap]::GetWindowRect($h, [ref]$r) | Out-Null
    $w = $r.R - $r.L; $ht = $r.B - $r.T
    $bmp = New-Object System.Drawing.Bitmap($w, $ht)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $dc = $g.GetHdc()
    [SmokeCap]::PrintWindow($h, $dc, 2) | Out-Null
    $g.ReleaseHdc($dc); $g.Dispose()
    $out = Join-Path $root "build\smoke_$name.png"
    $bmp.Save($out, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    Write-Host "  shot -> smoke_$name.png" -ForegroundColor DarkGray
}

Write-Host '[3/6] tabs + screenshots...' -ForegroundColor Cyan
Shot '01_processes'
# 页签栏 y=92（客户区），7 页签自适应宽 ~120px：Node·Python(136) 进程树(270) 项目(404) 端口(538) 日志(672)
ClickAt 136 92; Shot '02_nodepy'
ClickAt 270 92; Shot '03_tree'
# 树折叠：点击第一行行首箭头区
ClickAt 20 272; Start-Sleep 1; Shot '04_tree_folded'
ClickAt 404 92; Shot '05_project'
ClickAt 538 92; Shot '06_ports'
ClickAt 672 92; Shot '07_logs'

Write-Host '[4/6] AI panel...' -ForegroundColor Cyan
# AI 对话按钮（工具栏右上）
ClickAt 980 42; Start-Sleep 1; Shot '08_ai_panel'
ClickAt 300 300  # 关闭面板（点面板外区域不关——面板是模态需点关闭按钮）
# 点关闭按钮（面板右上 px+pw-104+42）
$W = 1192
ClickAt ($W - 430) 110

Write-Host '[5/6] done.' -ForegroundColor Cyan

if (-not $KeepOpen) {
    Get-Process -Name app -ErrorAction SilentlyContinue | Stop-Process -Force
    Write-Host '[6/6] app closed. Screenshots: build\smoke_*.png' -ForegroundColor Cyan
} else {
    Write-Host '[6/6] app left running (-KeepOpen).' -ForegroundColor Cyan
}
