param([string]$Destination = (Join-Path $PSScriptRoot 'Assets'))
# Draws the BudsPlus icon: a pair of white earbuds (round bud with a stem) on a near-black tile, with the red dot.
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
New-Item -ItemType Directory -Path $Destination -Force | Out-Null

function Add-RoundRect($path, [single]$x, [single]$y, [single]$w, [single]$h, [single]$r) {
    $r = [single][Math]::Min($r, [Math]::Min($w, $h))
    $path.AddArc($x, $y, $r, $r, 180, 90); $path.AddArc($x + $w - $r, $y, $r, $r, 270, 90)
    $path.AddArc($x + $w - $r, $y + $h - $r, $r, $r, 0, 90); $path.AddArc($x, $y + $h - $r, $r, $r, 90, 90)
    $path.CloseFigure()
}

function New-IconBitmap([int]$size) {
    $bmp = New-Object Drawing.Bitmap($size, $size, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.Clear([Drawing.Color]::Transparent)
    $s = [single]$size
    $tile = New-Object Drawing.Drawing2D.GraphicsPath
    $m = [single]($s * 0.04)
    Add-RoundRect $tile $m $m ($s - 2 * $m) ($s - 2 * $m) ([Math]::Max(2, $s * 0.44))
    $bg = New-Object Drawing.Drawing2D.LinearGradientBrush((New-Object Drawing.PointF(0, 0)), (New-Object Drawing.PointF(0, $s)), [Drawing.ColorTranslator]::FromHtml('#2B2B2B'), [Drawing.ColorTranslator]::FromHtml('#0E0E0E'))
    $g.FillPath($bg, $tile)
    $white = New-Object Drawing.SolidBrush([Drawing.Color]::White)
    $grey = New-Object Drawing.SolidBrush([Drawing.ColorTranslator]::FromHtml('#BDBDBD'))
    # Two earbuds, mirrored: a bud (circle) and a stem going down.
    foreach ($side in -1, 1) {
        $cx = [single]($s / 2 + $side * $s * 0.17)
        $budR = [single]($s * 0.13)
        $budY = [single]($s * 0.34)
        $stemW = [single]([Math]::Max(2, $s * 0.10))
        $stem = New-Object Drawing.Drawing2D.GraphicsPath
        Add-RoundRect $stem ([single]($cx - $stemW / 2)) $budY $stemW ([single]($s * 0.40)) $stemW
        $g.FillPath($white, $stem)
        $stem.Dispose()
        $g.FillEllipse($white, [single]($cx - $budR), [single]($budY - $budR), [single](2 * $budR), [single](2 * $budR))
        if ($size -ge 32) {
            # Speaker grille, towards the inside.
            $gr = [single]($budR * 0.42)
            $gx = [single]($cx - $side * $budR * 0.30)
            $g.FillEllipse($grey, [single]($gx - $gr), [single]($budY - $gr), [single](2 * $gr), [single](2 * $gr))
        }
    }
    # The red dot.
    $red = New-Object Drawing.SolidBrush([Drawing.ColorTranslator]::FromHtml('#E82525'))
    $dr = [single]([Math]::Max(1.5, $s * 0.065))
    $g.FillEllipse($red, [single]($s / 2 - $dr), [single]($s * 0.80 - $dr), [single](2 * $dr), [single](2 * $dr))
    $red.Dispose(); $grey.Dispose(); $white.Dispose(); $bg.Dispose(); $tile.Dispose(); $g.Dispose()
    return $bmp
}

# Multi-resolution ICO with PNG-compressed entries (supported since Windows Vista).
$sizes = 16, 20, 24, 32, 40, 48, 64, 256
$images = foreach ($size in $sizes) {
    $bmp = New-IconBitmap $size
    $ms = New-Object IO.MemoryStream
    $bmp.Save($ms, [Drawing.Imaging.ImageFormat]::Png)
    if ($size -eq 256) { $bmp.Save((Join-Path $Destination 'logo-256.png'), [Drawing.Imaging.ImageFormat]::Png) }
    $bmp.Dispose()
    , $ms.ToArray()
}
$out = New-Object IO.MemoryStream
$bw = New-Object IO.BinaryWriter($out)
$bw.Write([UInt16]0); $bw.Write([UInt16]1); $bw.Write([UInt16]$sizes.Count)
$offset = 6 + 16 * $sizes.Count
for ($i = 0; $i -lt $sizes.Count; $i++) {
    $dim = if ($sizes[$i] -ge 256) { 0 } else { $sizes[$i] }
    $bw.Write([byte]$dim); $bw.Write([byte]$dim); $bw.Write([byte]0); $bw.Write([byte]0)
    $bw.Write([UInt16]1); $bw.Write([UInt16]32)
    $bw.Write([UInt32]$images[$i].Length); $bw.Write([UInt32]$offset)
    $offset += $images[$i].Length
}
foreach ($img in $images) { $bw.Write($img) }
$bw.Flush()
[IO.File]::WriteAllBytes((Join-Path $Destination 'app.ico'), $out.ToArray())
$bw.Dispose()
