#Requires -Version 7.6
import-module WindowsMidiServices

# Confirms Windows MIDI Services is available before anything else is attempted.
Start-Midi

# Sending System Exclusive data. Send-MidiSystemExclusive takes either a path to
# a .syx file, or a byte array, and handles the conversion to SysEx 7 UMP
# messages and the pacing of the transfer for you.
#
# Under WinMM this was midiOutPrepareHeader / midiOutLongMsg /
# midiOutUnprepareHeader, plus responsibility for not freeing the buffer early.

# I'm using the default loopback that is created when you set up MIDI through the MIDI Settings app
$endpointDeviceId = "\\?\swd#midisrv#midiu_loop_a_default_loopback_a#{e7cce071-3c03-423f-88d3-f1045d02552b}"

# Set this to a .syx file to send a file instead of the bytes below.
$sysExFilePath = ""

$session = Start-MidiSession "PowerShell SysEx Send Sample"

if ($null -ne $session)
{
    $connection = Open-MidiEndpointConnection $session $endpointDeviceId

    if ($null -ne $connection)
    {
        if ($sysExFilePath -ne "")
        {
            Write-Host "Sending $sysExFilePath" -ForegroundColor Cyan

            # MessagesPerTransfer and DelayBetweenTransfersMilliseconds pace the
            # transfer. Older devices often need that time to keep up with a
            # large dump. Set either to zero to send with no pacing.
            Send-MidiSystemExclusive $connection -Path $sysExFilePath -GroupIndex 0 `
                -MessagesPerTransfer 10 -DelayBetweenTransfersMilliseconds 5
        }
        else
        {
            # A Universal Non-Real Time Identity Request. This asks a device to
            # say what it is, and is one of the very few System Exclusive
            # messages which is safe to send to hardware you know nothing about.
            #
            #   F0    start of System Exclusive
            #   7E    Universal Non-Real Time
            #   7F    device id, 7F meaning "all devices"
            #   06    General Information sub-id
            #   01    Identity Request
            #   F7    end of System Exclusive
            $identityRequest = [byte[]]@(0xF0, 0x7E, 0x7F, 0x06, 0x01, 0xF7)

            Write-Host "Sending a Universal Identity Request" -ForegroundColor Cyan

            Send-MidiSystemExclusive $connection -Bytes $identityRequest -GroupIndex 0
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
