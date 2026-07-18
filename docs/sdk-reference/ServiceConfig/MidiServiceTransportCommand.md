---
layout: sdk_reference_page
title: MidiServiceTransportCommand
namespace: Windows.Devices.Midi2.ServiceConfig
type: runtimeclass
implements: Windows.Devices.Midi2.ServiceConfig.IMidiServiceTransportPluginConfig
description: A command to send to a transport plugin in the MIDI service
---

This class represents a command (verb + arguments) to be sent to a transport plugin via `MidiServiceTransportPluginConfigManager.SendCommand()`. Common verbs are available as static properties on `MidiServiceTransportCommonCommands`.

## Constructors

| Constructor | Description |
| ----------- | ----------- |
| `MidiServiceTransportCommand(transportId)` | Create an empty command for the specified transport |
| `MidiServiceTransportCommand(transportId, verb)` | Create a command with the specified verb |
| `MidiServiceTransportCommand(transportId, verb, arguments)` | Create a command with the specified verb and arguments |

## Properties

| Property | Description |
| -------- | ----------- |
| `Verb` | The command verb string (e.g., from `MidiServiceTransportCommonCommands`) |
| `Arguments` | A map of string key/value argument pairs for this command |
