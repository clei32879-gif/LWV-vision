# gen_app_icon.ps1 - Generate LW Vision app icon (multi-size .ico, PNG-compressed entries)
# Usage: powershell -ExecutionPolicy Bypass -File tools/gen_app_icon.ps1
# Output: src/app/resources/app.ico
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$outDir = Join-Path $PSScriptRoot '..\src\app\resources'
$null = New-Item -ItemType Directory -Force -Path $outDir
$outIco = Join-Path $outDir 'app.ico'

# Draw the app icon at a given size. Returns System.Drawing.Bitmap (ARGB).
# Design: blue gradient disc, white eye (iris+pupil+highlight), viewfinder corner brackets.
function New-IconBitmap([int]$S) {
    $bmp = New-Object System.Drawing.Bitmap($S, $S, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.Clear([System.Drawing.Color]::Transparent)

    $f = [single]($S / 256.0)

    # ---- disc background (blue gradient) ----
    $c0 = [System.Drawing.Color]::FromArgb(255, 33, 150, 243)
    $c1 = [System.Drawing.Color]::FromArgb(255, 13, 71, 161)
    $lgRect = New-Object System.Drawing.RectangleF([single]0, [single]0, [single]$S, [single]$S)
    $lg = New-Object System.Drawing.Drawing2D.LinearGradientBrush(
        $lgRect, $c0, $c1, [System.Drawing.Drawing2D.LinearGradientMode]::Vertical)
    $g.FillEllipse($lg, [single]0, [single]0, [single]$S, [single]$S)

    # ---- corner brackets (viewfinder), only at larger sizes ----
    if ($S -ge 48) {
        $pen = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(210, 255, 255, 255), [single](3.5 * $f))
        $pen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
        $pen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
        $m = [single](30 * $f)
        $L = [single](30 * $f)
        $max = [single]($S - 1)
        foreach ($px in @($m, ($max - $m))) {
            foreach ($py in @($m, ($max - $m))) {
                $dx = if ($px -lt ($S / 2)) { [single]$L } else { [single](-$L) }
                $dy = if ($py -lt ($S / 2)) { [single]$L } else { [single](-$L) }
                $g.DrawLine($pen, $px, ($py + $dy), $px, $py)
                $g.DrawLine($pen, $px, $py, ($px + $dx), $py)
            }
        }
        $pen.Dispose()
    }

    # ---- eye: white almond (two overlapping ellipses), iris, pupil, highlight ----
    $cx = [single]($S / 2.0)
    $cy = [single]($S / 2.0)
    $white = [System.Drawing.Brushes]::White
    # almond approximated by a wide ellipse
    $g.FillEllipse($white, $cx - [single](58 * $f), $cy - [single](46 * $f), [single](116 * $f), [single](92 * $f))

    $irisR = [single](34 * $f)
    $g.FillEllipse([System.Drawing.Brushes]::CornflowerBlue,
        $cx - $irisR, $cy - $irisR, $irisR * 2, $irisR * 2)
    $pupilR = [single](17 * $f)
    $navy = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 13, 43, 75))
    $g.FillEllipse($navy, $cx - $pupilR, $cy - $pupilR, $pupilR * 2, $pupilR * 2)
    $hl = [single](8 * $f)
    $g.FillEllipse($white, $cx - $hl - [single](14 * $f), $cy - $hl - [single](14 * $f), $hl * 2, $hl * 2)
    $navy.Dispose()

    $g.Dispose()
    return $bmp
}

# Build .ico with PNG-compressed entries (valid on Vista+; simple to emit).
$sizes = 256, 64, 48, 32, 16
$pngs = @()
foreach ($S in $sizes) {
    $bmp = New-IconBitmap $S
    $ms = New-Object System.IO.MemoryStream
    $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
    $pngs += ,$ms.ToArray()
    $bmp.Dispose()
    $ms.Dispose()
}

$count = $sizes.Count
$headerSize = 6
$entrySize = 16
$offset = $headerSize + $entrySize * $count
$fs = [System.IO.File]::Create($outIco)
$bw = New-Object System.IO.BinaryWriter($fs)

# ICONDIR
$bw.Write([UInt16]0)       # reserved
$bw.Write([UInt16]1)       # type: icon
$bw.Write([UInt16]$count)  # count

for ($i = 0; $i -lt $count; $i++) {
    $S = $sizes[$i]
    $w = if ($S -ge 256) { 0 } else { $S }   # 0 means 256
    $len = $pngs[$i].Length
    $bw.Write([Byte]$w)            # width
    $bw.Write([Byte]$w)            # height
    $bw.Write([Byte]0)             # palette
    $bw.Write([Byte]0)             # reserved
    $bw.Write([UInt16]1)           # planes
    $bw.Write([UInt16]32)          # bpp
    $bw.Write([UInt32]$len)        # data size
    $bw.Write([UInt32]$offset)     # data offset
    $offset += $len
}

foreach ($p in $pngs) { $bw.Write($p) }

$bw.Dispose()
$fs.Dispose()
Write-Output ("ICON WRITTEN: " + $outIco + " (" + $count + " sizes)")
