---
layout: kb
title: How Network MIDI 2.0 works in Windows
audience: everyone
description: What the Windows Network MIDI 2.0 transport does, what it does not do in this first release, and the configuration file entries behind it.
---

> <h4>First release</h4>
> **This release does not support authentication.** A host on this PC accepts connections based
> on your approval choice, not on a password. Details are in
> [Authentication is not in this release](#authentication-is-not-in-this-release) below.

Network MIDI 2.0 carries MIDI over your existing Ethernet or Wi-Fi network, with no MIDI cables
and no audio interface in between. A device connected this way appears in Windows like any other
MIDI device, so your DAW and other MIDI software can use it straight away.

Windows can work in either direction, and both at once:

- **As a client**, this PC connects out to a synth, an interface, or another computer.
- **As a host**, this PC accepts connections from other devices.

To set any of this up, use the Network MIDI 2.0 Setup app. This page does not repeat what that
app does; see [Network MIDI 2.0 Setup]({{ site.baseurl }}/tools/midinetworksetup/) for the
walkthrough.

## What you need

**The two ends must be on the same network and the same subnet.** Devices announce themselves
locally, and those announcements do not normally cross between separate networks, or between a
guest network and your main one.

**To accept incoming connections, the service must be allowed through the firewall.** This is the
single most common reason other devices can see this PC but cannot connect to it: they see the
advertisement, and their connection request is dropped before the service ever sees it. See
[Adding Network MIDI 2 to your firewall]({{ site.baseurl }}/kb/network-midi-firewall/).

Connecting *out* from this PC does not need a firewall change.

**Wired is better than Wi-Fi for anything timing-critical.** A Wi-Fi radio sleeps between
messages, so the first message after a quiet moment waits for it to wake. Most wired networks
give round trip times under a millisecond.

## Authentication is not in this release

The Network MIDI 2.0 specification defines two optional authentication schemes: a shared password
for a host, and a user name and password per user. **Neither is supported in this release of
Windows MIDI Services.** Support is planned as a later update.

What that means in practice:

- **A host on this PC cannot be password protected.** Control who may connect using the approval
  policy instead: choose **Ask me first** so that nothing connects until you allow it, and use
  **Block** to refuse a device permanently. This is a real control, and for a home or studio
  network it is usually the right one.
- **This PC cannot connect to a remote device that requires a password.** The remote device will
  ask for authentication and Windows will end the session rather than pretend to authenticate.
- **A host asking for authentication will not start at all.** If a configuration file names
  `"authentication": "password"` or `"user"`, that host is refused at configuration time with
  `AuthenticationNotImplemented`. It is deliberately not downgraded to an unprotected host,
  because silently removing protection somebody asked for would be worse than refusing.

If you need to keep other devices off a host, put it on a network you control, or use **Ask me
first** and approve only the devices you recognize.

## How connections behave

**Discovery is automatic.** Devices that advertise themselves appear on their own, usually within
a few seconds. There is nothing to scan.

**Connecting can need action at the other end.** Many devices will not simply accept an incoming
connection; they wait for you to approve it on a web page, a companion app, or a physical button.
If a connection sits at "Connecting", check the device itself. Windows keeps trying, so once you
allow it at the other end the connection completes on its own.

**Connections are remembered and re-established on their own.** A connection you set up survives
a restart of the service or the PC, with no app running. If the device is switched off, the entry
stays and connects when the device reappears.

**A device that does not advertise can be connected by address.** Advertised connections are more
efficient, because Windows waits for an announcement rather than repeatedly probing an address.

**Naming.** Leave the name empty and Windows uses whatever the device calls itself. Give a name
and that is what appears everywhere in Windows, including your DAW's device list.

**MIDI 1.0 ports are optional.** A host can create classic MIDI 1.0 ports for connected devices
so that older software, which does not understand the newer combined API, can use them. Most
apps today fall into that category.

---

# Details

The rest of this page is for people who want to understand the transport itself, or who edit the
configuration file by hand.

## What is implemented

The transport implements the MIDI Association *Network MIDI 2.0 (UDP Transport)* specification,
M2-124-UM version 1.0.

| Area | State |
|---|---|
| UDP transport, sessions, invitations | Implemented |
| Retransmission and forward error correction | Implemented |
| mDNS / DNS-SD discovery and advertisement | Implemented |
| Client role and host role, simultaneously | Implemented |
| Remote client approval, allow and deny lists | Implemented |
| Authentication (shared secret, user credential) | **Not implemented** |

A command the transport does not recognize is answered inside an established session with
`NAK`, reason `CommandNotSupported`, and is ignored outside one. Commands added to the
specification later are therefore refused cleanly rather than half-handled.

## Identity on the network

A host is advertised over DNS-SD as `_midi2._udp.local`.

| Item | Rule |
|---|---|
| Service instance name | A single DNS label, so at most **63 bytes** once encoded as UTF-8. Must be unique among hosts on this PC |
| UMP endpoint name | Up to 98 characters, per the MIDI 2.0 specification |
| Product instance id | Up to 42 characters, per the MIDI 2.0 specification |
| Port | A specific port, or allocated automatically |

Datagrams are capped at a 1400 byte UDP payload, which keeps the whole packet inside a standard
1500 byte Ethernet MTU for both IPv4 and IPv6. `DontFragment` is set, so an oversized datagram is
dropped rather than fragmented.

## The configuration file

Settings live in `C:\ProgramData\Microsoft\MIDI\WindowsMidiServices.midiconfig.json`, under the
Network MIDI 2.0 transport's identifier.

The file is designed to be portable: copying it to another PC carries your setup with it.

A few rules apply to the whole file:

- **All keys are case-sensitive, including GUIDs.**
- GUID keys are written **braced and upper case**, for example `{C95DCD1F-...}`.
- Port numbers are written as **strings**, not numbers.
- A `"_comment"` key may appear anywhere. It is written by the tools to make the file readable and
  is never read back by the service.

```json
{
  "endpointTransportPluginSettings": {
    "{C95DCD1F-CDE3-4C2D-913C-528CB8A4CBE6}": {
      "_comment": "Network MIDI 2.0 (UDP)",
      "transportSettings": {
        "maxForwardErrorCorrectionCommandPackets": 2,
        "maxRetransmitBufferCommandPackets": 50,
        "outboundPingInterval": 2000,
        "invitationPendingTimeout": 120000,
        "maxHostConnections": 64,
        "directConnectionScanInterval": 20000
      },
      "create": {
        "hosts": {
          "{8E2F1A44-0C7B-4E5D-9A31-6B0F2D8C1E77}": {
            "name": "Windows MIDI Services Host",
            "serviceInstanceName": "studio-pc",
            "productInstanceId": "3263827-5150Net2Preview",
            "networkProtocol": "udp",
            "port": "auto",
            "allowPortFallback": true,
            "enabled": true,
            "advertise": true,
            "authentication": "none",
            "remoteClientPolicy": "requireApproval",
            "createMidi1Ports": false,
            "allowedClients": [
              { "umpEndpointName": "Bome BomeBox", "productInstanceId": "kb7C5D0A_1" }
            ],
            "deniedClients": []
          }
        },
        "clients": {
          "{971DA520-4559-4A2F-A72E-F9BEA17521CC}": {
            "networkProtocol": "udp",
            "match": {
              "directHostNameOrIP": "192.168.1.253",
              "directPort": "5004"
            }
          }
        }
      }
    }
  }
}
```

### Transport settings

These apply to the transport as a whole rather than to one host or client.

| Key | Default | Range | When it takes effect |
|---|---|---|---|
| `maxForwardErrorCorrectionCommandPackets` | 2 | 0 – 10 | New connections |
| `maxRetransmitBufferCommandPackets` | 50 | 0 – 1000 | New connections |
| `outboundPingInterval` | 2000 ms | 250 – 120000 | Within one interval, including open sessions |
| `invitationPendingTimeout` | 120000 ms | 1000 – 600000 | Invitations from that point on |
| `maxHostConnections` | 64 | 1 – 512 | Immediately, checked per invitation |
| `directConnectionScanInterval` | 20000 ms | 250 – 300000 | Next scan |

Two behaviors are worth knowing:

**Values are clamped, never refused.** A value out of range becomes the nearest bound, and a
missing or wrong-typed value becomes the default. A warning is written to the trace when anything
was corrected. Read the settings back to see what was actually taken.

**Sending a partial `transportSettings` object resets the keys you left out.** Parsing starts from
the defaults each time; it is not a merge. Read the current settings, change what you need, and
send the whole object back.

Lowering `maxHostConnections` does not disconnect clients that are already connected.

### Host entries

| Key | Type | Notes |
|---|---|---|
| `name` | string | The UMP endpoint name other devices see |
| `serviceInstanceName` | string | DNS-SD label, max 63 UTF-8 bytes, unique on this PC |
| `productInstanceId` | string | Max 42 characters |
| `networkProtocol` | string | Only `"udp"` |
| `port` | string | `"auto"`, or a port number as a string |
| `allowPortFallback` | boolean | Default true. With a specific port, falls back to an automatic one rather than refusing to start |
| `enabled` | boolean | |
| `advertise` | boolean | Announce over mDNS. With this off, devices must be given the address |
| `authentication` | string | `"none"` only. `"password"` and `"user"` are refused in this release |
| `remoteClientPolicy` | string | `"allowAny"` or `"requireApproval"` |
| `createMidi1Ports` | boolean | Create classic MIDI 1.0 ports for connected devices |
| `allowedClients` | array | Identity objects that may connect without asking |
| `deniedClients` | array | Identity objects that are refused without asking |

A client identity is `{ "umpEndpointName": "...", "productInstanceId": "..." }`. Identity is used
rather than IP address, because a device's address moves and its identity does not.

### Client entries

| Key | Type | Notes |
|---|---|---|
| `networkProtocol` | string | Only `"udp"` |
| `match` | object | How to find the device |

`match` carries either an advertised identity or a direct address:

| Key | Notes |
|---|---|
| `id` | Windows device id from discovery, such as `DnsSd#kb7C5D0A_1._midi2._udp.local#0` |
| `serviceInstance` | The advertised instance name, such as `kb7C5D0A_1` |
| `umpEndpointName` | The device's own endpoint name |
| `umpProductInstanceId` | The device's product instance id |
| `directHostNameOrIP` | An address or host name. Requires `directPort` |
| `directPort` | Port number as a string. Requires `directHostNameOrIP` |

A configured client is in one of four states:

| State | Meaning |
|---|---|
| `pending` | Configured, waiting for the service to connect |
| `live` | Connected, endpoint created |
| `failed` | The entry itself is not valid, so retrying cannot help |
| `unavailable` | A direct connection stopped answering. Nothing will announce its return, so it is retried only when an app asks again |

## Approval

When a host uses `requireApproval`, an unknown device is held pending until somebody answers.
A decision has a scope:

| Scope | Effect |
|---|---|
| `once` | This connection only. Held in memory |
| `untilRestart` | Until the service restarts. Held in memory |
| `always` | Written to `allowedClients` or `deniedClients` in the configuration file |

A device left pending is dropped after `invitationPendingTimeout`, which defaults to two minutes
because it is scaled to somebody walking over to a device rather than to a network round trip.

## See also

- [Network MIDI 2.0 Setup]({{ site.baseurl }}/tools/midinetworksetup/)
- [Adding Network MIDI 2 to your firewall]({{ site.baseurl }}/kb/network-midi-firewall/)
- [MidiNetworkTransportManager]({{ site.baseurl }}/sdk-reference/Transports/Network/MidiNetworkTransportManager)
- [MidiNetworkTransportSettings]({{ site.baseurl }}/sdk-reference/Transports/Network/MidiNetworkTransportSettings)
