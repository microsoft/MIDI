# Copyright (c) Microsoft Corporation and Contributors.
# Licensed under the MIT License
# ============================================================================
# This is part of Windows MIDI Services
# Further information: https://aka.ms/midi
# ============================================================================
#
# Flags accessibility defects in the WinUI tools that are invisible until somebody runs a screen
# reader. Every rule here is one that has actually shipped in this repository.
#
#   pwsh -File build\check_accessibility.ps1
#   pwsh -File build\check_accessibility.ps1 -Path src\in-box\user-tools\midi-player
#
# Exits 1 when anything is found, so it can gate a build.
#
# A file that legitimately breaks one of these rules opts out with the marker below.
#
#   accessibility-check: ignore-file

[CmdletBinding()]
param(
    [string[]] $Path = @('src'),

    [string] $ExcludePattern = '\\(Generated Files|GeneratedFiles|obj|bin|packages|vcpkg|node_modules|VSFiles|vsfiles-sdk|\.git)\\',

    [switch] $Quiet
)

$ErrorActionPreference = 'Stop'

Add-Type -AssemblyName System.Xml.Linq

$xamlNs = 'http://schemas.microsoft.com/winfx/2006/xaml/presentation'
$xNs = 'http://schemas.microsoft.com/winfx/2006/xaml'

# A template root of one of these supplies no plain text, so the generated ComboBoxItem or
# ListViewItem has an EMPTY accessible name and a screen reader announces nothing. A root that IS
# a text element (TextBlock, TextBox, CheckBox with Content) is named from its own content.
$silentTemplateRoots = @(
    'Grid', 'StackPanel', 'Border', 'RelativePanel', 'Canvas', 'DockPanel',
    'ItemsStackPanel', 'VariableSizedWrapGrid', 'Viewbox', 'Expander'
)

$iconOnlyContent = @('FontIcon', 'PathIcon', 'SymbolIcon', 'BitmapIcon', 'ImageIcon', 'Image', 'Path', 'Ellipse', 'Rectangle')

$buttonTypes = @('Button', 'ToggleButton', 'HyperlinkButton', 'AppBarButton', 'RepeatButton', 'DropDownButton', 'SplitButton')

$roots = foreach ($p in ($Path -split ',' | ForEach-Object { $_.Trim() } | Where-Object { $_ }))
{
    $full = if ([System.IO.Path]::IsPathRooted($p)) { $p } else { Join-Path (Split-Path -Parent $PSScriptRoot) $p }
    if (Test-Path $full) { $full } else { Write-Warning "Skipping missing path: $p" }
}

if (-not $roots)
{
    Write-Error "None of the requested paths exist: $($Path -join ', ')"
    exit 2
}

$findings = [System.Collections.Generic.List[object]]::new()
$optOut = 'accessibility-check: ignore-file'

function Add-Finding([string]$file, [int]$line, [string]$rule, [string]$detail)
{
    $findings.Add([PSCustomObject]@{ File = $file; Line = $line; Rule = $rule; Detail = $detail })
}

function Get-Line($element)
{
    $info = [System.Xml.IXmlLineInfo]$element
    if ($info.HasLineInfo()) { return $info.LineNumber }
    return 0
}

# ----------------------------------------------------------------------------------------
# Rule 1: a .resw automation key whose separator is a slash never resolves, so the control is
# silently left with no name at all. This shipped in sysex-tool for months.
# ----------------------------------------------------------------------------------------
$reswFiles = Get-ChildItem -Path $roots -Recurse -File -Filter '*.resw' -ErrorAction SilentlyContinue |
    Where-Object { $_.FullName -notmatch $ExcludePattern }

$reswKeysByFolder = @{}

foreach ($resw in $reswFiles)
{
    $lines = [System.IO.File]::ReadAllLines($resw.FullName)
    if ($lines -match [regex]::Escape($optOut)) { continue }

    $keys = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)

    for ($i = 0; $i -lt $lines.Count; $i++)
    {
        $m = [regex]::Match($lines[$i], 'data name="([^"]+)"')
        if (-not $m.Success) { continue }

        $key = $m.Groups[1].Value
        $null = $keys.Add($key)

        if ($key -match '\][A-Za-z]+/[A-Za-z]+$')
        {
            Add-Finding $resw.FullName ($i + 1) 'MalformedAutomationKey' `
                "'$key' uses a slash. The separator must be a dot, or the value never reaches the control."
        }
    }

    # en-US is the authored language; other locales are translations of the same keys.
    if ($resw.FullName -match '\\en-US\\')
    {
        $appFolder = Split-Path (Split-Path (Split-Path $resw.FullName -Parent) -Parent) -Parent
        $reswKeysByFolder[$appFolder] = $keys
    }
}

# ----------------------------------------------------------------------------------------
# Rules 2 and 3, over the XAML.
# ----------------------------------------------------------------------------------------
$xamlFiles = Get-ChildItem -Path $roots -Recurse -File -Filter '*.xaml' -ErrorAction SilentlyContinue |
    Where-Object { $_.FullName -notmatch $ExcludePattern -and $_.Name -ne 'App.xaml' }

foreach ($xaml in $xamlFiles)
{
    $text = [System.IO.File]::ReadAllText($xaml.FullName)
    if ($text.Contains($optOut)) { continue }

    try { $doc = [System.Xml.Linq.XDocument]::Parse($text, [System.Xml.Linq.LoadOptions]::SetLineInfo) }
    catch { Write-Warning "Could not parse $($xaml.FullName): $($_.Exception.Message)"; continue }

    # The app folder owns the .resw, so walk up until one is known.
    $keys = $null
    $probe = Split-Path $xaml.FullName -Parent
    while ($probe -and -not $keys)
    {
        if ($reswKeysByFolder.ContainsKey($probe)) { $keys = $reswKeysByFolder[$probe] }
        $probe = Split-Path $probe -Parent
    }

    function Test-HasName($element)
    {
        if ($element.Attribute('AutomationProperties.Name')) { return $true }
        if ($element.Attribute('AutomationProperties.LabeledBy')) { return $true }
        return $false
    }

    function Test-HasReswName($element)
    {
        $uid = $element.Attribute([System.Xml.Linq.XName]::Get('Uid', $xNs))
        if (-not $uid -or -not $keys) { return $false }

        $wanted = "$($uid.Value).[using:Microsoft.UI.Xaml.Automation]AutomationProperties.Name"
        if ($keys.Contains($wanted)) { return $true }

        # A Header or Content from resources names the control just as well.
        return $keys.Contains("$($uid.Value).Header") -or $keys.Contains("$($uid.Value).Content")
    }

    # Rule 2: an item template whose root is a layout panel and carries no name.
    foreach ($template in $doc.Descendants([System.Xml.Linq.XName]::Get('DataTemplate', $xamlNs)))
    {
        $root = $template.Elements() | Select-Object -First 1
        if (-not $root) { continue }
        if ($silentTemplateRoots -notcontains $root.Name.LocalName) { continue }
        if (Test-HasName $root) { continue }

        $dataType = $template.Attribute([System.Xml.Linq.XName]::Get('DataType', $xNs))
        $which = if ($dataType) { $dataType.Value } else { $template.Attribute([System.Xml.Linq.XName]::Get('Key', $xNs)).Value }

        Add-Finding $xaml.FullName (Get-Line $root) 'UnnamedTemplateRoot' `
            "DataTemplate for $which has a <$($root.Name.LocalName)> root with no AutomationProperties.Name, so its list or combo items announce nothing."
    }

    # Rule 3: a button whose only content is an icon, with nothing to announce.
    foreach ($button in $doc.Descendants() | Where-Object { $buttonTypes -contains $_.Name.LocalName })
    {
        if (Test-HasName $button) { continue }
        if (Test-HasReswName $button) { continue }

        $contentAttribute = $button.Attribute('Content')
        if ($contentAttribute -and $contentAttribute.Value.Trim()) { continue }

        # Any descendant that carries text is enough for XAML to name the button from it.
        $hasText = $button.Descendants() | Where-Object {
            $_.Name.LocalName -in @('TextBlock', 'TextBox', 'RichTextBlock') -or
            $_.Attribute([System.Xml.Linq.XName]::Get('Uid', $xNs))
        }
        if ($hasText) { continue }

        $iconChildren = $button.Descendants() | Where-Object { $iconOnlyContent -contains $_.Name.LocalName }
        if (-not $iconChildren) { continue }

        $name = $button.Attribute([System.Xml.Linq.XName]::Get('Name', $xNs))
        $label = if ($name) { $name.Value } else { "<$($button.Name.LocalName)>" }

        Add-Finding $xaml.FullName (Get-Line $button) 'UnnamedIconButton' `
            "$label shows only an icon and has no accessible name. A ToolTip is not a name."
    }
}

if (-not $Quiet)
{
    foreach ($group in $findings | Group-Object File | Sort-Object Name)
    {
        Write-Host ""
        Write-Host $group.Name -ForegroundColor Cyan

        foreach ($f in $group.Group | Sort-Object Line)
        {
            Write-Host ("  {0,5}  {1,-22} {2}" -f $f.Line, $f.Rule, $f.Detail)
        }
    }

    Write-Host ""
    if ($findings.Count -eq 0)
    {
        Write-Host "Scanned $($xamlFiles.Count) XAML and $($reswFiles.Count) resource files. No accessibility defects found." -ForegroundColor Green
    }
    else
    {
        Write-Host "Scanned $($xamlFiles.Count) XAML and $($reswFiles.Count) resource files. Found $($findings.Count) accessibility defect(s)." -ForegroundColor Yellow
        $findings | Group-Object Rule | Sort-Object Count -Descending |
            ForEach-Object { Write-Host ("  {0,5}  {1}" -f $_.Count, $_.Name) }
    }
}

if ($findings.Count -gt 0) { exit 1 }
exit 0
