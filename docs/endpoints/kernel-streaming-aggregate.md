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
| Mnemonic | `KSA` |

## Overview

The Kernel Streaming Aggregator transport creates UMP endpoints for MIDI 1.0 devices using MIDI 1.0 byte format devices, like third-party MIDI 1.0 drivers.

## Suggested Uses

Although this transport is primarily recommended for USB devices, it may be used for access to most other existing MIDI 1.0 devices on Windows.

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

