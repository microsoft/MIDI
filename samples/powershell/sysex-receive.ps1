#Requires -Version 7.6
import-module WindowsMidiServices

# Confirms Windows MIDI Services is available before anything else is attempted.
Start-Midi

# Receiving System Exclusive data. Receive-MidiSystemExclusive reassembles a
# message which arrives across many UMP messages, and either returns it as
# objects or writes it straight to a .syx file.
#
# Under WinMM this meant preparing MIDIHDR buffers, handling MIM_LONGDATA,
# requeueing each buffer, and stitching a message back together yourself.
#
# Pair this with sysex-send.ps1 pointed at the same loopback to see it work
# without any hardware attached. Run this one first.

# I'm using the default loopback that is created when you set up MIDI through the MIDI Settings app
$endpointDeviceId = "\\?\swd#midisrv#midiu_loop_a_default_loopback_a#{e7cce071-3c03-423f-88d3-f1045d02552b}"

# Set this to write what arrives to a file instead of returning it as objects.
# The file is written incrementally, so stopping with Ctrl+C still leaves a
# complete .syx file behind.
$sysExFilePath = ""

$session = Start-MidiSession "PowerShell SysEx Receive Sample"

if ($null -ne $session)
{
    $connection = Open-MidiEndpointConnection $session $endpointDeviceId

    if ($null -ne $connection)
    {
        Write-Host "Waiting for System Exclusive messages on group 1" -ForegroundColor Cyan
        Write-Host "Run sysex-send.ps1 in another window, or send from a device." -ForegroundColor DarkGray
        Write-Host ""

        if ($sysExFilePath -ne "")
        {
            # -Force overwrites an existing file. Without it, an existing file
            # is an error rather than something quietly destroyed.
            Receive-MidiSystemExclusive $connection -Path $sysExFilePath -GroupIndex 0 `
                -MessageCount 1 -TimeoutSeconds 30 -Force

            Write-Host "Wrote $sysExFilePath" -ForegroundColor Cyan
        }
        else
        {
            # Stop after the first complete message, or after 30 seconds if
            # nothing arrives. Omit both to keep listening until Ctrl+C.
            $messages = Receive-MidiSystemExclusive $connection -GroupIndex 0 `
                -MessageCount 1 -TimeoutSeconds 30

            if ($null -ne $messages)
            {
                foreach ($message in $messages)
                {
                    Write-Host "Received a System Exclusive message" -ForegroundColor Cyan
                    $message | Format-List
                }
            }
            else
            {
                Write-Host "No System Exclusive messages arrived before the timeout." -ForegroundColor Yellow
            }
        }

        Write-Host "Closing Endpoint connection" -ForegroundColor Cyan
        Close-MidiEndpointConnection $session $connection
    }
    else
    {
        Write-Host "Unable to open MIDI connection. Is the Endpoint Device Id correct?" -ForegroundColor Red
    }

    Write-Host "Stopping the MIDI session" -ForegroundColor Cyan
    Stop-MidiSession $session
}
else
{
    Write-Host "Unable to create new MIDI Session" -ForegroundColor Red
}
