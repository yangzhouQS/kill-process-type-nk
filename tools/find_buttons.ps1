# find_buttons.ps1 - scan a row of the screenshot for button color blocks
param([string]$Png = 'build\v4_settings.png', [int]$Row = 77)
Add-Type -AssemblyName System.Drawing
$bmp = New-Object System.Drawing.Bitmap($Png)
$w = $bmp.Width; $h = $bmp.Height
Write-Output "image ${w}x${h}"
$inBlock = $false; $start = 0
$found = @()
for ($x = 0; $x -lt $w; $x++) {
  $p = $bmp.GetPixel($x, $Row)
  $isGreen = ($p.R -lt 120 -and $p.G -gt 80 -and $p.B -lt 120)
  $isBlue = ($p.B -gt 180 -and $p.R -lt 150)
  $isBtn = $isGreen -or $isBlue
  if ($isBtn -and -not $inBlock) { $inBlock = $true; $start = $x }
  if (-not $isBtn -and $inBlock) {
    $inBlock = $false
    if ($x - $start -gt 30) { $found += ,@($start, $x) }
  }
}
if ($inBlock) { $found += ,@($start, $w) }
foreach ($b in $found) {
  $cx = [int](($b[0] + $b[1]) / 2)
  Write-Output "block [$($b[0])..$($b[1])] center=$cx (clientX=$($cx - 11))"
}
$bmp.Dispose()
