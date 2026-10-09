---
layout: kb
title: How Network MIDI 2.0 works in Windows
audience: everyone
description: What the Windows Network MIDI 2.0 transport does, what it does not do in this first release, and the configuration file entries behind it.
categories:
  - Transport Details
---

> <h4>First release</h4>
> **This release does not support authentication.** A host on this PC accepts connections based
> on your approval choice, not on a password. Details are in
> [Authentication is not in this release](#authentication-is-not-in-this-release) below.

Network MIDI 2.0 carries MIDI over your existing Ethernet or Wi-Fi network, with no MIDI cables and no audio interface in between. A device connected this way appears in Windows like any other MIDI device, so your DAW and other MIDI software can use it straight away.

Windows can work in either direction, and both at once:

- **As a client**, this PC connects out to a synth, an interface, or another computer.
- **As a host**, this PC accepts connections from other devices.

To set any of this up, use the Network MIDI Setup app. This page does not repeat what that app does; see [Network MIDI Setup]({{ site.baseurl }}/tools/midinetworksetup/) for the walkthrough.

## What you need

**The two ends must be on the same network and the same subnet.** Devices announce themselves locally, and those announcements do not normally cross between separate networks, or between a guest network and your main one.

**To accept incoming connections, the service must be allowed through the firewall.** This is the single most common reason other devices can see this PC but cannot connect to it: they see the advertisement, and their connection request is dropped before the service ever sees it. The **Firewall** page in [Network MIDI Setup]({{ site.baseurl }}/tools/midinetworksetup/#windows-firewall) does it for you. To do it by hand, see [Adding Network MIDI 2 to your firewall]({{ site.baseurl }}/kb/network-midi-firewall/).

Connecting *out* from this PC does not need a firewall change.

**Wired is better than Wi-Fi for anything timing-critical.** A Wi-Fi radio sleeps between messages, so the first message after a quiet moment waits for it to wake. Most wired networks give round trip times under a millisecond.

## Authentication is not in this release

The Network MIDI 2.0 specification defines two optional authentication schemes: a shared password for a host, and a user name and password per user. **Neither is supported in this release of Windows MIDI Services.** Support is planned as a later update.

What that means in practice:

- **A host on this PC cannot be password protected.** Control who may connect using the approval policy instead: choose **Ask me first** so that nothing connects until you allow it, and use **Block** to refuse a device permanently. This is a real control, and for a home or studio network it is usually the right one.
- **This PC cannot connect to a remote device that requires a password.** The remote device will ask for authentication and Windows will end the session rather than pretend to authenticate.
- **A host asking for authentication will not start at all.** If a configuration file names `"authentication": "password"` or `"user"`, that host is refused at configuration time with `AuthenticationNotImplemented`. It is deliberately not downgraded to an unprotected host, because silently removing protection somebody asked for would be worse than refusing.

If you need to keep other devices off a host, put it on a network you control, or use **Ask me first** and approve only the devices you recognize.

## How connections behave

**Discovery is automatic.** Devices that advertise themselves appear on their own, usually within a few seconds. There is nothing to scan.

**Connecting can need action at the other end.** Many devices will not simply accept an incoming connection; they wait for you to approve it on a web page, a companion app, or a physical button. If a connection sits at "Connecting", check the device itself. Windows keeps trying, so once you allow it at the other end the connection completes on its own.

**Connections are remembered and re-established on their own.** A connection you set up survives a restart of the service or the PC, with no app running. If the device is switched off, the entry stays and connects when the device reappears.

**A device that does not advertise can be connected by address.** Advertised connections are more efficient, because Windows waits for an announcement rather than repeatedly probing an address.

**A device with more than one address is tried at each one in turn.** A device can have an IPv4 address and several IPv6 addresses, and its announcement doesn't say which one to use. Windows tries them in the same order it uses for every other app on this PC. That's usually IPv6 first, but an IPv6 address starting with `fd` comes after IPv4, and an address this PC has no way to reach is skipped. If the device doesn't answer at one address, Windows tries the next one right away. Once a connection works, Windows tries that address first next time. To always use one particular address, connect to the device by that address. An administrator can also make the whole PC [prefer IPv4 over IPv6](https://learn.microsoft.com/troubleshoot/windows-server/networking/configure-ipv6-in-windows).

**Naming.** Leave the name empty and Windows uses whatever the device calls itself. Give a name and that is what appears everywhere in Windows, including your DAW's device list.

**MIDI 1.0 ports are created by default.** Alongside the modern UMP endpoint, Windows creates classic MIDI 1.0 ports so that older software, which does not understand the newer combined API, can use the device. Most apps today fall into that category. This applies in both directions: to devices this PC connects out to, and to devices that connect in to a host here. You can turn it off per entry if you only want the UMP endpoint.

**A device that loses data can be sent to more slowly.** Some devices can't keep up when a lot of data arrives at once, like a long SysEx dump. Set a sending speed for the host or the device in Network MIDI Setup. A single note is never held back. See [Sending speed](#sending-speed).

---

## Details

The rest of this page is for people who want to understand the transport itself, or who edit the configuration file by hand.

## What is implemented

The transport implements the MIDI Association *Network MIDI 2.0 (UDP Transport)* specification, M2-124-UM version 1.0.

| Area | State |
|---|---|
| UDP transport, sessions, invitations | Implemented |
| Retransmission and forward error correction | Implemented |
| mDNS / DNS-SD discovery and advertisement | Implemented |
| Client role and host role, simultaneously | Implemented |
| Remote client approval, allow and deny lists | Implemented |
| Authentication (shared secret, user credential) | **Not implemented** |

A command the transport does not recognize is answered inside an established session with `NAK`, reason `CommandNotSupported`, and is ignored outside one. Commands added to the specification later are therefore refused cleanly rather than half-handled.

## Identity on the network

A host is advertised over DNS-SD as `_midi2._udp.local`.

| Item | Rule |
|---|---|
| Service instance name | A single DNS label, so at most **63 bytes** once encoded as UTF-8. Must be unique among hosts on this PC |
| UMP endpoint name | Up to 98 characters, per the MIDI 2.0 specification |
| Product instance id | Up to 42 characters, per the MIDI 2.0 specification |
| Port | A specific port, or allocated automatically |

Datagrams are capped at a 1400 byte UDP payload, which keeps the whole packet inside a standard 1500 byte Ethernet MTU for both IPv4 and IPv6. `DontFragment` is set, so an oversized datagram is dropped rather than fragmented.

## The configuration file

Settings live in `C:\ProgramData\Microsoft\MIDI\WindowsMidiServices.midiconfig.json`, under the Network MIDI 2.0 transport's identifier.

The file is designed to be portable: copying it to another PC carries your setup with it.

A few rules apply to the whole file:

- **All keys are case-sensitive, including GUIDs.**
- GUID keys are written **braced and upper case**, for example `{C95DCD1F-...}`.
- Port numbers are written as **strings**, not numbers.
- A `"_comment"` key may appear anywhere. It is written by the tools to make the file readable and is never read back by the service.

```json
{
  "endpointTransportPluginSettings": {
    "{C95DCD1F-CDE3-4C2D-913C-528CB8A4CBE6}": {
      "_comment": "Network MIDI 2.0 (UDP)",
      "transportSettings": {
        "maxForwardErrorCorrectionCommandPackets": 2,
        "maxRetransmitBufferCommandPackets": 250,
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
            "createMidi1Ports": true,
            "fallbackMidi1PortCount": 1,
            "sendSpeedLimit": 0,
            "reduceSendSpeedAutomatically": false,
            "allowedClients": [
              { "umpEndpointName": "Bome BomeBox", "productInstanceId": "kb7C5D0A_1" }
            ],
            "deniedClients": []
          }
        },
        "clients": {
          "{971DA520-4559-4A2F-A72E-F9BEA17521CC}": {
            "networkProtocol": "udp",
            "createMidi1Ports": true,
            "fallbackMidi1PortCount": 1,
            "sendSpeedLimit": 4,
            "reduceSendSpeedAutomatically": true,
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
| `maxRetransmitBufferCommandPackets` | 250 | 0 – 1000 | New connections |
| `outboundPingInterval` | 2000 ms | 250 – 120000 | Within one interval, including open sessions |
| `invitationPendingTimeout` | 120000 ms | 1000 – 600000 | Invitations from that point on |
| `maxHostConnections` | 64 | 1 – 512 | Immediately, checked per invitation |
| `directConnectionScanInterval` | 20000 ms | 250 – 300000 | Next scan |

Two behaviors are worth knowing:

**Values are clamped, never refused.** A value out of range becomes the nearest bound, and a missing or wrong-typed value becomes the default. A warning is written to the trace when anything was corrected. Read the settings back to see what was actually taken.

**Sending a partial `transportSettings` object resets the keys you left out.** Parsing starts from the defaults each time; it is not a merge. Read the current settings, change what you need, and send the whole object back.

Lowering `maxHostConnections` does not disconnect clients that are already connected.

`maxRetransmitBufferCommandPackets` is how many recent packets each connection keeps, so it can send one again when the other device missed it. Each connection also keeps no more than 256 KB of them, so a connection sending large SysEx keeps fewer.

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
| `createMidi1Ports` | boolean | Default true. Create classic MIDI 1.0 ports for connected devices |
| `fallbackMidi1PortCount` | number | Default 1, range 1 – 16. Source and destination ports to create for a device that declares no function blocks. See [MIDI 1.0 ports](#midi-10-ports) |
| `sendSpeedLimit` | number | Default 0, no limit. How fast this host sends to each connected device, as a multiple of MIDI 1.0 wire speed: 1, 2, 4, 8, 16 or 32. See [Sending speed](#sending-speed) |
| `reduceSendSpeedAutomatically` | boolean | Default false. Send more slowly while a device keeps asking for data again. See [Slowing down by itself](#slowing-down-by-itself) |
| `allowedClients` | array | Identity objects that may connect without asking |
| `deniedClients` | array | Identity objects that are refused without asking |

A client identity is `{ "umpEndpointName": "...", "productInstanceId": "..." }`. Identity is used rather than IP address, because a device's address moves and its identity does not.

### Client entries

| Key | Type | Notes |
|---|---|---|
| `networkProtocol` | string | Only `"udp"` |
| `createMidi1Ports` | boolean | Default true. Create classic MIDI 1.0 ports for this device |
| `fallbackMidi1PortCount` | number | Default 1, range 1 – 16. Source and destination ports to create when the device declares no function blocks. See [MIDI 1.0 ports](#midi-10-ports) |
| `sendSpeedLimit` | number | Default 0, no limit. How fast this PC sends to the device, as a multiple of MIDI 1.0 wire speed: 1, 2, 4, 8, 16 or 32. See [Sending speed](#sending-speed) |
| `reduceSendSpeedAutomatically` | boolean | Default false. Send more slowly while the device keeps asking for data again. See [Slowing down by itself](#slowing-down-by-itself) |
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
| `pending` | Configured, waiting for the service to connect. Also where an entry waits after a host said it was busy, and where a direct connection waits between tries |
| `live` | Connected, endpoint created |
| `failed` | The entry itself is not valid, or the host turned the connection down, so retrying cannot help |
| `unavailable` | Not reported at present. A direct connection that stops answering goes back to `pending` and is tried again |

## MIDI 1.0 ports

A Network MIDI 2.0 device describes itself with **function blocks**, which it sends when Windows asks it to during endpoint discovery. Those say how many groups the device uses and in which direction, and Windows creates one MIDI 1.0 source and one destination per group from them. A device that describes itself properly needs nothing configured here.

Not every device answers. Some never complete discovery, and unlike USB there are no group terminal blocks to fall back on, so Windows would have nothing to build ports from. For that case the transport supplies a block of its own, and `fallbackMidi1PortCount` says how wide it is: the value is the number of source ports and also the number of destination ports. It defaults to **1**, because a device which does not describe itself is usually a bridge with a single cable, and 16 would mean 32 ports of clutter in every MIDI 1.0 application.

**The fallback is ignored the moment the device does describe itself.** Function blocks take precedence, so raising the count for a device that declares three groups changes nothing.

Two things behave differently when you change them:

| Setting | When it takes effect |
|---|---|
| `fallbackMidi1PortCount` | Straight away, on connections that are already up. Ports are added or removed without the session being interrupted |
| `createMidi1Ports` | The next time the endpoint is created, so disconnect and reconnect, or restart the service |

The difference is not arbitrary. Whether an endpoint has MIDI 1.0 ports at all is settled when the endpoint is built and cannot be changed underneath a running one; how many ports it has is driven by properties the service watches, so that can be rewritten live.

## Sending speed

Some devices lose data when a lot of it arrives at once. A hardware synth taking a long SysEx dump, or a network to DIN bridge with a small buffer, may have been built for the speed of a MIDI 1.0 cable. A network is many times faster than that.

Every host and every client entry has a sending speed. It limits how fast this PC sends to the other device. It doesn't change what this PC receives.

A host can also give one device a speed of its own. The host uses that speed for the device instead of its own, so a slow synth can get MIDI 1.0 wire speed while a computer connected to the same host gets no limit. The host knows the device by its name and product instance id, ignoring uppercase and lowercase differences, the same way it remembers whether to let the device in. So the device gets its own speed every time it connects, and right away if it's already connected. Network MIDI Setup shows each connected device's speed under its host, with a **Change** link. In PowerShell, use [`Set-MidiNetworkRemoteClientSendSpeed`]({{ site.baseurl }}/tools/powershell/#set-midinetworkremoteclientsendspeed-and-remove-midinetworkremoteclientsendspeed).

| `sendSpeedLimit` | Speed |
|---|---|
| `0` | No limit. This is the default |
| `1` | MIDI 1.0 wire speed: 31,250 bits a second, the speed of a DIN cable |
| `2`, `4`, `8`, `16`, `32` | That many times MIDI 1.0 wire speed. `32` is about 1 megabit a second |

A number above 32 means no limit. The speed is measured in the bytes the same messages would take on a MIDI 1.0 cable, so at `1` a SysEx dump takes about as long as it would over a cable.

**A single message is never held back.** After a quiet moment, a short burst goes out at once: 64 bytes at wire speed, and 64 more for each step up. Only what comes after that is spaced out. So somebody playing a keyboard won't notice a limit, while a 3,000 byte SysEx dump at wire speed takes about a second, as it would on a cable.

**Nothing is dropped to keep to the limit.** Windows holds the extra messages and sends them as fast as the limit allows. If an app keeps sending faster than that, its sends slow down to match.

**A change applies right away**, including to connections that are already up, without disconnecting them.

### Slowing down by itself

When a device misses a packet, it asks for it again. With `reduceSendSpeedAutomatically` turned on, Windows takes that as a sign the device can't keep up:

- Each time the device asks again, the connection halves its speed, down to MIDI 1.0 wire speed. A connection with no limit drops to 32 times wire speed first. Requests that arrive together count once.
- After 10 seconds without a request, it tries the next speed up, until it's back to `sendSpeedLimit`.
- If the faster speed causes trouble again, it waits twice as long before the next try, up to about 5 minutes. After 10 minutes without trouble, it goes back to waiting 10 seconds.

Each connection keeps its own speed. Network MIDI Setup shows when a connection has slowed down, and so does `CurrentSendSpeedLimit` in the API.

## Network adapters

A PC can be on several networks at once. A host can run on all of them, which is the default, or be limited to one network adapter. A host limited to one adapter advertises itself only on that adapter, and only answers devices that reach it through that adapter.

Windows knows each adapter by a GUID. A USB network adapter often gets a new GUID when it's plugged into a different USB port, so the adapter's hardware address is remembered too. When no adapter has the GUID anymore, the adapter with that hardware address is used instead.

An adapter counts as missing when it's gone, turned off, or doesn't have an IP address yet. What the host does then depends on its fallback setting:

| Fallback | While the adapter is missing |
|---|---|
| On, the default | The host runs on every adapter, and moves back to its own adapter when it's back |
| Off | The host doesn't run. It starts by itself when the adapter is back |

A host that's waiting for its adapter shows a warning in [Network MIDI Setup]({{ site.baseurl }}/tools/midinetworksetup/#choosing-a-network-adapter), and MIDI Notifications tells you about it. Changing a host's adapter restarts the host, which ends its connections.

## Approval

When a host uses `requireApproval`, an unknown device is held pending until somebody answers. A decision has a scope:

| Scope | Effect |
|---|---|
| `once` | This connection only. Held in memory |
| `untilRestart` | Until the service restarts. Held in memory |
| `always` | Written to `allowedClients` or `deniedClients` in the configuration file |

A device left pending is dropped after `invitationPendingTimeout`, which defaults to two minutes because it is scaled to somebody walking over to a device rather than to a network round trip.

## See also

- [Network MIDI Setup]({{ site.baseurl }}/tools/midinetworksetup/)
- [Adding Network MIDI 2 to your firewall]({{ site.baseurl }}/kb/network-midi-firewall/)
- [MidiNetworkTransportManager]({{ site.baseurl }}/sdk-reference/Transports/Network/MidiNetworkTransportManager)
- [MidiNetworkTransportSettings]({{ site.baseurl }}/sdk-reference/Transports/Network/MidiNetworkTransportSettings)
