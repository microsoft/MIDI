---
layout: page
title: Kernel Streaming Aggregate
parent: Transport Types
grandparent: Windows MIDI Services
has_children: false
---

# Kernel Streaming

| Property | Value |
| -------- | ----- |
| Abstraction Id | `TBA` |
| Abbreviation | `KSA` |

## Overview

The Kernel Streaming Aggregator transport creates UMP endpoints for MIDI 1.0 devices using MIDI 1.0 byte format devices, like third-party MIDI 1.0 drivers.

> NOTE: This transport may be recombined with the base Kernel Streaming transport in the future

## Suggested Uses

Although this transport is primarily recommended for USB devices, it may be used for access to most other existing MIDI 1.0 devices on Windows.

## Compatibility

The best experience for Windows MIDI Services will be with applications using the new Windows MIDI Services SDK.

| API | Compatible | Notes |
| --- | ---------- | ----- |
| Windows MIDI Services Native | Yes | Data is translated between UMP and MIDI bytes (MIDI 1.0 data format) as required. |
| WinRT MIDI 1.0 | Yes | Data is translated between UMP and MIDI bytes (MIDI 1.0 data format) as required. |
| WinMM MIDI 1.0 | Yes | Data is translated between UMP and MIDI bytes (MIDI 1.0 data format) as required. |

## Naming

MIDI 1.0 port names use a new algorithm for naming, compared to what we have had in the past. Specifically, the device interface (the filter) name is the default root of the name. If there is no filter name, then we walk up to the parent device and use that device's name as the root. Next, we query for the pin names, if provided via `iJack`, and append that to the name.

> When the MIDI 1.0 endpoints for a device are combined into a single UMP endpoint for use in Windows MIDI Services, the MIDI 1.0 port names, without the parent device name, are used as the virtual Group Terminal Block names. The groups are mapped to the individual IO pins, starting at 0 (input and output both start at 0) and increasing monotonically for both input and output.

## Configuration

Endpoints for this transport are not created through the configuration file, but certain properties, such as the name and description, are updatable through it.

```json
"endpointTransportPluginSettings":
{
    "{26FA740D-469C-4D33-BEB1-3885DE7D6DF1}":
    {
        "_comment": "KSA MIDI (USB etc.)",
        "SWD: \\\\?\\SWD#MIDISRV#MIDIU_KSA_BIDI_6799286025327820155#{e7cce071-3c03-423f-88d3-f1045d02552b}":
        {
            
        }
    }
}
```

## Implementation

