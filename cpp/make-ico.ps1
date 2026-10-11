Add-Type -AssemblyName System.Drawing

# --- master render: dark rounded tile + gradient "0x" + convert arrow
$W = 512
$bmp = New-Object System.Drawing.Bitmap $W, $W
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.SmoothingMode = 'AntiAlias'
$g.InterpolationMode = 'HighQualityBicubic'
$g.PixelOffsetMode = 'HighQuality'
$g.TextRenderingHint = 'AntiAliasGridFit'

function RoundedRect($x, $y, $w, $h, $r) {
    $p = New-Object System.Drawing.Drawing2D.GraphicsPath
    $d = $r * 2
    $p.AddArc($x, $y, $d, $d, 180, 90)
    $p.AddArc($x + $w - $d, $y, $d, $d, 270, 90)
    $p.AddArc($x + $w - $d, $y + $h - $d, $d, $d, 0, 90)
    $p.AddArc($x, $y + $h - $d, $d, $d, 90, 90)
    $p.CloseFigure()
    return $p
}

function ARGB($a, $r, $g, $b) {
    return [System.Drawing.Color]::FromArgb($a, $r, $g, $b)
}

# tile body: navy vertical gradient (matches the app's dark chrome)
$tile = RoundedRect 20 20 472 472 100
$bg = New-Object System.Drawing.Drawing2D.LinearGradientBrush `
    (New-Object System.Drawing.Point(0, 20)), `
    (New-Object System.Drawing.Point(0, 492)), `
    (ARGB 255 22 40 63), (ARGB 255 7 14 24)
$g.FillPath($bg, $tile)

# top glass sheen
$sheen = RoundedRect 28 28 456 210 88
$sg = New-Object System.Drawing.Drawing2D.LinearGradientBrush `
    (New-Object System.Drawing.Point(0, 28)), `
    (New-Object System.Drawing.Point(0, 238)), `
    (ARGB 26 255 255 255), (ARGB 0 255 255 255)
$g.FillPath($sg, $sheen)

# rim light
$pen = New-Object System.Drawing.Pen (ARGB 150 110 170 250), 6
$g.DrawPath($pen, $tile)

# "0x" — app title gradient: #7CC4FF -> #3A9BDC -> #9D7BFF
$font = New-Object System.Drawing.Font('Segoe UI', 215, [System.Drawing.FontStyle]::Bold)
$fmt = New-Object System.Drawing.StringFormat
$fmt.Alignment = 'Center'
$fmt.LineAlignment = 'Center'
$layout = New-Object System.Drawing.RectangleF(20, 66, 472, 300)
$tbr = New-Object System.Drawing.Drawing2D.LinearGradientBrush `
    (New-Object System.Drawing.Point(40, 0)), `
    (New-Object System.Drawing.Point(472, 0)), `
    (ARGB 255 124 196 255), (ARGB 255 157 123 255)
$blend = New-Object System.Drawing.Drawing2D.ColorBlend(3)
$blend.Colors = @(
    (ARGB 255 124 196 255),
    (ARGB 255 58 155 220),
    (ARGB 255 157 123 255))
$blend.Positions = @(0.0, 0.5, 1.0)
$tbr.InterpolationColors = $blend
$g.DrawString('0x', $font, $tbr, $layout, $fmt)

# <-> convert arrow under the text
$arr = New-Object System.Drawing.Pen (ARGB 235 110 190 255), 20
$arr.StartCap = 'Round'
$arr.EndCap = 'Round'
$ay = 400; $ax0 = 148; $ax1 = 364
$g.DrawLine($arr, $ax0, $ay, $ax1, $ay)
$g.DrawLine($arr, $ax0 + 38, $ay - 28, $ax0, $ay)
$g.DrawLine($arr, $ax0 + 38, $ay + 28, $ax0, $ay)
$g.DrawLine($arr, $ax1 - 38, $ay - 28, $ax1, $ay)
$g.DrawLine($arr, $ax1 - 38, $ay + 28, $ax1, $ay)

# save the source PNG next to the script's assets dir
$assets = Join-Path $PSScriptRoot '..\assets'
New-Item -ItemType Directory -Force $assets | Out-Null
$png = Join-Path $assets 'app-icon.png'
$bmp.Save($png, [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose()

# --- pack multi-size .ico from the master
$src = [System.Drawing.Image]::FromFile($png)
$sizes = 16, 24, 32, 48, 64, 128, 256
$pngs = @()
foreach ($s in $sizes) {
    $b = New-Object System.Drawing.Bitmap $s, $s
    $gg = [System.Drawing.Graphics]::FromImage($b)
    $gg.InterpolationMode = 'HighQualityBicubic'
    $gg.SmoothingMode = 'HighQuality'
    $gg.PixelOffsetMode = 'HighQuality'
    $gg.DrawImage($src, 0, 0, $s, $s)
    $gg.Dispose()
    $ms = New-Object System.IO.MemoryStream
    $b.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
    $pngs += , $ms.ToArray()
    $ms.Dispose()
    $b.Dispose()
}
$src.Dispose()
$bmp.Dispose()

$fs = [System.IO.File]::Create((Join-Path $PSScriptRoot 'src\app.ico'))
$bw = New-Object System.IO.BinaryWriter $fs
$bw.Write([uint16]0); $bw.Write([uint16]1); $bw.Write([uint16]$sizes.Count)
$offset = 6 + 16 * $sizes.Count
foreach ($p in $pngs) {
    $w = ($p[16] -shl 24) -bor ($p[17] -shl 16) -bor ($p[18] -shl 8) -bor $p[19]
    $h = ($p[20] -shl 24) -bor ($p[21] -shl 16) -bor ($p[22] -shl 8) -bor $p[23]
    $bw.Write([byte]($w -band 0xFF)); $bw.Write([byte]($h -band 0xFF))
    $bw.Write([byte]0); $bw.Write([byte]0)
    $bw.Write([uint16]1); $bw.Write([uint16]32)
    $bw.Write([uint32]$p.Length); $bw.Write([uint32]$offset)
    $offset += $p.Length
}
foreach ($p in $pngs) { $bw.Write($p) }
$bw.Flush(); $fs.Close()

Write-Host "wrote $png"
Write-Host "wrote $(Join-Path $PSScriptRoot 'src\app.ico')"
