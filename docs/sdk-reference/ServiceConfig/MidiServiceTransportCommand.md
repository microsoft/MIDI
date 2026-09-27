---
layout: sdk_reference_page
title: MidiServiceTransportCommand
namespace: Windows.Devices.Midi2.ServiceConfig
type: runtimeclass
implements: Windows.Devices.Midi2.ServiceConfig.IMidiServiceTransportPluginConfig
description: A command to send to a transport plugin in the MIDI service
---

A command to send to a transport with `MidiServiceTransportPluginConfigManager.SendCommand()`. A command has a verb, which says what to do, and arguments, which give the details. `MidiServiceTransportCommonCommands` has the common verbs.

## Constructors

| Constructor | Description |
| ----------- | ----------- |
| `MidiServiceTransportCommand()` | Creates an empty command with no transport id. You can't set `TransportId` later, so use one of the other constructors if you plan to send it |
| `MidiServiceTransportCommand(transportId)` | Creates an empty command for this transport |
| `MidiServiceTransportCommand(transportId, verb)` | Creates a command with this verb |
| `MidiServiceTransportCommand(transportId, verb, arguments)` | Creates a command with this verb and these arguments |

## Properties

| Property | Description |
| -------- | ----------- |
| `Verb` | The command's verb, such as one from `MidiServiceTransportCommonCommands` |
| `Arguments` | The command's arguments, as pairs of text keys and values |
| `TransportId` | The GUID of the transport the command is for. From `IMidiServiceTransportPluginConfig` |
| `ConfigJson` | The command, as JSON. From `IMidiServiceTransportPluginConfig` |
