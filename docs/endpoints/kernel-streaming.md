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
| Mnemonic | `KS` |

## Overview

Kernel Streaming is the mechanism through which most USB and other MIDI 1.0 drivers are used in Windows MIDI Services. In addition to USB MIDI 1.0, this provides access to many third-party drivers including BLE MIDI 1.0 and more.

In addition to MIDI 1.0 drivers, the Kernel Streaming transport is also the interface to the MIDI 2.0 class driver.

## Suggested Uses

Although this transport is primarily recommended for USB devices, it may be used for access to most other existing MIDI 1.0 devices on Windows.

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

