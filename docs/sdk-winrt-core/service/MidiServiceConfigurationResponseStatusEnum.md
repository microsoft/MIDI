---
layout: api_page
title: MidiServiceConfigurationResponseStatus
parent: Service
grand_parent: Midi2
has_children: false
---

# MidiServiceConfigurationResponseStatus Enumeration

Indicates success or failure mode for configuring an endpoint or message processing plugin in the service.

## Properties

| Property | Value | Description |
| -------- | ------- | ------ |
| `Success` | `0` | The entire operation succeeded |
| `ErrorTargetNotFound` | `404` | The target of the change was not found. This could be the plugin itself, or if the configuration requires an endpoint, the endpoint instance.  |
| `ErrorJsonNullOrEmpty` | `600` | The supplied JSON was null or empty. |
| `ErrorProcessingJson` | `601` | The supplied JSON was invalid in some way. It could be malformed or have missing required data or keys. |
| `ErrorNotImplemented` | `2600` | One or more of the requests are not implemented by the service or by the plugin. |
| `ErrorOther` | `9999` | All other errors |

## IDL

[MidiServiceConfigurationResponseStatus IDL](https://github.com/microsoft/MIDI/blob/main/src/app-sdk/winrt-core/MidiServiceConfigurationResponseStatusEnum.idl)
