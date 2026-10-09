# PROTOTYPE. Drives midi-mcp-spike.exe over stdio the way an MCP host does, in both protocol eras,
# and checks every answer. Writes drafts only into a throwaway folder under %TEMP%, never into
# Documents. Opens no MIDI connection and sends no MIDI: none of the tools can.
#
#   pwsh -File Invoke-McpHarness.ps1 [-Exe <path>] [-KeepFiles]

param(
    [string] $Exe = (Join-Path $PSScriptRoot '..\..\out\x64\Release\midi-mcp-spike.exe'),
    [switch] $KeepFiles
)

$ErrorActionPreference = 'Stop'
$Exe = (Resolve-Path $Exe).Path

$work = Join-Path $env:TEMP ("midi-mcp-harness-" + [guid]::NewGuid().ToString('N').Substring(0, 8))
$patchFolder = Join-Path $work 'MIDI Patches'
$layoutFolder = Join-Path $work 'MIDI Layouts'
New-Item -ItemType Directory -Path $patchFolder, $layoutFolder -Force | Out-Null

$shots = Join-Path $PSScriptRoot '..\..\out\harness'
New-Item -ItemType Directory -Path $shots -Force | Out-Null

$script:passed = 0
$script:failed = 0
$script:nextId = 1

function Check([string] $name, [bool] $condition, [string] $detail = '') {
    if ($condition) {
        $script:passed++
        Write-Host "PASS  $name"
    }
    else {
        $script:failed++
        Write-Host "FAIL  $name  $detail" -ForegroundColor Red
    }
}

function Start-Server([string[]] $arguments) {
    $info = [System.Diagnostics.ProcessStartInfo]::new($Exe)
    foreach ($a in $arguments) { $info.ArgumentList.Add($a) }
    $info.RedirectStandardInput = $true
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    $info.UseShellExecute = $false
    $info.CreateNoWindow = $true
    $info.StandardInputEncoding = [System.Text.UTF8Encoding]::new($false)
    $info.StandardOutputEncoding = [System.Text.UTF8Encoding]::new($false)
    $process = [System.Diagnostics.Process]::Start($info)
    $process.StandardInput.AutoFlush = $true
    return $process
}

function Send-Line($process, [string] $line) {
    $process.StandardInput.Write($line + "`n")
}

function Read-Reply($process, [int] $timeoutMs = 60000) {
    $task = $process.StandardOutput.ReadLineAsync()
    if (-not $task.Wait($timeoutMs)) { throw "No reply within $timeoutMs ms" }
    return $task.Result
}

function Invoke-Rpc($process, [string] $method, $params, [string] $era) {
    $id = $script:nextId++
    $message = [ordered]@{ jsonrpc = '2.0'; id = $id; method = $method }

    if ($era -eq 'modern') {
        if ($null -eq $params) { $params = [ordered]@{} }
        $params['_meta'] = [ordered]@{
            'io.modelcontextprotocol/protocolVersion'    = '2026-07-28'
            'io.modelcontextprotocol/clientInfo'         = @{ name = 'midi-mcp-harness'; version = '1.0' }
            'io.modelcontextprotocol/clientCapabilities' = @{}
        }
    }

    if ($null -ne $params) { $message['params'] = $params }

    Send-Line $process ($message | ConvertTo-Json -Depth 40 -Compress)
    $raw = Read-Reply $process
    $reply = $raw | ConvertFrom-Json -Depth 60
    if ($reply.id -ne $id) { throw "Reply id $($reply.id) does not match request id $id" }
    return $reply
}

function Invoke-Tool($process, [string] $name, $arguments, [string] $era = 'modern') {
    return Invoke-Rpc $process 'tools/call' ([ordered]@{ name = $name; arguments = $arguments }) $era
}

function Text-Of($reply) {
    return (($reply.result.content | Where-Object type -eq 'text') | ForEach-Object text) -join "`n"
}

# The endpoints as the model reads them in the text.
function Endpoints-Of($reply) {
    $pattern = '(?m)^\d+\. (?<name>.+?)(?: \[(?<transport>[A-Z0-9]+)\])?(?:, [^\r\n]*)?\r?\n {3}id: (?<id>\S+)(?<details>(?:\r?\n {3}[^\r\n]*)*)'
    foreach ($m in [regex]::Matches((Text-Of $reply), $pattern)) {
        [pscustomobject]@{ name = $m.Groups['name'].Value; transport = $m.Groups['transport'].Value; id = $m.Groups['id'].Value; details = $m.Groups['details'].Value }
    }
}

# ------------------------------------------------------------------------------------------------
Write-Host "`n== Protocol, handshake era (2025-06-18) ==" -ForegroundColor Cyan
$server = Start-Server @('--app', 'all', '--patch-folder', $patchFolder, '--layout-folder', $layoutFolder)

$init = Invoke-Rpc $server 'initialize' ([ordered]@{
        protocolVersion = '2025-06-18'
        capabilities    = @{}
        clientInfo      = @{ name = 'Legacy Harness'; version = '1.0' }
    }) 'legacy'
Check 'initialize echoes a supported version' ($init.result.protocolVersion -eq '2025-06-18')
Check 'initialize names the server' ($init.result.serverInfo.name -like 'windows-midi*')
Check 'initialize declares tools' ($null -ne $init.result.capabilities.tools)
Check 'initialize carries instructions' ($init.result.instructions.Length -gt 100)

Send-Line $server '{"jsonrpc":"2.0","method":"notifications/initialized"}'

$ping = Invoke-Rpc $server 'ping' $null 'legacy'
Check 'ping answers' ($null -ne $ping.result)

$legacyList = Invoke-Rpc $server 'tools/list' $null 'legacy'
$toolNames = @($legacyList.result.tools | ForEach-Object name)
Check 'tools/list returns every tool' ($toolNames.Count -eq 8) ($toolNames -join ', ')
Check 'handshake era has no resultType' ($null -eq $legacyList.result.resultType)
Check 'read only tools say so' (($legacyList.result.tools | Where-Object name -eq 'preview_patch').annotations.readOnlyHint -eq $true)
Check 'saving tools say they write' (($legacyList.result.tools | Where-Object name -eq 'save_patch_draft').annotations.readOnlyHint -eq $false)

$unknownVersion = Invoke-Rpc $server 'initialize' ([ordered]@{ protocolVersion = '1999-01-01'; capabilities = @{}; clientInfo = @{ name = 'x' } }) 'legacy'
Check 'initialize offers its newest handshake version for an unknown one' ($unknownVersion.result.protocolVersion -eq '2025-11-25')

# ------------------------------------------------------------------------------------------------
Write-Host "`n== Protocol, stateless era (2026-07-28) ==" -ForegroundColor Cyan

$discover = Invoke-Rpc $server 'server/discover' $null 'modern'
Check 'server/discover lists the stateless version' ($discover.result.supportedVersions -contains '2026-07-28')
Check 'server/discover lists the handshake versions too' ($discover.result.supportedVersions -contains '2025-06-18')
Check 'server/discover is complete' ($discover.result.resultType -eq 'complete')
Check 'server/discover can be cached' ($discover.result.ttlMs -gt 0 -and $discover.result.cacheScope -eq 'private')
Check 'server/discover names the server in _meta' ($null -ne $discover.result._meta.'io.modelcontextprotocol/serverInfo'.name)

$modernList = Invoke-Rpc $server 'tools/list' $null 'modern'
Check 'stateless tools/list is complete and cacheable' ($modernList.result.resultType -eq 'complete' -and $modernList.result.ttlMs -gt 0)
Check 'tool order is the same in both eras' ((@($modernList.result.tools | ForEach-Object name) -join ',') -eq ($toolNames -join ','))

$id = $script:nextId++
Send-Line $server ('{"jsonrpc":"2.0","id":' + $id + ',"method":"tools/list","params":{"_meta":{"io.modelcontextprotocol/protocolVersion":"1900-01-01"}}}')
$bad = Read-Reply $server | ConvertFrom-Json
Check 'unknown version is refused with -32022' ($bad.error.code -eq -32022)
Check 'refusal lists what is supported' ($bad.error.data.supported -contains '2026-07-28' -and $bad.error.data.requested -eq '1900-01-01')

# ------------------------------------------------------------------------------------------------
Write-Host "`n== Protocol errors ==" -ForegroundColor Cyan

Send-Line $server 'this is not json'
$parse = Read-Reply $server | ConvertFrom-Json
Check 'garbage is a parse error' ($parse.error.code -eq -32700)

Send-Line $server '[{"jsonrpc":"2.0","id":1,"method":"ping"}]'
$batch = Read-Reply $server | ConvertFrom-Json
Check 'a batch is refused' ($batch.error.code -eq -32600)

$missing = Invoke-Rpc $server 'resources/list' $null 'modern'
Check 'unknown method is -32601' ($missing.error.code -eq -32601)

$noTool = Invoke-Tool $server 'launch_rockets' @{}
Check 'unknown tool is -32602' ($noTool.error.code -eq -32602)

# A notification gets no answer, so the next reply belongs to the next request.
Send-Line $server '{"jsonrpc":"2.0","method":"notifications/roots/list_changed","params":{}}'
$after = Invoke-Rpc $server 'ping' $null 'legacy'
Check 'a notification is not answered' ($null -ne $after.result)

# ------------------------------------------------------------------------------------------------
Write-Host "`n== list_midi_endpoints ==" -ForegroundColor Cyan

$endpoints = Invoke-Tool $server 'list_midi_endpoints' @{}
$list = @(Endpoints-Of $endpoints)
Check 'endpoints are listed' ($list.Count -gt 0) "count $($list.Count)"
Check 'results carry no structuredContent, which VS Code would give the model instead of the text' ($null -eq $endpoints.result.PSObject.Properties['structuredContent'])
$loopA = $list | Where-Object { $_.name -eq 'Default App Loopback (A)' } | Select-Object -First 1
$loopB = $list | Where-Object { $_.name -eq 'Default App Loopback (B)' } | Select-Object -First 1
$bloop = $list | Where-Object { $_.transport -eq 'BLOOP' } | Select-Object -First 1
Check 'the default loopback pair is there' ($null -ne $loopA -and $null -ne $loopB)
Check 'a basic loopback is there' ($null -ne $bloop)
Check 'a loopback names its partner' ($loopA.details -match 'comes out of "Default App Loopback \(B\)"')
Check 'no serial numbers are handed to the model' (-not ((Text-Of $endpoints) -match 'serial'))
Check 'groups are numbered from 1' ($loopA.details -match 'sends on: group 1' -and (Text-Of $endpoints) -notmatch '\bgroup 0\b')

$filtered = Invoke-Tool $server 'list_midi_endpoints' @{ nameContains = 'Loopback' }
Check 'nameContains narrows the list' (@(Endpoints-Of $filtered).Count -lt $list.Count -or $list.Count -le 3)

# ------------------------------------------------------------------------------------------------
Write-Host "`n== MIDI Patchbay ==" -ForegroundColor Cyan

# Every loopback here declares one group, so the upper half of the split goes to a second
# loopback pair when this PC has one, and otherwise comes from the other side of the default pair.
$otherLoop = $list | Where-Object { $_.transport -eq 'LOOP' -and $_.id -notmatch 'loop_[ab]_default' } | Select-Object -First 1
$upperFrom = if ($otherLoop) { 'Default App Loopback (A)' } else { 'Default App Loopback (B)' }
$upperTo = if ($otherLoop) { $otherLoop.name } else { $bloop.name }

$split = [ordered]@{
    name        = 'Keyboard split'
    description = 'Lower half to the bass, upper half to the pad'
    request     = 'Split my keyboard at middle C'
    routes      = @(
        [ordered]@{
            from       = @{ endpoint = 'Default App Loopback (A)'; group = 1 }
            to         = @{ endpoint = $bloop.name; group = 1 }
            channels   = @(1)
            messages   = @('notes', 'pitchBend')
            noteRange  = @{ lowest = 0; highest = 59 }
            transposeSemitones = 12
            velocity   = @{ curve = 'fixed'; fixedPercent = 80 }
        },
        [ordered]@{
            from       = @{ endpoint = $upperFrom; group = 1 }
            to         = @{ endpoint = $upperTo }
            noteRange  = @{ lowest = 60; highest = 127 }
            block      = @('clock', 'activeSensing')
            channelMap = @(@{ from = 1; to = 2 })
        }
    )
}

$preview = Invoke-Tool $server 'preview_patch' $split
$previewText = Text-Of $preview
Check 'a good patch previews as ready' ($previewText -match '^This patch is ready to save as a draft') $previewText
Check 'the preview explains the split' ($previewText -match 'transposed up 12' -and $previewText -match 'only channel 1' -and $previewText -match 'no timing clock')
Check 'the preview names the notes' ($previewText -match '\(0 to 59\)' -and $previewText -match '\(60 to 127\)')
Check 'preview writes nothing' ((Get-ChildItem $patchFolder).Count -eq 0)

$loop = Invoke-Tool $server 'preview_patch' ([ordered]@{
        name   = 'Feedback'
        routes = @(@{ from = @{ endpoint = 'Default App Loopback (A)' }; to = @{ endpoint = 'Default App Loopback (B)' } })
    })
Check 'a loop through a loopback pair is caught' ((Text-Of $loop) -match '^This patch cannot be saved yet' -and (Text-Of $loop) -match 'feedback loop')

$ambiguous = Invoke-Tool $server 'preview_patch' ([ordered]@{
        name   = 'Which one'
        routes = @(@{ from = @{ endpoint = 'Loopback' }; to = @{ endpoint = $bloop.name } })
    })
Check 'an ambiguous name comes back as a question' ((Text-Of $ambiguous) -match 'Ask the customer which one')

$badKind = Invoke-Tool $server 'preview_patch' ([ordered]@{
        name   = 'Bad kind'
        routes = @(@{ from = @{ endpoint = 'Default App Loopback (A)' }; to = @{ endpoint = $bloop.name }; messages = @('drums') })
    })
Check 'an unknown message kind lists the real ones' ((Text-Of $badKind) -match 'controlChanges')

$badGroup = Invoke-Tool $server 'preview_patch' ([ordered]@{
        name   = 'Bad group'
        routes = @(@{ from = @{ endpoint = 'Default App Loopback (A)'; group = 17 }; to = @{ endpoint = $bloop.name } })
    })
Check 'a group out of range is refused' ((Text-Of $badGroup) -match 'group must be 1 to 16')

$saved = Invoke-Tool $server 'save_patch_draft' $split
Check 'save_patch_draft succeeds' ($saved.result.isError -eq $false) (Text-Of $saved)
$file = Get-ChildItem $patchFolder -Filter '*.midipatch' | Select-Object -First 1
Check 'the draft file exists' ($null -ne $file)

if ($file) {
    $patch = Get-Content $file.FullName -Raw | ConvertFrom-Json -Depth 40
    Check 'the draft does not route at startup' ($patch.activateAtStartup -eq $false)
    Check 'the draft records who wrote it' ($patch._draft.by -eq 'midi-mcp-harness')
    Check 'the draft records the request' ($patch._draft.request -eq 'Split my keyboard at middle C')
    Check 'three endpoints, two connections' ($patch.endpoints.Count -eq 3 -and $patch.connections.Count -eq 2)
    $first = $patch.connections[0]
    Check 'filter allows channel 1 only' ($first.filter.channels -eq 1)
    Check 'filter allows voice message types only' ($first.filter.messageTypes -eq 20)
    Check 'filter allows note off, note on and pitch bend' ($first.filter.channelVoiceStatuses -eq (0x100 + 0x200 + 0x4000))
    Check 'filter drops every system message' ($first.filter.systemMessages -eq 0)
    Check 'note range is 0 to 59' ($first.filter.limitNoteRange -and $first.filter.lowestNote -eq 0 -and $first.filter.highestNote -eq 59)
    Check 'transpose is +12' ($first.transform.transposeSemitones -eq 12)
    Check 'velocity is fixed at 80 percent' ($first.transform.velocityCurve -eq 3 -and $first.transform.fixedVelocityPercent -eq 80)
    $second = $patch.connections[1]
    Check 'group 1 to all groups is stored as 0 and -1' ($second.sourceGroup -eq 0 -and $second.destinationGroup -eq -1)
    Check 'blocking clock clears only its bit' ($second.filter.systemMessages -eq (0x3FF -band -bnot (0x10 + 0x100)))
    Check 'channel map is stored 0 based' ($second.transform.channelMap[0].from -eq 0 -and $second.transform.channelMap[0].to -eq 1)
    Check 'match uses the service configuration keys' ($null -ne $patch.endpoints[0].match.endpointDeviceId)

    # The file judged by MIDI Patchbay's own filter and transform code, not by the code that wrote it.
    $oracle = Join-Path $PSScriptRoot '..\..\out\oracle\patch-oracle.exe'

    if (Test-Path $oracle) {
        $lower = @(& $oracle $file.FullName 0 20902864 20904664 20912864 20B00740 20E00040 10F80000)
        Check 'Patchbay itself: low note on channel 1 goes up an octave at 80% velocity' ($lower[0] -eq '20902864 20903466') $lower[0]
        Check 'Patchbay itself: a note above the split is dropped' ($lower[1] -eq '20904664 dropped') $lower[1]
        Check 'Patchbay itself: channel 2 is dropped' ($lower[2] -eq '20912864 dropped') $lower[2]
        Check 'Patchbay itself: a controller is dropped' ($lower[3] -eq '20B00740 dropped') $lower[3]
        Check 'Patchbay itself: pitch bend passes untouched' ($lower[4] -eq '20E00040 20E00040') $lower[4]
        Check 'Patchbay itself: clock is dropped' ($lower[5] -eq '10F80000 dropped') $lower[5]

        $upper = @(& $oracle $file.FullName 1 20904664 20902864 10F80000 10FE0000 10FA0000 20B00740)
        Check 'Patchbay itself: high note moves to channel 2' ($upper[0] -eq '20904664 20914664') $upper[0]
        Check 'Patchbay itself: a note below the split is dropped' ($upper[1] -eq '20902864 dropped') $upper[1]
        Check 'Patchbay itself: blocked clock is dropped' ($upper[2] -eq '10F80000 dropped') $upper[2]
        Check 'Patchbay itself: blocked active sensing is dropped' ($upper[3] -eq '10FE0000 dropped') $upper[3]
        Check 'Patchbay itself: start still passes' ($upper[4] -eq '10FA0000 10FA0000') $upper[4]
        Check 'Patchbay itself: a controller moves to channel 2' ($upper[5] -eq '20B00740 20B10740') $upper[5]
    }
    else {
        Write-Host "SKIP  Patchbay oracle not built (run patch-oracle\build-oracle.ps1)"
    }
}

$again = Invoke-Tool $server 'save_patch_draft' $split
Check 'a second save never replaces the first' ((Get-ChildItem $patchFolder -Filter '*.midipatch').Count -eq 2)

$patches = Invoke-Tool $server 'list_patches' @{}
Check 'list_patches shows the drafts' ((Text-Of $patches) -match '\[draft by midi-mcp-harness')

# The apps rename old .json files only when they next start.
[IO.File]::WriteAllText((Join-Path $patchFolder 'Old build.midipatch.json'), (@{ fileVersion = 1; name = 'Old build'; activateAtStartup = $false; endpoints = @(); connections = @() } | ConvertTo-Json -Depth 5))
Check 'list_patches still reads the old .midipatch.json extension' ((Text-Of (Invoke-Tool $server 'list_patches' @{})) -match '"Old build"')

# A saved patch that routes at startup can close a loop the draft cannot close alone.
$startup = [ordered]@{
    fileVersion = 1; name = 'Already routing'; activateAtStartup = $true
    endpoints   = @(
        @{ id = 'n1'; displayName = $bloop.name; match = @{ endpointDeviceId = $bloop.id } },
        @{ id = 'n2'; displayName = 'Default App Loopback (A)'; match = @{ endpointDeviceId = $loopA.id } }
    )
    connections = @(@{ id = 'c1'; sourceEndpointId = 'n1'; sourceGroup = -1; destinationEndpointId = 'n2'; destinationGroup = -1 })
}
[IO.File]::WriteAllText((Join-Path $patchFolder 'Already routing.midipatch'), ($startup | ConvertTo-Json -Depth 20))

$crossLoop = Invoke-Tool $server 'preview_patch' ([ordered]@{
        name   = 'Closes the circle'
        routes = @(@{ from = @{ endpoint = 'Default App Loopback (B)' }; to = @{ endpoint = $bloop.name } })
    })
Check 'a loop closed by another saved patch is caught and named' ((Text-Of $crossLoop) -match 'together with the saved patch "Already routing"')

# ------------------------------------------------------------------------------------------------
Write-Host "`n== MIDI Glass ==" -ForegroundColor Cyan

$controls = Invoke-Tool $server 'list_glass_controls' @{}
$kinds = @([regex]::Matches((Text-Of $controls), '(?m)^- (\w+): ') | ForEach-Object { $_.Groups[1].Value })
Check 'the control kinds come from the app' ($kinds.Count -ge 20 -and $kinds -contains 'fader' -and $kinds -contains 'xyPad') ($kinds -join ', ')
Check 'every kind has a note for the model' (-not ((Text-Of $controls) -match '\(no note\)'))

$faders = @(1..8 | ForEach-Object { [ordered]@{ kind = 'fader'; label = "Ch $_"; hue = 1; sends = @(@{ kind = 'controlChange'; channel = $_; number = 7 }) } })
$knobs = @(1..8 | ForEach-Object { [ordered]@{ kind = 'knob'; label = "Pan $_"; hue = 3; sends = @(@{ kind = 'controlChange'; channel = $_; number = 10 }) } })
$knobs[0]['newRow'] = $true
$layout = [ordered]@{
    name        = 'Eight channel mixer'
    description = 'Volume and pan for channels 1 to 8'
    request     = 'Faders and pan knobs for my eight synth channels'
    theme       = 'Studio Dark'
    devices     = @(@{ name = 'Synth'; endpoint = $bloop.name })
    pages       = @(@{ name = 'Mix'; controls = @($faders + $knobs + @([ordered]@{ kind = 'xyPad'; label = 'Filter'; newRow = $true; sends = @(@{ kind = 'controlChange'; number = 74 }, @{ kind = 'controlChange'; number = 71 }) })) })
}

$glassPreview = Invoke-Tool $server 'preview_layout' $layout
$image = $glassPreview.result.content | Where-Object type -eq 'image' | Select-Object -First 1
Check 'preview_layout is ready' ($glassPreview.result.isError -eq $false) (Text-Of $glassPreview)
Check 'preview_layout returns a PNG drawn by MIDI Glass' ($null -ne $image -and $image.mimeType -eq 'image/png' -and $image.data.StartsWith('iVBORw0KGgo')) (Text-Of $glassPreview)
if ($image) {
    [IO.File]::WriteAllBytes((Join-Path $shots 'glass-preview.png'), [Convert]::FromBase64String($image.data))
}
Check 'the preview says what each control sends' ((Text-Of $glassPreview) -match 'CC 7 on channel 8 to Synth' -and (Text-Of $glassPreview) -match 'CC 74 on channel 1 to Synth and CC 71')
Check 'preview writes nothing to the layouts folder' ((Get-ChildItem $layoutFolder).Count -eq 0)

$badLayout = Invoke-Tool $server 'preview_layout' ([ordered]@{ name = 'Bad'; devices = @(@{ endpoint = $bloop.name }); controls = @(@{ kind = 'slider' }) })
Check 'an unknown control kind lists the real ones' ((Text-Of $badLayout) -match 'fader, pad|knob, fader')

$tooBig = Invoke-Tool $server 'preview_layout' ([ordered]@{ name = 'Too big'; devices = @(@{ endpoint = $bloop.name }); controls = @(@{ kind = 'fader'; x = 1200; y = 700; width = 200; height = 300 }) })
Check 'a control off the page is caught' ((Text-Of $tooBig) -match 'outside the page')

$glassSaved = Invoke-Tool $server 'save_layout_draft' $layout
Check 'save_layout_draft succeeds' ($glassSaved.result.isError -eq $false) (Text-Of $glassSaved)
$layoutFile = Get-ChildItem $layoutFolder -Filter '*.midilayout' | Select-Object -First 1
Check 'the layout file exists' ($null -ne $layoutFile)

if ($layoutFile) {
    $doc = Get-Content $layoutFile.FullName -Raw | ConvertFrom-Json -Depth 60
    Check 'the layout keeps the draft marker' ($doc._draft.by -eq 'midi-mcp-harness')
    Check 'the layout has 17 controls' ($doc.pages[0].controls.Count -eq 17)
    Check 'the device table matches by endpoint id' ($doc.devices[0].match.endpointDeviceId -eq $bloop.id)

    $midiglass = Join-Path $PSScriptRoot '..\..\..\..\in-box\vsfiles-sdk\out\midiglass\x64\Release\midiglass.exe'
    if (Test-Path $midiglass) {
        $png = Join-Path $shots 'glass-saved-draft.png'
        $render = Start-Process -FilePath $midiglass -ArgumentList @('--thumbnail', "`"$($layoutFile.FullName)`"", "`"$png`"", '960') -PassThru -Wait -WindowStyle Hidden
        Check 'MIDI Glass itself reads the saved draft' ($render.ExitCode -eq 0 -and (Test-Path $png)) "exit $($render.ExitCode)"
    }
}

$layouts = Invoke-Tool $server 'list_glass_layouts' @{}
Check 'list_glass_layouts reads it back through the app reader' ((Text-Of $layouts) -match 'Eight channel mixer" \[draft')

$parameters = [ordered]@{
    name     = 'Synth parameters'
    devices  = @(@{ name = 'Synth'; endpoint = $bloop.name })
    controls = @(
        [ordered]@{ kind = 'knob'; label = 'Cutoff'; sends = @(@{ kind = 'nrpn'; msb = 3; lsb = 17 }) },
        [ordered]@{ kind = 'fader'; label = 'Bend range'; sends = @(@{ kind = 'rpn'; number = 0 }) }
    )
}
$parameterPreview = Invoke-Tool $server 'preview_layout' $parameters
Check 'preview_layout names an NRPN and an RPN the way a manual does' ((Text-Of $parameterPreview) -match 'NRPN 3:17 on channel 1 to Synth' -and (Text-Of $parameterPreview) -match 'RPN 0:0 on channel 1 to Synth') (Text-Of $parameterPreview)

$parameterSaved = Invoke-Tool $server 'save_layout_draft' $parameters
$parameterFile = Get-ChildItem $layoutFolder -Filter 'Synth parameters*.midilayout' | Select-Object -First 1
if ($parameterFile) {
    $sent = (Get-Content $parameterFile.FullName -Raw | ConvertFrom-Json -Depth 60).pages[0].controls | ForEach-Object { $_.messages[0] }
    Check 'the saved draft carries the app''s RPN and NRPN kinds and numbers' ($sent[0].kind -eq 'assignedController' -and $sent[0].number -eq 401 -and $sent[1].kind -eq 'registeredController' -and $sent[1].number -eq 0)
}
else {
    Check 'the saved draft carries the app''s RPN and NRPN kinds and numbers' $false (Text-Of $parameterSaved)
}

$badParameter = Invoke-Tool $server 'preview_layout' ([ordered]@{ name = 'Bad NRPN'; devices = @(@{ endpoint = $bloop.name }); controls = @(@{ kind = 'knob'; sends = @(@{ kind = 'nrpn'; number = 20000 }) }) })
Check 'an NRPN number past 16383 is refused' ((Text-Of $badParameter) -match '0 to 16383')

if ($layoutFile) {
    Copy-Item $layoutFile.FullName (Join-Path $layoutFolder 'Old build.midilayout.json')
    Check 'list_glass_layouts still reads the old .midilayout.json extension' ((Text-Of (Invoke-Tool $server 'list_glass_layouts' @{})) -match '^3 saved layouts')
}

# ------------------------------------------------------------------------------------------------
Write-Host "`n== Shutdown ==" -ForegroundColor Cyan
$server.StandardInput.Close()
$exited = $server.WaitForExit(10000)
Check 'the server exits when its input closes' ($exited -and $server.ExitCode -eq 0)
$stderr = $server.StandardError.ReadToEnd()
Check 'logging went to stderr' ($stderr -match 'serving all on stdio')

if (-not $KeepFiles) { Remove-Item $work -Recurse -Force }
else { Write-Host "Files kept in $work" }

Write-Host "`n$($script:passed) passed, $($script:failed) failed"
exit $script:failed
