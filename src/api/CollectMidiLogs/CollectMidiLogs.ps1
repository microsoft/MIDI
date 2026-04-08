# Copyright (c) Microsoft Corporation. All rights reserved.

param(
    [Parameter(Mandatory=$false)]
    [string] $specifiedOutputFile,

    [Parameter(Mandatory=$false)]
    [switch] $automatic
)

Import-Module "$PSScriptRoot\tttraceall.psm1";

# need this for [System.IO.Compression.ZipFile]::CreateFromDirectory to work on PowerShell 5
Add-Type -AssemblyName System.IO.Compression.FileSystem;



# Create a directory for all output files
$tempOutDirName = "$($env:COMPUTERNAME)_$(Get-Date -Format yyyyMMdd-HHmmss)";

Write-Host "Creating temporary directory $env:TEMP\$tempOutDirName.";
$outDir = New-Item -Path $env:TEMP -ItemType Directory -Name $tempOutDirName;

# see if the final results should be copied to a specific file name/path
if ($PSBoundParameters.ContainsKey('specifiedOutputFile'))
{
    $outputFileName = $specifiedOutputFile;
    Write-Host "Output will be here: $outputFileName";

    # todo: check if output file already exists
}
else
{
    $outputFileName = "$PSScriptRoot\${outDirName}.zip";    
}

$system32 = "${env:windir}\system32";
# check for WOW64
if ($env:PROCESSOR_ARCHITEW6432 -ne $null)
{
    Write-Host "WARNING: script is running WOW64";
    $system32 = "${env:windir}\sysnative";
}

# Ask the user if they'd like to take a repro trace
if (-NOT $automatic)
{
    do 
    {
        $reproResponse = Read-Host "Take a repro trace? (y/n)";
    }
    while (($reproResponse.Trim().ToLower())[0] -ne 'y' -and ($reproResponse.Trim().ToLower())[0] -ne 'n');
}

if ($automatic -OR ($reproResponse.Trim().ToLower())[0] -eq 'y')
{
    # Packaged wprp file
    $wprp = "$PSScriptRoot\providers.wprp";

    # Profile in wprp file to use for repro trace
    $profile = 'midi';

    $midisrvConfigChange = $false
    $tracingStarted = $false

    # prompt and optionally start the time travel traces
    # before starting etw tracing, so that the potential
    # service restarts are not included in the etw logs
    StartTTTracing -OutFolder $outDir -MidisrvConfigChange ([ref]$midisrvConfigChange) -TracingStarted ([ref]$tracingStarted)

    # Start repro
    $wprExe = "$system32\wpr.exe";
    $wprStartArgs = "-start `"$wprp!$profile`"";
    Start-Process $wprExe -ArgumentList $wprStartArgs -NoNewWindow -Wait;

    # Wait for user to press enter before continuing
    Write-Host "Tracing started, please reproduce your scenario.";
    $null = Read-Host "Press Enter to stop tracing";

    # Stop repro
    Write-Host 'Writing repro trace to disk.';
    $wprStopArgs = "-stop `"$outDir\Repro.etl`"";
    Start-Process $wprExe -ArgumentList $wprStopArgs -NoNewWindow -Wait;

    # stop time travel traces, if they were started
    # do this after stopping etw's so we don't have the potential
    # service restarts included in the logs
    StopTTTracing -OutFolder $outDir -MidisrvConfigChange ([ref]$midisrvConfigChange) -TracingStarted ([ref]$tracingStarted)
}


# Ask the user if they'd like to gather diagnostic information
if (-NOT $automatic)
{
    do 
    {
        $diagResponse = Read-Host "Gather system information (this can take a few minutes)? (y/n)";
    }
    while (($diagResponse.Trim().ToLower())[0] -ne 'y' -and ($diagResponse.Trim().ToLower())[0] -ne 'n');
}

if ($automatic -OR ($diagResponse.Trim().ToLower())[0] -eq 'y')
{
    # TODO: Get location of MIDI Diag and MIDI KSInfo from registry and also run that




    # Run command lines
    @(
        ("ddodiag", "$system32\ddodiag.exe", "-o `"$outDir\ddodiag.xml`""),
        ("dxdiag (text)", "$system32\dxdiag.exe", "/t `"$outDir\dxdiag.txt`""),
        ("pnputil", "$system32\pnputil.exe", "/export-pnpstate `"$outDir\pnpstate.pnp`" /force")
    ) | ForEach-Object {
        
        Write-Host "Running $($_[0]).";
        $proc = Start-Process $_[1] -ArgumentList $_[2] -NoNewWindow -PassThru;
        $timeout = 60; # seconds
        $proc | Wait-Process -TimeoutSec $timeout -ErrorAction Ignore;
        if (!$proc.HasExited)
        {
            Write-Host "$($_[0]) took longer than $timeout seconds, skipping.";
            taskkill /T /F /PID $proc.ID;
        }
    }
}

if ($automatic -OR ($reproResponse.Trim().ToLower())[0] -eq 'y' -or ($diagResponse.Trim().ToLower())[0] -eq 'y')
{
    # Logs should exist

    # Zip logs
    $logLoc = $outputFileName; 
    Write-Host "Zipping logs...";
    [System.IO.Compression.ZipFile]::CreateFromDirectory($outDir, $logLoc);

    # Delete temporary directory
    Write-Host "Deleting temporary directory $outDir.";
    Remove-Item -Path $outDir -Recurse;

    # Write log location to console
    Write-Host '';
    Write-Host "Logs are located at $logLoc";
    Write-Host 'You may now close this console window';
    Write-Host '';

    if ($automatic)
    {
        Read-Host "Press enter to close window."
    }

}
else
{
    # No logs were gathered
    Write-Host "No logs gathered."

    # Delete temporary directory
    Write-Host "Deleting temporary directory $outDir.";
    Remove-Item -Path $outDir;
}

