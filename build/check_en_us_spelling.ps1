# Copyright (c) Microsoft Corporation and Contributors.
# Licensed under the MIT License
# ============================================================================
# This is part of Windows MIDI Services
# Further information: https://aka.ms/midi
# ============================================================================
#
# Flags en-GB spellings in authored text: UI strings, comments, identifiers and documentation.
# Windows MIDI Services ships in en-US, so "colour", "centre" and "cancelled" are bugs.
#
#   pwsh -File build\check_en_us_spelling.ps1
#   pwsh -File build\check_en_us_spelling.ps1 -Path src\api\Client\WinRT\user-tools
#
# Exits 1 when anything is found, so it can gate a build.
#
# A file that legitimately contains en-GB spellings (a word list, a style guide) opts out by
# containing the marker below. This script and the matching .instructions.md both do.
#
#   en-us-spelling-check: ignore-file

[CmdletBinding()]
param(
    # Defaults to the folders that contain hand-written text.
    [string[]] $Path = @('src', 'docs', 'samples', 'build'),

    [string[]] $Include = @(
        '*.md', '*.resw', '*.resx', '*.rc', '*.cpp', '*.h', '*.hpp', '*.c',
        '*.cs', '*.idl', '*.xaml', '*.ps1', '*.psm1', '*.psd1', '*.json', '*.yml', '*.yaml'
    ),

    # Third party, generated, and build output. These are not ours to reword.
    [string] $ExcludePattern = '\\(vcpkg|vcpkg_installed|node_modules|_site|obj|bin|packages|Generated Files|GeneratedFiles|intermediate|VSFiles|vsfiles-sdk|nuke_build|\.git)\\|\\build\\(release|staging)\\',

    # Counts per term and per folder instead of every hit. For triaging a large backlog.
    [switch] $Summary,

    [switch] $Quiet
)

$ErrorActionPreference = 'Stop'

# en-GB spelling -> en-US replacement.
#
# Every entry is matched as a whole word, case-insensitively. Only spellings that are ALWAYS
# wrong in en-US belong here, so a hit is never a judgement call:
#   - "cancellation" is spelled the same in both, so only cancelled/cancelling appear below
#   - "analyses" is a correct en-US noun, so only analyse/analysed/analysing appear
#   - "advertise", "otherwise", "surprise" and friends are correct en-US, so no bare -ise rule
#   - "dialogue" is deliberately absent: correct en-US for conversation, though UI uses "dialog"
#   - "spectre" is deliberately absent: this repo uses Spectre.Console and the /Qspectre switch
#   - "grey" is deliberately absent: it is third party API surface here (dye::grey, on_grey,
#     Spectre's [grey] markup) and an author's name in a sample. Prefer "gray" in prose anyway.
$replacements = [ordered]@{
    # doubled consonant before a suffix
    'cancelled'       = 'canceled'
    'cancelling'      = 'canceling'
    'travelled'       = 'traveled'
    'travelling'      = 'traveling'
    'traveller'       = 'traveler'
    'labelled'        = 'labeled'
    'labelling'       = 'labeling'
    'modelled'        = 'modeled'
    'modelling'       = 'modeling'
    'signalled'       = 'signaled'
    'signalling'      = 'signaling'
    'levelled'        = 'leveled'
    'levelling'       = 'leveling'
    'fuelled'         = 'fueled'
    'fuelling'        = 'fueling'
    'totalled'        = 'totaled'
    'totalling'       = 'totaling'
    'marshalled'      = 'marshaled'
    'marshalling'     = 'marshaling'

    # single consonant where en-US doubles
    'fulfil'          = 'fulfill'
    'instil'          = 'instill'
    'enrol'           = 'enroll'
    'distil'          = 'distill'
    'appal'           = 'appall'
    'skilful'         = 'skillful'
    'wilful'          = 'willful'

    # -our -> -or
    'colour'          = 'color'
    'colours'         = 'colors'
    'coloured'        = 'colored'
    'colouring'       = 'coloring'
    'colourful'       = 'colorful'
    'behaviour'       = 'behavior'
    'behaviours'      = 'behaviors'
    'behavioural'     = 'behavioral'
    'honour'          = 'honor'
    'honours'         = 'honors'
    'honoured'        = 'honored'
    'honouring'       = 'honoring'
    'favour'          = 'favor'
    'favours'         = 'favors'
    'favoured'        = 'favored'
    'favourite'       = 'favorite'
    'neighbour'       = 'neighbor'
    'neighbours'      = 'neighbors'
    'labour'          = 'labor'
    'endeavour'       = 'endeavor'
    'rumour'          = 'rumor'
    'humour'          = 'humor'
    'armour'          = 'armor'
    'vapour'          = 'vapor'
    'flavour'         = 'flavor'
    'harbour'         = 'harbor'

    # -re -> -er
    'centre'          = 'center'
    'centres'         = 'centers'
    'centred'         = 'centered'
    'centring'        = 'centering'
    'metre'           = 'meter'
    'metres'          = 'meters'
    'fibre'           = 'fiber'
    'calibre'         = 'caliber'
    'manoeuvre'       = 'maneuver'

    # -ce -> -se
    'licence'         = 'license'
    'defence'         = 'defense'
    'offence'         = 'offense'
    'pretence'        = 'pretense'
    'practise'        = 'practice'

    # -ise / -isation -> -ize / -ization
    'analyse'         = 'analyze'
    'analysed'        = 'analyzed'
    'analysing'       = 'analyzing'
    'apologise'       = 'apologize'
    'authorise'       = 'authorize'
    'authorised'      = 'authorized'
    'authorisation'   = 'authorization'
    'categorise'      = 'categorize'
    'characterise'    = 'characterize'
    'customise'       = 'customize'
    'customised'      = 'customized'
    'digitise'        = 'digitize'
    'emphasise'       = 'emphasize'
    'equalise'        = 'equalize'
    'familiarise'     = 'familiarize'
    'finalise'        = 'finalize'
    'formalise'       = 'formalize'
    'generalise'      = 'generalize'
    'harmonise'       = 'harmonize'
    'initialise'      = 'initialize'
    'initialised'     = 'initialized'
    'initialising'    = 'initializing'
    'initialisation'  = 'initialization'
    'itemise'         = 'itemize'
    'localise'        = 'localize'
    'localised'       = 'localized'
    'localisation'    = 'localization'
    'maximise'        = 'maximize'
    'maximised'       = 'maximized'
    'memorise'        = 'memorize'
    'minimise'        = 'minimize'
    'minimised'       = 'minimized'
    'mobilise'        = 'mobilize'
    'modernise'       = 'modernize'
    'neutralise'      = 'neutralize'
    'normalise'       = 'normalize'
    'normalised'      = 'normalized'
    'normalisation'   = 'normalization'
    'optimise'        = 'optimize'
    'optimised'       = 'optimized'
    'optimisation'    = 'optimization'
    'organise'        = 'organize'
    'organised'       = 'organized'
    'organisation'    = 'organization'
    'packetise'       = 'packetize'
    'packetised'      = 'packetized'
    'packetisation'   = 'packetization'
    'penalise'        = 'penalize'
    'personalise'     = 'personalize'
    'prioritise'      = 'prioritize'
    'prioritised'     = 'prioritized'
    'randomise'       = 'randomize'
    'rationalise'     = 'rationalize'
    'realise'         = 'realize'
    'realised'        = 'realized'
    'recognise'       = 'recognize'
    'recognised'      = 'recognized'
    'sanitise'        = 'sanitize'
    'sanitised'       = 'sanitized'
    'serialise'       = 'serialize'
    'serialised'      = 'serialized'
    'serialisation'   = 'serialization'
    'specialise'      = 'specialize'
    'specialised'     = 'specialized'
    'stabilise'       = 'stabilize'
    'standardise'     = 'standardize'
    'sterilise'       = 'sterilize'
    'summarise'       = 'summarize'
    'summarised'      = 'summarized'
    'symmetrise'      = 'symmetrize'
    'sympathise'      = 'sympathize'
    'synchronise'     = 'synchronize'
    'synchronised'    = 'synchronized'
    'synchronisation' = 'synchronization'
    'tokenise'        = 'tokenize'
    'utilise'         = 'utilize'
    'utilised'        = 'utilized'
    'utilisation'     = 'utilization'
    'virtualise'      = 'virtualize'
    'visualise'       = 'visualize'
    'visualised'      = 'visualized'

    # -ogue -> -og
    'analogue'        = 'analog'
    'catalogue'       = 'catalog'
    'catalogues'      = 'catalogs'

    # miscellaneous
    'programme'       = 'program'
    'artefact'        = 'artifact'
    'artefacts'       = 'artifacts'
    'judgement'       = 'judgment'
    'acknowledgement' = 'acknowledgment'
    'aluminium'       = 'aluminum'
    'enquire'         = 'inquire'
    'enquiry'         = 'inquiry'
    'whilst'          = 'while'
    'amongst'         = 'among'
    'learnt'          = 'learned'
    'spelt'           = 'spelled'
}

# Two passes, because identifiers do not use word boundaries the way prose does.
#   prose / SNAKE_CASE / kebab-case : bounded by anything that is not a letter
#   camelCase / PascalCase          : a lowercase-to-uppercase hump, e.g. SendCancelled
$sorted = $replacements.Keys | Sort-Object { $_.Length } -Descending
$alternation = $sorted -join '|'
$humped = ($sorted | ForEach-Object { $_.Substring(0, 1).ToUpperInvariant() + $_.Substring(1) }) -join '|'

$regexes = @(
    [regex]::new("(?i)(?<![A-Za-z])($alternation)(?![A-Za-z])", 'Compiled')
    [regex]::new("(?<=[a-z0-9])($humped)(?![a-z])", 'Compiled')
)

$roots = foreach ($p in ($Path -split ',' | ForEach-Object { $_.Trim() } | Where-Object { $_ }))
{
    $full = if ([System.IO.Path]::IsPathRooted($p)) { $p } else { Join-Path (Split-Path -Parent $PSScriptRoot) $p }
    if (Test-Path $full) { $full } else { Write-Warning "Skipping missing path: $p" }
}

# Without this, Get-ChildItem -Path $null quietly walks the current directory, so a typo in
# -Path reports on the whole repo and looks like a real result.
if (-not $roots)
{
    Write-Error "None of the requested paths exist: $($Path -join ', ')"
    exit 2
}

$files = Get-ChildItem -Path $roots -Recurse -File -Include $Include -ErrorAction SilentlyContinue |
    Where-Object { $_.FullName -notmatch $ExcludePattern }

$findings = [System.Collections.Generic.List[object]]::new()
$optOut = 'en-us-spelling-check: ignore-file'

foreach ($file in $files)
{
    $lineNumber = 0
    $lines = [System.IO.File]::ReadAllLines($file.FullName)

    if ($lines -match [regex]::Escape($optOut)) { continue }

    foreach ($line in $lines)
    {
        $lineNumber++

        foreach ($regex in $regexes)
        {
            foreach ($match in $regex.Matches($line))
            {
                $findings.Add([PSCustomObject]@{
                    File       = $file.FullName
                    Line       = $lineNumber
                    Found      = $match.Value
                    Suggestion = $replacements[$match.Value.ToLowerInvariant()]
                    Text       = $line.Trim()
                })
            }
        }
    }
}

if ($Summary -and -not $Quiet)
{
    Write-Host ""
    Write-Host "By term" -ForegroundColor Cyan
    $findings | Group-Object { $_.Found.ToLowerInvariant() } | Sort-Object Count -Descending |
        ForEach-Object { Write-Host ("  {0,5}  {1} -> {2}" -f $_.Count, $_.Name, $replacements[$_.Name]) }

    Write-Host ""
    Write-Host "By area" -ForegroundColor Cyan
    $repoRoot = Split-Path -Parent $PSScriptRoot
    $findings | Group-Object { ($_.File.Replace($repoRoot + '\', '') -split '\\')[0..2] -join '\' } |
        Sort-Object Count -Descending | Select-Object -First 20 |
        ForEach-Object { Write-Host ("  {0,5}  {1}" -f $_.Count, $_.Name) }
}
elseif (-not $Quiet)
{
    foreach ($group in $findings | Group-Object File | Sort-Object Name)
    {
        Write-Host ""
        Write-Host $group.Name -ForegroundColor Cyan

        foreach ($f in $group.Group)
        {
            $snippet = if ($f.Text.Length -gt 100) { $f.Text.Substring(0, 100) + '...' } else { $f.Text }
            Write-Host ("  line {0,-6} {1} -> {2}" -f $f.Line, $f.Found, $f.Suggestion) -ForegroundColor Yellow
            Write-Host ("               {0}" -f $snippet) -ForegroundColor DarkGray
        }
    }
}

if (-not $Quiet)
{
    Write-Host ""
    Write-Host ("Scanned {0} files. Found {1} en-GB spelling(s)." -f $files.Count, $findings.Count)
}

if ($findings.Count -gt 0) { exit 1 }

exit 0
