---
layout: kb
title: About the Kernel Streaming Aggregate Transport
audience: everyone
description: All about the Kernel Streaming Aggregate transport, which is the service code which talks to existing MIDI 1.0 drivers, whether vendor-supplied or in-box.
---

| Property | Value |
| -------- | ----- |
| Transport Id | `{0F273B18-E372-4D95-87AC-C31C3D22E937}` |
| Abbreviation | `KSA` |

## Overview

The Kernel Streaming Aggregator transport creates UMP endpoints for MIDI 1.0 devices using MIDI 1.0 byte format devices, like third-party MIDI 1.0 drivers, and our in-box USB MIDI 1.0 Class Driver.

## Suggested Uses

Although this transport is primarily recommended for USB devices, it may be used for access to most other existing MIDI 1.0 devices on Windows as long as they have a Kernel Streaming driver.

## Compatibility

The best experience for Windows MIDI Services will be with applications using the new Windows MIDI Services SDK.

| API | Compatible | Notes |
| --- | ---------- | ----- |
| Windows MIDI Services Native | Yes | Data is translated between UMP and MIDI bytes (MIDI 1.0 data format) as required. |
| WinRT MIDI 1.0 | Yes | Data is translated between UMP and MIDI bytes (MIDI 1.0 data format) as required. |
| WinMM MIDI 1.0 | Yes | Data is translated between UMP and MIDI bytes (MIDI 1.0 data format) as required. |

## Grouping

When creating MIDI 2.0 / UMP Endpoints, the aggregation happens at the device level. All interfaces and pins under the device are aggregated and presented as a single UMP endpoint.

## Naming

MIDI 1.0 port names use a new algorithm for naming, compared to what we have had in the past. Specifically, the device interface (the filter) name is the default root of the name. If there is no filter name, then we walk up to the parent device and use that device's name as the root. Next, we query for the pin names, if provided via `iJack`, and append that to the name.

> When the MIDI 1.0 endpoints for a device are combined into a single UMP endpoint for use in Windows MIDI Services, the MIDI 1.0 port names, without the parent device name, are used as the virtual Group Terminal Block names. The groups are mapped to the individual IO pins, starting at 0 (input and output both start at 0) and increasing monotonically for both input and output.

## Configuration

Endpoints for this transport are not created through the configuration file, but certain properties, such as the name and description, are updatable through it.

```json
"endpointTransportPluginSettings":
{
    "{0F273B18-E372-4D95-87AC-C31C3D22E937}":
    {
        "_comment": "KSA MIDI (USB etc.)",
        "SWD: \\\\?\\swd#midisrv#midiu_ksa_6799286025327820155#{e7cce071-3c03-423f-88d3-f1045d02552b}":
        {
            
        }
    }
}
```

> TODO: Need to add options for the WinMM port numbers and more


> If you want to see how Windows sees any given device at the Kernel Streaming interface level, run the SDK tool `midiksinfo`. That will list all KS devices as they are seen before we create MIDI endpoints for them.
