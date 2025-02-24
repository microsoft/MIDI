#Requires -Version 7.4
import-module WindowsMidiServices

$sdkinfo = Start-Midi | Write-Output

# List all available endpoints. Enumeration functions do not require an active session.
Write-Host "Available MIDI Endpoints" -ForegroundColor Cyan
(Get-MidiEndpointDeviceInfoList) | Sort-Object -Property Name | Format-Table -AutoSize

# I'm using the default loopback that is created when you set up MIDI through the MIDI Settings app
$endpointDeviceId = "\\?\swd#midisrv#midiu_loop_a_default_loopback_a#{e7cce071-3c03-423f-88d3-f1045d02552b}"

# show some info about the device we're interested in
Write-Host "Endpoint we intend to connect to" -ForegroundColor Cyan
(Get-MidiEndpointDeviceInfo $endpointDeviceId)  | Format-List

# create a new session so we can send and receive messages
$session = Start-MidiSession "Powershell Demo Session"

if ($session -ne $null)
{
    Write-Host "This session" -ForegroundColor Cyan
    Write-Output $session | Format-List

    # list all the active sessions
    Write-Host "All active MIDI sessions" -ForegroundColor Cyan
    (Get-MidiSessionList) | Sort-Object -Property Name | Format-Table -AutoSize

    # open a connection to the endpoint
    $connection = Open-MidiEndpointConnection $session $endpointDeviceId
   
    if ($connection -ne $null)
    {
        # each sub-array is a complete MIDI UMP
        $messages = (0x40905252, 0x02001111), (0x40805252, 0x02000000), 0x25971234

        foreach ($message in $messages)
        {
            Write-Host "Sending MIDI message" -ForegroundColor Cyan

            # this gets / displays information about the MIDI message we're sending
            Get-MidiMessageInfo $message | Format-List 
            Send-MidiMessage $connection $message -Timestamp 0
        }

        # You don't need to close the connection if you plan to just destroy the session
        # but it's a good practice to clean up what you create
        Write-Host "Closing Endpoint connection" -ForegroundColor Cyan
        Close-MidiEndpointConnection $session $connection
    }
    else
    {
        Write-Host "Unable to open MIDI connection. Is the Endpoint Device Id correct?" -ForegroundColor Red
    }

    # Terminate the session, and any connections it has open
    Write-Host "Stopping the MIDI session" -ForegroundColor Cyan
    Stop-MidiSession $session
}
else
{
    Write-Host "Unable to create new MIDI Session" -ForegroundColor Red
}

# Cleanly shut down the SDK, stop WinRT activation redirection, etc.
Write-Host "Shutting down the SDK" -ForegroundColor Cyan
Stop-Midi