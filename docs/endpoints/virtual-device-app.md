---
layout: page
title: Virtual Device App
parent: Transport Types
grandparent: Windows MIDI Services
has_children: false
---

# Virtual Device App

| Property | Value |
| -------- | ----- |
| Abstraction Id | `{8FEAAD91-70E1-4A19-997A-377720A719C1}` |
| Abbreviation | `APP` |

## Overview

One way to have app-to-app MIDI on Windows is to use a simple loopback. That is typically created ahead of time, and is available for any applications to use to communicate with each other. The lifetime of these loopback endpoints are not tied to any one application. They are simply a pipe between applications.

Another approach is to allow applications to create and publish an endpoint which is declared through settings inside the application itself. When the application closes, the endpoint closes. That is the model the Virtual Device App feature implements. (If you want the simple pre-created loopback, see the Virtual Loopback transport)

In addition, MIDI 2.0 has additional requirements for endpoints. They need to be able to participate in the MIDI Endpoint Discovery process, and respond with appropriate endpoint capabilities and preferred settings. The Virtual Device App makes that configuration simple for application developers and musicians.

### Lifetime

* The Virtual UMP Device includes two endpoints: one is just for the application creating the device, the other is for all other applications to connect to.
* The lifetime of the Virtual UMP Device is controlled by the lifetime of the application's connection. Once the owning application disconnects from the endpoint, the Virtual UMP Device is torn down.

## Suggested Uses

This is the right kind of endpoint to create if you have an application which operates as a synthesizer, tone generator, or an input device, and you want the app to act and be seen in the same way a piece of MIDI hardware would be seen. 

Today, the API provides support for MIDI 2.0 / UMP protocol. In the future, we'll add MIDI CI and other capabilities to these virtual devices (today, those need to be done manually or through a third-party library).

## Compatibility

The best experience for Windows MIDI Services will be with applications using the new Windows MIDI Services SDK. Only apps using the new SDK, and which are MIDI 2.0-aware, will be able to define and host Virtual Devices. 

| API | Compatible App User | Compatible App Host | Notes |
| --- | ---------- | ---------- | ----- |
| Windows MIDI Services Native | Yes | Yes | This provides the best experience for applications. |
| WinRT MIDI 1.0 | Yes | No | Any applications using the WinRT MIDI 1.0 API we shipped with Windows 10/11 will be able to see these devices. Note that they can only send/receive MIDI 1.0 messages, and will not see any MIDI 2.0-specific messages that cannot be translated, including stream messages. |
| WinMM MIDI 1.0 | No | No | There are challenges with the port indexes with the WinMM API. For the forseeable future, we will not surface these virtual devices with the WinMM API. This means that most MIDI 1.0 applications will not recognize these devices. |

## Configuration

Because this type of endpoint is tied to the runtime of an application, it can be created only through the API by creating a `MidiVirtualDeviceDefinition` and calling the appropriate `MidiSession` methods to create and open the endpoint. This is not something that a MIDI user will pre-create in the settings app or configuration file.
