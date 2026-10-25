# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

<#
.SYNOPSIS
    Checks build-all prerequisite selection and MSBuild arguments without building or staging files.
.EXAMPLE
    pwsh -NoProfile -File build\tests\build-prerequisites.tests.ps1
#>
[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$buildScript = Join-Path $PSScriptRoot '..\build-all.ps1'
$parseErrors = $null
$ast = [System.Management.Automation.Language.Parser]::ParseFile($buildScript, [ref]$null, [ref]$parseErrors)
if ($parseErrors.Count) { throw ($parseErrors -join "`n") }

# Import only declarations, never the build script's file-writing or cleanup operations.
$functionNames = @('Expand-Argument', 'Invoke-MSBuild', 'Invoke-PrerequisiteProjects', 'Invoke-ServiceTarget', 'Invoke-SdkTarget')
$assignmentNames = @('AllTargets', 'ServicePrerequisiteProjects', 'SdkPrerequisiteProjects', 'Arm64ToolPrerequisiteProjects', 'Arm64EcProjects')
foreach ($statement in $ast.EndBlock.Statements) {
    if (($statement -is [System.Management.Automation.Language.FunctionDefinitionAst] -and $statement.Name -in $functionNames) -or
        ($statement -is [System.Management.Automation.Language.AssignmentStatementAst] -and
            $statement.Left.Extent.Text.TrimStart('$') -in $assignmentNames)) {
        . ([scriptblock]::Create($statement.Extent.Text))
    }
}
$invokeMSBuild = ${function:Invoke-MSBuild}

function Assert-Equal($Actual, $Expected, [string] $Message) {
    if (($Actual -join "`n") -cne ($Expected -join "`n")) {
        throw "$Message`nExpected: $($Expected -join ', ')`nActual: $($Actual -join ', ')"
    }
    $script:Assertions++
}

function Write-Step {}
function Write-Detail {}
function Write-Note {}
function Update-ApiReferenceHeader {}
function Remove-SdkLinkOutput {}
function Test-SdkBuildOutput {}
function Get-VersionProperties { @{ Version = 'test-version' } }

$ApiRoot = Join-Path (Split-Path (Split-Path $PSScriptRoot)) 'src\in-box'
$ServiceSolution = Join-Path $ApiRoot 'Midi2.sln'
$SdkSolution = Join-Path $ApiRoot 'Midi2-AppSDK.sln'
$Transports = @()
$script:Versions = @{ Sdk = @{} }
$script:Assertions = 0
$script:Calls = [Collections.Generic.List[object]]::new()
$script:FailProject = ''

function Invoke-MSBuild {
    param($ProjectOrSolution, $BuildPlatform, $Targets = @(), $Properties = @{}, $SolutionDir, [switch] $Serial)
    if ($ProjectOrSolution -eq $script:FailProject) { throw 'Expected prerequisite failure' }
    $script:Calls.Add([pscustomobject]@{
        Project = $ProjectOrSolution.Substring($ApiRoot.Length + 1)
        Platform = $BuildPlatform
        Targets = $Targets
        Properties = $Properties
        SolutionDir = $SolutionDir
        Serial = $Serial.IsPresent
    })
}

# Expected paths are independent of the production lists, so omitting a dependency fails the test.
$servicePaths = @(
    'idl\IDL.vcxproj'
    'Libs\MidiPluginConfigurationLib\MidiPluginConfigurationLib.vcxproj'
    'Libs\MidiEndpointNamingLib\MidiEndpointNamingLib.vcxproj'
    'Libs\MidiPnpLib\MidiPnpLib.vcxproj'
    'Midi2.sln'
)
$sdkPaths = @(
    'idl\IDL.vcxproj'
    'Transport\MidiSrvTransport\Midi2.MidiSrvTransport.vcxproj'
    'Libs\SDK-MidiPluginConfigurationLib\MidiPluginConfigurationLib.vcxproj'
    'Libs\SDK-MidiEndpointNamingLib\MidiEndpointNamingLib.vcxproj'
    'Libs\SDK-MidiPnpLib\MidiPnpLib.vcxproj'
)
$toolPaths = @('Libs\MidiKsCommon\MidiKSCommon.vcxproj', 'Libs\MidiKs\MidiKS.vcxproj', 'Test\Libs\MidiKSEnum\MidiKSEnum.vcxproj')
$ecPaths = $sdkPaths + @('Client\WinRT\com-extensions-idl\com-extensions-idl.vcxproj', 'Client\WinRT\core\Windows.Devices.Midi2.vcxproj')

foreach ($Configuration in @('Debug', 'Release')) {
    foreach ($platforms in @(@('x64'), @('Arm64'), @('x64', 'Arm64'))) {
        $Platform = $platforms
        $script:Calls.Clear()
        Invoke-ServiceTarget
        $expected = @(foreach ($plat in $Platform) { foreach ($path in $servicePaths) { "$plat|$path" } })
        Assert-Equal @($script:Calls | ForEach-Object { "$($_.Platform)|$($_.Project)" }) $expected "Service order: $Configuration/$Platform"
        foreach ($call in $script:Calls) {
            Assert-Equal $call.Properties.NoWarn 'MIDL2111' 'Preserve service build properties'
        }

        $script:Calls.Clear()
        Invoke-SdkTarget
        $expected = @(
            if ($Platform -contains 'Arm64') {
                foreach ($path in ($sdkPaths + $toolPaths + @('Midi2-AppSDK.sln'))) { "Arm64|$path" }
                foreach ($path in $ecPaths) { "Arm64EC|$path" }
            }
            if ($Platform -contains 'x64') {
                foreach ($path in ($sdkPaths + @('Midi2-AppSDK.sln'))) { "x64|$path" }
            }
        )
        Assert-Equal @($script:Calls | ForEach-Object { "$($_.Platform)|$($_.Project)" }) $expected "SDK order: $Configuration/$Platform"
        Assert-Equal $script:SdkBuiltThisPass $Platform 'SDK platform tracking'
        foreach ($call in $script:Calls) {
            Assert-Equal $call.Properties.Version 'test-version' 'Forward SDK version properties'
            Assert-Equal $call.Properties.GeneratePackageOnBuild 'false' 'Do not pack prerequisites'
            if ($call.Project -notlike '*.sln') {
                Assert-Equal $call.SolutionDir $ApiRoot 'Use the solution output folders'
                Assert-Equal $call.Serial $true 'Serialize prerequisite projects'
            }
            $expectedTargets = if ($call.Project -eq $sdkPaths[1]) { @('Midl') } else { @() }
            Assert-Equal $call.Targets $expectedTargets 'Generate service transport headers without compiling the transport'
        }
    }
}

# Run the real target-selection expressions and final dispatch with unrelated targets stubbed.
$source = $ast.Extent.Text
$selectionStart = $source.IndexOf('$targets = @(if')
$selectionEnd = $source.IndexOf('$script:CompilersPerProject =', $selectionStart)
$selectTargets = [scriptblock]::Create($source.Substring($selectionStart, $selectionEnd - $selectionStart))
$mainTry = $ast.EndBlock.Statements | Where-Object { $_ -is [System.Management.Automation.Language.TryStatementAst] } | Select-Object -Last 1
$dispatch = @($mainTry.Body.Statements | Where-Object { $_.Extent.Text -match '^if \(\$targets -contains ' })
foreach ($name in @('Version', 'Pack', 'Samples', 'Stage', 'Setup', 'Release')) {
    Set-Item "function:Invoke-${name}Target" -Value {}
}
foreach ($requested in @(@('Service'), @('Sdk'), @('Service,Sdk'), @('All'), @('Stage,Setup'), @('Clean'), @('Version'), @('Release'))) {
    $Platform = @('x64', 'Arm64')
    $Target = Expand-Argument -Value $requested -Allowed (@('All', 'Clean') + $AllTargets) -Name 'Target'
    . $selectTargets
    $script:Calls.Clear()
    foreach ($statement in $dispatch) { . ([scriptblock]::Create($statement.Extent.Text)) }
    Assert-Equal @($script:Calls | Where-Object Project -eq 'Midi2.sln').Count ([int]($targets -contains 'Service') * 2) "Service selection: $requested"
    Assert-Equal @($script:Calls | Where-Object Project -eq 'Midi2-AppSDK.sln').Count ([int]($targets -contains 'Sdk') * 2) "SDK selection: $requested"
    if ($targets -notcontains 'Service' -and $targets -notcontains 'Sdk') {
        Assert-Equal $script:Calls.Count 0 "No prerequisite builds for $requested"
    }
}

foreach ($plat in @('x64', 'Arm64')) {
    $Platform = @($plat)
    $Target = @('All')
    . $selectTargets
    Assert-Equal $targets @('Version', 'Service', 'Sdk', 'Stage', 'Setup', 'Release') 'Single-platform All skips packing and samples'
    $Target = @('Sdk', 'Pack')
    $failed = $false
    try { . $selectTargets } catch {
        if ($_.Exception.Message -notlike 'Pack needs -Platform x64,Arm64:*') { throw }
        $failed = $true
    }
    Assert-Equal $failed $true 'Explicit Pack still requires both platforms'
}

$Platform = @('Arm64')
$script:Calls.Clear()
$script:FailProject = Join-Path $ApiRoot $servicePaths[2]
$failed = $false
try { Invoke-ServiceTarget } catch {
    if ($_.Exception.Message -ne 'Expected prerequisite failure') { throw }
    $failed = $true
}
Assert-Equal $failed $true 'Propagate prerequisite failure'
Assert-Equal @($script:Calls.Project) $servicePaths[0..1] 'Do not start consumers after prerequisite failure'

# Exercise the actual native-command argument builder against a stub, not MSBuild.
$script:MSBuild = {
    $script:NativeArgs = @($args)
    $global:LASTEXITCODE = $script:NativeExitCode
}
$MaxCpuCount = 8
$script:CompilersPerProject = 4
$Priority = 'BelowNormal'
$Verbosity = 'minimal'
$script:NativeExitCode = 0
foreach ($Configuration in @('Debug', 'Release')) {
    foreach ($plat in @('x64', 'Arm64', 'Arm64EC')) {
        foreach ($project in @('library.vcxproj', 'projects.sln')) {
            & $invokeMSBuild -ProjectOrSolution $project -BuildPlatform $plat -SolutionDir $ApiRoot
            Assert-Equal ($script:NativeArgs -contains "/p:Configuration=$Configuration") $true 'Forward configuration'
            Assert-Equal ($script:NativeArgs -contains "/p:Platform=$plat") $true 'Forward architecture'
            Assert-Equal ($script:NativeArgs -contains '/p:CL_MPCount=4') $true 'Limit compiler processes'
            Assert-Equal ($script:NativeArgs -contains '-low') $true 'Lower build priority'
            Assert-Equal @($script:NativeArgs | Where-Object { $_ -like '/m*' }) $(if ($project -like '*.sln') { '/m:8' }) 'No multiprocess switch for one project'
            Assert-Equal ($script:NativeArgs -contains "/p:SolutionDir=$ApiRoot\") $true 'Forward solution directory'
            Assert-Equal @($script:NativeArgs | Where-Object { $_ -like '/t:*' }).Count 0 'Use incremental default Build target'
        }
    }
}
& $invokeMSBuild -ProjectOrSolution 'projects.sln' -BuildPlatform 'Arm64' -Serial -Targets @('Midl') -Properties @{ NoWarn = 'test-warning' }
Assert-Equal @($script:NativeArgs | Where-Object { $_ -like '/m*' }).Count 0 'Serial invocation stays single-node'
Assert-Equal ($script:NativeArgs -contains '/t:Midl') $true 'Forward explicit header-generation target'
Assert-Equal ($script:NativeArgs -contains '/p:NoWarn=test-warning') $true 'Forward additional build properties'
$script:NativeExitCode = 7
$failed = $false
try { & $invokeMSBuild -ProjectOrSolution 'library.vcxproj' -BuildPlatform 'Arm64' } catch {
    if ($_.Exception.Message -notlike 'MSBuild failed (7):*') { throw }
    $failed = $true
}
Assert-Equal $failed $true 'Propagate native build exit code'
Write-Host "Passed $script:Assertions build prerequisite assertions."
