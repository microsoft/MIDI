#Requires -Version 7.6
import-module WindowsMidiServices

# Confirms Windows MIDI Services is available before anything else is attempted.
Start-Midi

# Reporting on Network MIDI 2.0. Everything in this sample is READ-ONLY on
# purpose. Connecting to or disconnecting from a host changes what other
# applications on this PC can see, and disconnecting a live remote client will
# interrupt whoever is using it, so those are shown at the bottom but not run.

# Hosts advertising themselves on the local network over DNS-SD. This is
# discovery, not connection: seeing a host here does not mean this PC is using it.
Write-Host "Advertised hosts on the network" -ForegroundColor Cyan

$advertised = Get-MidiNetworkAdvertisedHost

if ($null -ne $advertised)
{
    $advertised | Format-Table -AutoSize
}
else
{
    Write-Host "  None found. Discovery is passive, so give it a few seconds after the service starts." -ForegroundColor DarkGray
    Write-Host ""
}

# Hosts this PC is running. A host accepts incoming connections from other
# machines, which is the opposite direction from the clients below.
Write-Host "Hosts configured on this PC" -ForegroundColor Cyan

$hosts = Get-MidiNetworkConfiguredHost

if ($null -ne $hosts)
{
    $hosts | Format-Table -AutoSize
}
else
{
    Write-Host "  None configured." -ForegroundColor DarkGray
    Write-Host ""
}

# Clients are the outbound direction: connections this PC has made to a host
# somewhere else.
Write-Host "Clients configured on this PC" -ForegroundColor Cyan

$clients = Get-MidiNetworkConfiguredClient

if ($null -ne $clients)
{
    $clients | Format-Table -AutoSize
}
else
{
    Write-Host "  None configured." -ForegroundColor DarkGray
    Write-Host ""
}

Write-Host "Connecting and disconnecting" -ForegroundColor Cyan
Write-Host @"
  Not run by this sample, because both change what other applications can see.

  Connect to a host by address:
      Connect-MidiNetworkHost -HostNameOrAddress <host> -Port <port> ``
          -EndpointName "My Network Endpoint" -WhatIf

  Or pass an advertised host object straight through:
      Connect-MidiNetworkHost -AdvertisedHost `$someAdvertisedHost -WhatIf

  Disconnect a client this PC created:
      Disconnect-MidiNetworkHost -ClientId <guid> -WhatIf

  Two name parameters are easy to confuse:
    -LocalEndpointName is what THIS PC announces to the remote host.
    -EndpointName      is what Windows calls the endpoint created here.

  Both cmdlets support -WhatIf, which is worth using first. Disconnecting
  affects the remote machine as well as this one.
"@ -ForegroundColor DarkGray
