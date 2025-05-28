#Requires -Version 7.4
import-module WindowsMidiServices

$sdkinfo = Start-Midi | Write-Output

# List all available endpoints. Enumeration functions do not require an active session.
Write-Host
Write-Host "Available MIDI Endpoints" -ForegroundColor White
$endpoints = Get-MidiEndpointDeviceInfoList | Sort-Object -Property Name

$index = 0
foreach ($endpoint in $endpoints)
{
    Write-Host ($index + 1)  -NoNewline -ForegroundColor Cyan
    Write-Host ": " -NoNewline -ForegroundColor Gray
    Write-Host $endpoint.Name  -ForegroundColor DarkCyan
    $index++
}

Write-Host "Please enter the number of an endpoint to monitor, or Q to quit" -ForegroundColor Yellow
$selection = Read-Host 

if ($selection -match 'q')
{
    # quit
    Write-Host "Quitting..."
}
else
{
    # validate that the user entered a number
    if ($selection -match "^[\d]")
    {
        # selectedNumber is the index + 1
        $selectedNumber = [int]$selection

        if (($selectedNumber -gt 0) -and ($selectedNumber -le $endpoints.Count))
        {
            # create session
            $session = Start-MidiSession "Powershell Monitor Session"

            if ($session -ne $null)
            {
                Write-Host
                Write-Host "Opening connection to: " 
                Write-Host $endpoints[$selectedNumber-1].Name  -ForegroundColor DarkCyan -NoNewLine
                Write-Host " - " $endpoints[$selectedNumber-1].EndpointDeviceId  -ForegroundColor DarkCyan
                Write-Host
                Write-Host "Press any key to quit monitoring" -ForegroundColor White

                # open a connection to the endpoint
                $connection = Open-MidiEndpointConnection $session $endpoints[$selectedNumber-1].EndpointDeviceId
        
                if ($connection -ne $null)
                {
                    $eventHandlerAction = {
                        #Write-Host "Message Received"
                        #Write-Host $EventArgs.Timestamp
                        Get-MidiMessageInfo $EventArgs.Words
                    }

                    $job = Register-ObjectEvent -SourceIdentifier "OnMessageReceivedHandler" -InputObject $connection -EventName "MessageReceived" -Action $eventHandlerAction

                    # just spin until a key is pressed
                    do
                    {
                        Receive-Job -Job $job
                        #Start-Sleep -Milliseconds 250
                    } until ([System.Console]::KeyAvailable)

                    # we don't do anything with the key here, but you could
                    $keyPressed = [System.Console]::ReadKey($true)

                    Write-Host
                    Write-Host "Key pressed. Shutting down ... "

                    Unregister-Event -SourceIdentifier "OnMessageReceivedHandler"
                    Stop-Job $job
                    Remove-Job $job

                    Close-MidiEndpointConnection $session $connection
                }
                else
                {
                    Write-Host "Unable to connect to endpoint." -ForegroundColor Red
                }
            }
            else
            {
                Write-Host "Unable to create new session." -ForegroundColor Red
            }
        }
        else
        {
            Write-Host "Selection not in range." -ForegroundColor Red
        }
    }
    else
    {
        Write-Host "Entry not on the menu." -ForegroundColor Red
    }
}



# Cleanly shut down the SDK, stop WinRT activation redirection, etc.
#Write-Host "Shutting down the SDK"
Stop-Midi


