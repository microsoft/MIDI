---
layout: kb
title: About the MIDI 2.0 Loopback Transport
audience: everyone
description: All about the MIDI 2.0 loopback (app-to-app MIDI) transport
---

| Property | Value |
| -------- | ----- |
| Transport Id | `{942BF02D-93C0-4EA8-B03E-D51156CA75E1}` |
| Abbreviation | `LOOP` |

## Overview

A Virtual Loopback is a mechanism for two or more applications to communicate with each other over MIDI. 

## Suggested Uses

If you want to have loopback endpoints which are always available for routing between applications, and support MIDI 2.0 bidirectional communication, but do not want them to be controlled by the applications themselves, this is the right kind of endpoint to set up.

## Configuration

As with all configuration file changes, you must use the MIDI Settings app. Direct modification of the configuration file is not supported.

Here's an example configuration section for the Virtual Loopback MIDI 2.0 (bidirectional) endpoints.

```json

"endpointTransportPluginSettings":
{
    "{942BF02D-93C0-4EA8-B03E-D51156CA75E1}":
    {
        "_comment": "Loopback MIDI",

        "create":
        {
            "{0C1B3439-593F-4B7A-8950-4698D97B0897}":
            {
                "_comment" : "the above GUID is the unique association Id for this pair",
                "endpointA":
                {
                    "name": "Perm Loopback 1A",
                    "description": "This is a loopback I created in the configuration file",
                    "uniqueIdentifier": "3263827"
                },
                "endpointB":
                {
                    "name": "Perm Loopback 1B",
                    "description": "This is the b-side of the loopback I created in the configuration file",
                    "uniqueIdentifier": "3263827"
                }
            },
            "{B21B4973-3F85-48A0-8BA3-B35F44683D36}":
            {
                "_comment" : "the above GUID is the unique association Id for this pair",
                "endpointA":
                {
                    "name": "Perm Loopback 2A",
                    "description": "This is a loopback I created in the configuration file",
                    "uniqueIdentifier": "5150-1984"
                },
                "endpointB":
                {
                    "name": "Perm Loopback 2B",
                    "description": "This is the b-side of the loopback I created in the configuration file",
                    "uniqueIdentifier": "OU812"
                }
            }
        }
    }
}
```

Each loopback endpoint pair is identified by a GUID for the association id. The association GUID must be a valid unique GUID and shall not be an empty (all zeroes) GUID.

| Key | Description |
| -------- | ----- |
| (Association Id) | Unique GUID for this pair of endpoints. The GUID itself is the property key under the "create" node. |
| endpointA | Data for the first endpoint |
| endpointB | Data for the second endpoint |
| (endpoint) name | Required. This becomes the transport-supplied name for the loopback endpoint. |
| (endpoint) description | Optional. This becomes the transport-supplied description for the loopback endpoint. |
| (endpoint) uniqueId | Required. This is a short (32 characters or fewer) case-insensitive unique Id for the endpoint. When combined with the loopback A/B prefixes in the service, it must be unique across all loopback endpoints in Windows. You can use the same unique id for each endpoint in the same pair, but not the same as other pairs. |

# Implementation

Internally, the Virtual Loopback is implemented as two endpoints which are cross-wired, so anything sent out to Loopback A arrives on the input of Loopback B, and vice versa. Each declared pair has an exclusive relationship, and there's no practical limit to the number of loopback pairs you can define.
