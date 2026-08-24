---
layout: kb
title: About the MIDI 1.0 Basic Loopback Transport
audience: everyone
description: All about the MIDI 1.0 basic loopback (MIDI 1.0 app-to-app MIDI) transport
---

| Property | Value |
| -------- | ----- |
| Transport Id | `{10088473-9478-4E62-850B-3D2315E135B8}` |
| Abbreviation | `BLOOP` |

## Overview

A Virtual Loopback is a mechanism for two or more applications to communicate with each other over MIDI. It can also be called a "bus", but we use Loopback on Windows due to its historical association with this feature from third-parties.

## Suggested Uses

If you want to have loopback endpoints which are always available for routing between applications, have a single name like existing MIDI 1.0 loopback drivers, but do not want them to be controlled by the applications themselves, this is the right kind of endpoint to set up.

## Configuration

As with all configuration file changes, you must use the MIDI Settings app. Direct modification of the configuration file is not supported.

Here's an example configuration section for the Virtual Basic Loopback MIDI 1.0 endpoints.

[Please read the article about the config file for rules about using or editing the config file](config-json.md)


```json

"endpointTransportPluginSettings":
{
    "{10088473-9478-4E62-850B-3D2315E135B8}":
    {
        "_comment": "Basic MIDI 1.0 Loopback MIDI",

        "create":
        {
            "{0C1B3439-593F-4B7A-8950-4698D97B0897}":
            {
                "_comment" : "the above GUID is the unique association Id for this loopback",
                "endpoint":
                {
                    "name": "Perm Loopback 1",
                    "description": "This is a loopback I created in the configuration file",
                    "uniqueIdentifier": "3263827"
                }
            },
            "{B21B4973-3F85-48A0-8BA3-B35F44683D36}":
            {
                "_comment" : "the above GUID is the unique association Id for this loopback",
                "endpoint":
                {
                    "name": "Perm Loopback 2",
                    "description": "This is a loopback I created in the configuration file",
                    "uniqueIdentifier": "5150-1984"
                }
            }
        }
    }
}
```

Each loopback endpoint is identified by a GUID for the association id. This was created to ensure there's a persistent identifier the app can use to add/remove the endpoint. The association GUID must be a valid unique GUID and shall not be an empty (all zeroes) GUID.

| Key | Description |
| -------- | ----- |
| (Association Id) | Unique GUID for this endpoint. The GUID itself is the property key under the "create" node. |
| endpoint | Data for the endpoint |
| (endpoint) name | Required. This becomes the transport-supplied name for the loopback endpoint. |
| (endpoint) description | Optional. This becomes the transport-supplied description for the loopback endpoint. |
| (endpoint) uniqueIdentifier | Required. This is a short (32 characters or fewer) case-insensitive unique Id for the endpoint. When combined with the basic loopback prefix in the service, it must be unique across all loopback endpoints in Windows. |

# Implementation

Internally, the Basic Loopback transport creates one endpoint that is wired out-to-in. Anything sent to the loopback Destination/Output port arrives on the Source/Input port of that same endpoint. There's no practical limit to the number of loopback endpoints you can define.
