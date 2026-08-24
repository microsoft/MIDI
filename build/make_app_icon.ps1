# Copyright (c) Microsoft Corporation. All rights reserved.

<#
.SYNOPSIS
Builds a multi-resolution .ico from a square source PNG.

.DESCRIPTION
Produces the same layout the existing app icons use: 16, 20, 24, 32, 48, 64 and 256 pixel entries,
each stored 32bpp and PNG-compressed. Windows 10 and later read PNG-compressed entries at every size,
so there are no legacy DIB entries.

Source art should be square and have a real alpha channel. The design folder holds both a flattened
copy and an "@2x" copy of each icon; the @2x files are the ones with alpha and are what should be used.

.PARAMETER SourcePath
The source PNG.

.PARAMETER DestinationPath
The .ico to write. Overwritten if it exists.

.EXAMPLE
.\make_app_icon.ps1 -SourcePath ..\design\AppIcon-MIDI-SysEx-Tool@2x.png `
                    -DestinationPath ..\src\api\Client\WinRT\user-tools\sysex-tool\Assets\AppIcon.ico
#>

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $SourcePath,

    [Parameter(Mandatory = $true)]
    [string] $DestinationPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

Add-Type -AssemblyName System.Drawing

$IconSizes = @(16, 20, 24, 32, 48, 64, 256)

$resolvedSource = (Resolve-Path -LiteralPath $SourcePath).Path

# .NET file APIs resolve relative paths against the process working directory, which is not
# PowerShell's current location, so the destination is made absolute up front.
$resolvedDestination = $PSCmdlet.GetUnresolvedProviderPathFromPSPath($DestinationPath)

$source = [System.Drawing.Image]::FromFile($resolvedSource)

try
{
    if ($source.Width -ne $source.Height)
    {
        Write-Warning ("Source is {0}x{1}, not square. It will be stretched." -f $source.Width, $source.Height)
    }

    $encoded = [System.Collections.Generic.List[byte[]]]::new()

    foreach ($size in $IconSizes)
    {
        # System.Drawing's PNG encoder is noticeably worse than the tools that produce the source
        # art, so when a size matches the source exactly the original bytes are reused as-is.
        if ($size -eq $source.Width -and $size -eq $source.Height)
        {
            $encoded.Add([System.IO.File]::ReadAllBytes($resolvedSource))
            continue
        }

        $bitmap = [System.Drawing.Bitmap]::new($size, $size,
            [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)

        try
        {
            $graphics = [System.Drawing.Graphics]::FromImage($bitmap)

            try
            {
                $graphics.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceCopy
                $graphics.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
                $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
                $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
                $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
                $graphics.Clear([System.Drawing.Color]::Transparent)

                # Without TileFlipXY the sampler reads transparent black past the edge and leaves a
                # dark halo around the outside of every downscaled size.
                $attributes = [System.Drawing.Imaging.ImageAttributes]::new()

                try
                {
                    $attributes.SetWrapMode([System.Drawing.Drawing2D.WrapMode]::TileFlipXY)

                    $destination = [System.Drawing.Rectangle]::new(0, 0, $size, $size)

                    $graphics.DrawImage($source, $destination, 0, 0, $source.Width, $source.Height,
                        [System.Drawing.GraphicsUnit]::Pixel, $attributes)
                }
                finally
                {
                    $attributes.Dispose()
                }
            }
            finally
            {
                $graphics.Dispose()
            }

            $stream = [System.IO.MemoryStream]::new()

            try
            {
                $bitmap.Save($stream, [System.Drawing.Imaging.ImageFormat]::Png)
                $encoded.Add($stream.ToArray())
            }
            finally
            {
                $stream.Dispose()
            }
        }
        finally
        {
            $bitmap.Dispose()
        }
    }
}
finally
{
    $source.Dispose()
}

$output = [System.IO.MemoryStream]::new()
$writer = [System.IO.BinaryWriter]::new($output)

try
{
    # ICONDIR: reserved, type 1 (icon), image count.
    $writer.Write([uint16] 0)
    $writer.Write([uint16] 1)
    $writer.Write([uint16] $IconSizes.Count)

    $offset = 6 + (16 * $IconSizes.Count)

    for ($i = 0; $i -lt $IconSizes.Count; $i++)
    {
        $size = $IconSizes[$i]
        $bytes = $encoded[$i]

        # 256 is stored as 0 in the single-byte width and height fields.
        $dimension = if ($size -ge 256) { 0 } else { $size }

        $writer.Write([byte] $dimension)      # width
        $writer.Write([byte] $dimension)      # height
        $writer.Write([byte] 0)               # palette entries, 0 for 32bpp
        $writer.Write([byte] 0)               # reserved
        $writer.Write([uint16] 1)             # color planes
        $writer.Write([uint16] 32)            # bits per pixel
        $writer.Write([uint32] $bytes.Length)
        $writer.Write([uint32] $offset)

        $offset += $bytes.Length
    }

    foreach ($bytes in $encoded)
    {
        $writer.Write($bytes)
    }

    $writer.Flush()

    $destinationDirectory = Split-Path -Parent $resolvedDestination

    if ($destinationDirectory -and -not (Test-Path -LiteralPath $destinationDirectory))
    {
        New-Item -ItemType Directory -Path $destinationDirectory -Force | Out-Null
    }

    [System.IO.File]::WriteAllBytes($resolvedDestination, $output.ToArray())
}
finally
{
    $writer.Dispose()
    $output.Dispose()
}

$written = Get-Item -LiteralPath $resolvedDestination

Write-Host ("Wrote {0} ({1:N0} bytes, {2} sizes: {3})" -f `
    $written.FullName, $written.Length, $IconSizes.Count, ($IconSizes -join ', '))
