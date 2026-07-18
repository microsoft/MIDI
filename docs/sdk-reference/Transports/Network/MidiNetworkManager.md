---
layout: sdk_reference_page
title: MidiNetworkEndpointManager
namespace: Microsoft.Windows.Devices.Midi2.Endpoints.Network
library: Microsoft.Windows.Devices.Midi2
type: runtimeclass
status: preview
description: Class for managing the creation of Network MIDI 2.0 hosts and clients from the settings app or from applications
---

Network MIDI 2 is currently in development, and so works only from configuration file entries. Further documentation will be supplied after the SDK types are implemented, allowing for runtime creation/removal.

## Prerequisites

- Network MIDI Transport preview service plugin installed
- `midisrv.exe` (the Windows MIDI Service) allowed through any firewalls, including Windows Firewall
- You have Network MIDI 2.0 device(s) on your subnet, or another Windows PC with Network MIDI 2.0 installed and configured. Crossing subnets is not currently supported because we do not enable direct connections.
- mDNS is running on the PC (we use built-in mDNS services, not Bonjour)
- mDNS is allowed on your network / subnet

## Definitions

- **Host:** Windows MIDI Services is the host that other clients connect to. Typically, you will start with a single host for your PC.
- **Client:** An entry for Windows MIDI Services to automatically connect to when found via mDNS. Only needed if you have external host devices or PCs to connect to.

## Configuration File 

The current configuration format looks like this (subject to change):

```json
"{C95DCD1F-CDE3-4C2D-913C-528CB8A4CBE6}":                           # network MIDI 2.0 transport id. Case sensitive
{
    "_comment": "Network MIDI 2.0",                                 # just a comment
    "transportSettings":                                            # transport-wide settings
    {
        "maxForwardErrorCorrectionCommandPackets": 2,               # FEC in the specification. Applies to all connections
        "maxRetransmitBufferCommandPackets" : 50,                   # applies to all connections
        "outboundPingInterval" : 2000                               # applies to all connections. Value is in milliseconds.
    },
    "create":
    {
        "hosts":                                                    # contains a comma-separated list of host objects as properties. Not an array.
        {
            "{090ad480-3cf8-4228-b58f-469f773e4b61}":               # arbitrary unique identifier for this host entry
            {
                "name": "Windows MIDI Services Host",               # name for this host. This is what other devices will see
                "serviceInstanceName" : "windows",                  # used in the mDNS entry, per spec. Change this to a value you want, that is unique on your network.
                "productInstanceId" : "3263827-5150Net2Preview",    # used in the mDNS entry, per spec. Change this to a value you want. It should match the discovered id per spec.
                "networkProtocol" : "udp",                          # only supported value right now is "udp"
                "port" : "auto",                                    # a specific port number or "auto"
                "enabled" : true,                                   # true if the host should be enabled
                "advertise" : true,                                 # true if the host should advertise via mDNS
                "authentication" : "none",                          # current supported value is "none"
                "connectionPolicyIpv4" : "allowAny"                 # current supported value is "allowAny"
            }
        },
        "clients":                                                  # contains a comma-separated list of client objects as properties. Not an array.
        {
            "{25d5789f-c84d-4310-91ea-bdc1680f35d5}":               # arbitrary unique identifier for this client entry
            {
                "_comment": "kissbox",                              # just a comment
                "networkProtocol" : "udp",                          # protocol for the connection. Only "udp" is supported
                "match" :                                           # criteria used when discovering the device to connect to
                {
                    "id": "DnsSd#kb7C5D0A_1._midi2._udp.local#0"    # SWD is, for the moment, the only match value here. Use midimdnsinfo.exe to find values
                }
            },
            "{ba0f1174-b343-4b32-84e4-01e368d08545}":
            {
                "_comment": "direct ip example",
                "name": "BomeBox via IP",
                "networkProtocol" : "udp",
                "match" :
                {
                    "directIPAddress": "192.168.1.243",             # IP address of the remote host
                    "directPort": "39820"                           # port number of the remote host
                }
            },
            "{fd0bf1d0-4ac6-4d57-b0e8-7bb29b029f4f}":
            {
                "_comment": "direct host name example",
                "name": "BomeBox DIN via HostName",
                "networkProtocol" : "udp",
                "match" :
                {
                    "directHostName": "BomeBox.local",              # Host Name of the remote host (must be something that this PC can resolve)
                    "directPort": "51492"                           # port number of the remote host
                }
            }            
        }        
    }
},
```
The above json (minus the # annotations) goes into the transport settings section in the json configuration.

Once the SDK types are complete, the settings app will be used to create this configuration file entry.

## Current limitations and assumptions

This code is still preview-quality and will have bugs. It has been tested with BomeBox and KissBox as well as with AmeNote devices. Here are some known limitations beyond those mentioned in the json above:

- mDNS is required for all connections. Direct connection by ip/port is not yet supported
- mDNS has limitations on subnet. Devices on other subnets will not be found.
- There is no authentication in place
- mDNS discovery/enumeration is somewhat slow. It can take the better part of a minute to discover a device. This is being investigated. You can see this in the `midimdnsinfo` client app in the SDK.
- You must restart the MIDI Service after making any changes to this configuration file
- From experience at the NAMM show, this may not work in situations where the PC and device (or other PC) are connected with a network cable with no ethernet hub in between.
- Probably lots of other limitations. Again, this is a preview and the majority of the work has been focused on the protocol itself.

## NAMM Show demonstration

Sonic State video interview here:

https://www.youtube.com/watch?v=_78TSNxpDYs