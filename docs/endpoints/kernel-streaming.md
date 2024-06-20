---
layout: page
title: Kernel Streaming
parent: Transport Types
grandparent: Windows MIDI Services
has_children: false
---

# Kernel Streaming

| Property | Value |
| -------- | ----- |
| Abstraction Id | `{26FA740D-469C-4D33-BEB1-3885DE7D6DF1}` |
| Abbreviation | `KS` |

## Overview

Kernel Streaming is the mechanism through which MIDI 1.0 and MIDI 2.0 devices using the new UMP-based class driver are enumerated and communicated with.

In addition, any third-party UMP drivers will also route through this service.

## Compatibility

The best experience for Windows MIDI Services will be with applications using the new Windows MIDI Services SDK.

| API | Compatible | Notes |
| --- | ---------- | ----- |
| Windows MIDI Services Native | Yes | This provides the best experience for applications. |
| WinRT MIDI 1.0 | Yes | Data is translated between UMP and MIDI bytes (MIDI 1.0 data format) as required. |
| WinMM MIDI 1.0 | Yes | Data is translated between UMP and MIDI bytes (MIDI 1.0 data format) as required. |

> When creating endpoints for use by MIDI 1.0 applications which connect to MIDI 2.0 devices, we're limited to using the declared Group Terminal blocks and their associated groups, unless the device is MIDI 2.0 and the function blocks retrieved are marked as static function blocks. 

## Configuration

Endpoints for this transport are not created through the configuration file, but certain properties, such as the name and description, are updatable through it.

```json
"endpointTransportPluginSettings":
{
    "{26FA740D-469C-4D33-BEB1-3885DE7D6DF1}":
    {
        "_comment": "KS MIDI (USB etc.)",
        "SWD: \\\\?\\SWD#MIDISRV#MIDIU_KS_BIDI_6799286025327820155_OUTPIN.0_INPIN.2#{e7cce071-3c03-423f-88d3-f1045d02552b}":
        {
            "_comment" : "Native Instruments Kontrol S61",
            "userSuppliedName" : "Pete's Kontrol S61",
            "userSuppliedDescription" : "This is my Kontrol S61. There are many like it, but this one is mine. My Kontrol S61 is my best friend. It is my life. I must master it as I must master my life. Without me, my S61 is useless. Without my S61, I am useless."
        }
    }
}
```


## Implementation

