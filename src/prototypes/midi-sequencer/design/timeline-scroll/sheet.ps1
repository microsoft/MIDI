param([string] $Out, [string[]] $Crops, [double] $Scale = 1.0)
# A contact sheet: crops of shots stacked top to bottom, each "name.png:x,y,w,h", with its name above.
Add-Type -AssemblyName System.Drawing
$crops = @($Crops | ForEach-Object { $_ -split ';' } | Where-Object { $_ })
$items = foreach ($c in $crops) {
    $file, $rect = $c -split ':'
    $x, $y, $w, $h = $rect -split ',' | ForEach-Object { [int]$_ }
    [pscustomobject]@{ File = $file; X = $x; Y = $y; W = $w; H = $h }
}
$label = 18
$width = [int](($items | Measure-Object W -Maximum).Maximum * $Scale)
$height = [int](($items | ForEach-Object { $_.H * $Scale + $label + 6 } | Measure-Object -Sum).Sum)
$sheet = New-Object System.Drawing.Bitmap $width, $height
$g = [System.Drawing.Graphics]::FromImage($sheet)
$g.Clear([System.Drawing.Color]::FromArgb(255, 255, 255))
$g.InterpolationMode = 'HighQualityBicubic'
$font = New-Object System.Drawing.Font 'Segoe UI', 10
$top = 0
foreach ($i in $items) {
    $g.DrawString($i.File, $font, [System.Drawing.Brushes]::Black, 2, $top + 1)
    $img = [System.Drawing.Image]::FromFile((Join-Path $PSScriptRoot "shots\$($i.File)"))
    $dest = New-Object System.Drawing.Rectangle 0, ($top + $label), ([int]($i.W * $Scale)), ([int]($i.H * $Scale))
    $g.DrawImage($img, $dest, (New-Object System.Drawing.Rectangle $i.X, $i.Y, $i.W, $i.H), [System.Drawing.GraphicsUnit]::Pixel)
    $img.Dispose()
    $top += [int]($i.H * $Scale) + $label + 6
}
$g.Dispose()
$sheet.Save($Out); $sheet.Dispose()
"saved the sheet ($width x $height)"
