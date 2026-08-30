#Requires -Version 5.1
<#
.SYNOPSIS
    Generates placeholder logo PNGs for the Windows MIDI Console MSIX package.

.DESCRIPTION
    The MSIX manifest references a set of tile/logo images. This script generates simple
    placeholder PNGs at the required sizes so the package can be built before real branding
    art is available.

    TODO: replace the generated files in Packaging\Images with real artwork.

.PARAMETER Force
    Overwrite images that already exist. By default existing files are left untouched so
    real artwork is never clobbered.
#>
[CmdletBinding()]
param(
    [switch] $Force
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

Add-Type -AssemblyName System.Drawing

$imagesDir = Join-Path $PSScriptRoot 'Images'
if (-not (Test-Path $imagesDir)) {
    New-Item -ItemType Directory -Path $imagesDir | Out-Null
}

# name -> width x height, matching the assets referenced by Package.appxmanifest.
$assets = [ordered]@{
    'Square44x44Logo.png'                  = @(44, 44)
    'Square44x44Logo.targetsize-24_altform-unplated.png' = @(24, 24)
    'Square150x150Logo.png'                = @(150, 150)
    'SmallTile.png'                        = @(71, 71)
    'LargeTile.png'                        = @(310, 310)
    'Wide310x150Logo.png'                  = @(310, 150)
    'StoreLogo.png'                        = @(50, 50)
}

$background = [System.Drawing.Color]::FromArgb(255, 0, 90, 158)
$foreground = [System.Drawing.Color]::White

foreach ($name in $assets.Keys) {
    $path = Join-Path $imagesDir $name

    if ((Test-Path $path) -and -not $Force) {
        Write-Host "Skipping existing $name"
        continue
    }

    $width  = $assets[$name][0]
    $height = $assets[$name][1]

    $bitmap   = New-Object System.Drawing.Bitmap($width, $height)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.SmoothingMode     = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
        $graphics.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAlias

        $brush = New-Object System.Drawing.SolidBrush($background)
        $graphics.FillRectangle($brush, 0, 0, $width, $height)
        $brush.Dispose()

        # Draw a simple "M" centered in the tile.
        $fontSize = [Math]::Max(6, [int]([Math]::Min($width, $height) * 0.55))
        $font     = New-Object System.Drawing.Font('Segoe UI', $fontSize, [System.Drawing.FontStyle]::Bold, [System.Drawing.GraphicsUnit]::Pixel)
        $format   = New-Object System.Drawing.StringFormat
        $format.Alignment     = [System.Drawing.StringAlignment]::Center
        $format.LineAlignment = [System.Drawing.StringAlignment]::Center

        $textBrush = New-Object System.Drawing.SolidBrush($foreground)
        $rect      = New-Object System.Drawing.RectangleF(0, 0, $width, $height)
        $graphics.DrawString('M', $font, $textBrush, $rect, $format)

        $textBrush.Dispose()
        $format.Dispose()
        $font.Dispose()

        $bitmap.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
        Write-Host "Created $name ($width x $height)"
    }
    finally {
        $graphics.Dispose()
        $bitmap.Dispose()
    }
}

Write-Host ''
Write-Host "Placeholder images written to $imagesDir" -ForegroundColor Yellow
Write-Host 'Replace these with real branding art before shipping.' -ForegroundColor Yellow
