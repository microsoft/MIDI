# Copyright (c) Microsoft Corporation. All rights reserved.

# Collects a Time Travel Trace for the Windows MIDI Service (midisrv) and for any of the
# in-box Windows MIDI Services command-line and GUI apps that are running when it starts.

# The command-line and GUI apps that the Windows MIDI Services installers ship (see the file
# lists under src\installers). midisoundmodule and midi2-console are built but not shipped.
# Keep in step with InBoxClientProcessNames() in the MIDI Troubleshooting app (ClientProcesses.cpp).
$InBoxClientProcessNames = @(
    "midi",
    "midi1enum",
    "midi1monitor",
    "midi1rtmonitor",
    "midi2monitor",
    "midibluetoothsetup",
    "midiclock",
    "mididiag",
    "midiglass",
    "midikeyboard",
    "midiksinfo",
    "midiloopbacksetup",
    "midinetworksetup",
    "midinotifications",
    "midipatchbay",
    "midiplayer",
    "midiscratchpad",
    "midisequencer",
    "midisettings",
    "midisysextool",
    "miditroubleshooter"
)

Function FindInBoxClientProcesses()
{
    # -ErrorAction: a name with no running process is the normal case, not an error
    return @(Get-Process -Name $InBoxClientProcessNames -ErrorAction SilentlyContinue)
}

Function WriteClientInventory(
    [string]$OutFolder
)
{
    # Which MIDI libraries each running in-box app has loaded: Windows.Devices.Midi,
    # Windows.Devices.Midi2, the Midi2.* client transport, midimap, winmm and wdmaud2.
    # The ETW trace records later loads (Loader keyword); this is the picture at the start.
    $lines = @("Windows MIDI Services apps running when the capture started", "")

    $clients = FindInBoxClientProcesses

    if ($clients.Count -eq 0)
    {
        $lines += "none"
    }

    foreach ($client in $clients)
    {
        $lines += "$($client.ProcessName).exe (process $($client.Id))"

        try
        {
            $modules = @($client.Modules | Where-Object {
                $_.ModuleName -match 'midi' -or $_.ModuleName -match '^winmm' -or $_.ModuleName -match '^wdmaud' })

            if ($modules.Count -eq 0)
            {
                $lines += "  no MIDI modules loaded"
            }

            foreach ($module in $modules)
            {
                $info = $module.FileVersionInfo
                $lines += "  $($module.FileName)  $($info.FileMajorPart).$($info.FileMinorPart).$($info.FileBuildPart).$($info.FilePrivatePart)"
            }
        }
        catch
        {
            $lines += "  modules could not be read"
        }

        $lines += ""
    }

    $lines | Out-File -FilePath (Join-Path $OutFolder "client-processes.txt") -Encoding utf8

    Write-Host "Windows MIDI Services apps running: $($clients.Count). Their MIDI libraries are listed in client-processes.txt."
}

Function FindProcessWithModule(
    [string]$ProcessName,
    [string]$Module
)
{
    # uses the process name to find the win32 process, if it's not a process then it checks to see if it's a service.
    # it then convers the WMI object to a process object, for later use.
    $processFound = get-wmiobject -class win32_process -Filter "name='$ProcessName'"
    if ($processFound -eq $null)
    {
        $processFound = get-wmiobject win32_service -Filter "name='$ProcessName'"
    }

    if ($processFound -ne $null)
    {
        # find only the rundll32 that contains mmsys.cpl, we don't need a trace from others
        foreach ($item in $processFound)
        {
            $thisProcess = get-process -Id $item.ProcessId
            
            if ($Modules -eq $null -or
                $thisProcess.Modules.ModuleName.Contains($Module))
            {
                $process += $thisProcess
            }
        }
    }

    return $process
}

Function GetMitigations(
    [String]$ProcessName,
    $Process,
    [ref]$ConfigChange
)
{
    $ConfigChange.Value=$false
    # retrieve the current process mitigations, and disable shadowstack if necessary
    # shadowstacks must be disabled for time travel traces to work. Shadowstacks is a security
    # mechanism to protect the stored return address so it cannot be overwritten with a buffer
    # overflow.
    if ($Process -ne $null)
    {
        $mitigation = get-processmitigation -Id $Process.Id
        if ($mitigation.UserShadowStack.UserShadowStack -eq "ON" -or
            $mitigation.UserShadowStack.UserShadowStackStrictMode -eq "ON")
        {
            set-processmitigation -Name $ProcessName -Disable UserShadowStack,UserShadowStackStrictMode -Force on
            $ConfigChange.Value=$true
        }
    }
}

Function RestoreMitigations(
    [string]$ProcessName,
    [bool]$ConfigChanged
)
{
    # restore shadowstack settings
    if ($ConfigChanged -eq $true)
    {
        # "notset" is not on or off, but rather a neutral value indicating that the setting is inherited
        # This is the default value.
        set-processmitigation -Name $ProcessName -Enable UserShadowStack,UserShadowStackStrictMode -Force notset
    }
}

Function ResetProcesses()
{
    # if something with shadowstacks changed, we need to shut down and restart the processes for it to take
    # effect.
    stop-service -Name "midisrv" -Force
    start-service -Name "midisrv"
}

Function StartTracer(
    $ProcessName,
    $Process,
    $OutFolder
)
{
    if ($Process -ne $null)
    {
        $processId =  $Process.Id.ToUInt32($null);

        # capture the STDOUT and STDERR of each tttracer start process
        $stdoutlog = $OutFolder + "\tttracer-" + $ProcessId + ".log";
        $stderrlog = $OutFolder + "\tttracer-" + $ProcessId + ".err";

        # the default size for ring tracing is a little over 2 GB
        # we can increase this by adding a -maxFile <file size in megabytes> switch
        start-process "tttracer.exe" `
            -argumentlist "-ring -out $OutFolder -attach $ProcessId" `
            -nonewwindow `
            -redirectstandardoutput $stdoutlog `
            -redirectstandarderror $stderrlog;
    }
    else
    {
        
        Write-Host "$ProcessName is not running"
    }
}

Function StartTTTracing(
    [String]$OutFolder,
    [ref]$MidisrvConfigChange,
    [ref]$TracingStarted
)
{
    # check to see if this is a machine with tttracer inbox, if not,
    # can't get a ttt
    if ( !(test-path "${env:windir}\system32\tttracer.exe"))
    {
        Write-Host "System does not support TimeTravelTrace, continuing"
        return;
    }

    Write-Host "A time travel trace records the MIDI service and any Windows MIDI Services apps that are open now,"
    Write-Host "such as midi1rtmonitor or Windows MIDI Monitor. Open the app you're having trouble with before you answer."

    do 
    {
        $reproResponse = Read-Host "Record time travel traces? (y/n)";
    }
    while (($reproResponse.Trim().ToLower())[0] -ne 'y' -and ($reproResponse.Trim().ToLower())[0] -ne 'n');

    if (($reproResponse.Trim().ToLower())[0] -eq 'n')
    {
        return
    }

    $TracingStarted.Value = $true

    $midisrv = FindProcessWithModule -ProcessName "midisrv.exe"

    GetMitigations -Process $midisrv -ProcessName "midisrv.exe" -ConfigChange $MidisrvConfigChange

    # if we needed a config change, we have to restart everything.
    if ($MidisrvConfigChange.Value -eq $true)
    {
        Write-Host "Mitigations change required, resetting processes."
        "Processes were reset" | out-file (join-path $OutFolder "TTTProcessReset.txt")
        ResetProcesses

        $midisrv = FindProcessWithModule -ProcessName "midisrv.exe"
    }

    # shadowstack settings are good, start the trace
    StartTracer -ProcessName "midisrv" -process $midisrv -OutFolder $OutFolder

    # The client side: every in-box app that is already running. Their shadow stack settings are
    # left alone, because changing them would mean restarting the customer's app; if an attach
    # is refused, the tttracer-<pid>.err file in the package says so. Apps started after this
    # point are in the ETW trace but not time travel traced.
    foreach ($client in (FindInBoxClientProcesses))
    {
        Write-Host "Time travel tracing $($client.ProcessName).exe (process $($client.Id))."
        StartTracer -ProcessName $client.ProcessName -Process $client -OutFolder $OutFolder
    }
}

Function StopTTTracing(
    [string]$OutFolder,
    [ref]$MidisrvConfigChange,
    [ref]$TracingStarted
)
{
    if ($TracingStarted.Value -eq $true)
    {
        # capture the STDOUT and STDERR of the tttracer stop process
        $stdoutlog = $OutFolder + "\tttracer-stop.log";
        $stderrlog = $OutFolder + "\tttracer-stop.err";
        start-process "tttracer.exe" `
            -argumentlist "-stop all" `
            -nonewwindow `
            -redirectstandardoutput $stdoutlog `
            -redirectstandarderror $stderrlog `
            -wait;

        # restore from any mitigation changes
        RestoreMitigations -ProcessName "midisrv.exe" -ConfigChange $MidisrvConfigChange.Value

        Write-Host "Resetting processes to close out traces."
        ResetProcesses

        # The service restart above finishes its trace. The apps are left running, because they
        # are the customer's; they keep the recording overhead until they are closed.
        Write-Host "Any Windows MIDI Services app that was traced stays slow until you close it and open it again."
    }
}


