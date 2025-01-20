---
layout: page
title: Config JSON for Transports
parent: Transport Types
grandparent: Windows MIDI Services
---

# JSON Config File

It's best to use the Settings application and the transport / processing plugins for Settings to manipulate the file. However, if you edit it by hand, here are some notes.

## The File location is Restricted

The JSON configuration files are all stored in `%allusersprofile%\Microsoft\MIDI` which typically resolves to `C:\ProgramData\Microsoft\MIDI`. For security reasons, we don't allow the file to be stored in any other location. However, you can have as many files in that folder as you want, and switch between them as needed.

The default config file is typically named `Default.midiconfig.json`. The actual name is stored in the registry under `HKLM\SOFTWARE\Microsoft\Windows MIDI Services` in the `CurrentConfig` value. This value must not contain any non-filename path characters (no backslashes, colons, etc.).

## JSON is Case-Sensitive

JSON is typically case-sensitive for all keys. The Windows.Data.Json parser used by Windows MIDI Services is case-sensitive with no option to ignore case. That includes GUID values. For example, the following two values are not equivalent JSON keys:

```json
"{26FA740D-469C-4D33-BEB1-3885DE7D6DF1}":
{
    "_comment": "KS MIDI (USB etc.)"
}
```
and
```json
"{26fa740d-469c-4d33-beb1-3885de7d6df1}":
{
    "_comment": "KS MIDI (USB etc.)"
}
```

## Schema

The JSON config file is such that each transport owns its own schema within the bucket associated with its class ID (GUID). We do not impose a schema on the transports or other plugins. Therefore there is no formal JSON Schema for this file. 

Here's an example of a bare-bones file, with sections for three different transports.

```json
{
    "header":
    {
        "_comment": "NOTE: All json keys are case-sensitive, including GUIDs.",
        "product" : "Windows MIDI Services",
        "fileVersion": 1.0
    },
    
    "endpointTransportPluginSettings":
    {
        "{26FA740D-469C-4D33-BEB1-3885DE7D6DF1}":
        {
            "_comment": "KS MIDI (USB etc.)"
        },
        "{C95DCD1F-CDE3-4C2D-913C-528CB8A4CBE6}":
        {
            "_comment": "Network MIDI"
        },
        "{8FEAAD91-70E1-4A19-997A-377720A719C1}":
        {
            "_comment": "Virtual MIDI"
        }
    },
    "endpointProcessingPluginSettings":
    {

    }
}
```

### Endpoint Properties

> NOTE: This section is in flux, as we're changing how devices are identified, and how properties are set.

Here's how the KS (USB using the new UMP driver) transport works as an example as it has the most complex lookup mechanisms to attempt to identify devices, even when they are moved from USB port to port.

```json
"{26FA740D-469C-4D33-BEB1-3885DE7D6DF1}":
{
    "_comment": "KS MIDI (USB etc.)",
    "update":
    [
        {
            "match":
            {
                "SWD": "\\\\?\\swd#midisrv#midiu_ks_bidi_6051189816177518400_outpin.0_inpin.2#{e7cce071-3c03-423f-88d3-f1045d02552b}"
            },
            "_comment" : "Roland A88 mk2",
            "userSuppliedName" : "Pete's A88",
            "userSuppliedDescription" : "The A88 is the giant MIDI 2.0 piano-action keyboard here in my studio."
        },
        {
            ...
        }
    ]   
},
```

> TODO: Include KSA endpoints and their generated group terminal blocks / groups.


Valid properties you can set:

| Property | Type | Description |
| -------- | ---- | ----------- |
| `userSuppliedName` | Quoted Text | The name you want to use for the endpoint. This will override the name displayed in correctly-coded applications, but won't necessarily change what you see in Device Manager. These names should be relatively short so they display fully in all/most applications, but meaningful to you. |
| `userSuppliedDescription` | Quoted Text | A text description and/or notes about the endpoint. Applications may or may not use this data |

## Plugin-specific settings

Appropriate configuration information may be found in the individual transport pages.
