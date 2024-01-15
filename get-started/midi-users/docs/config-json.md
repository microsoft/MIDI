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
        },
```
and
```json
"{26fa740d-469c-4d33-beb1-3885de7d6df1}":
        {
            "_comment": "KS MIDI (USB etc.)"
        },
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

The basics of this are identical for each transport. We'll use KS (USB) as an example

```json
"{26FA740D-469C-4D33-BEB1-3885DE7D6DF1}":
{
    "_comment": "KS MIDI (USB etc.)"
    "SWD: \\\\?\\SWD#MIDISRV#MIDIU_KS_BIDI_6799286025327820155_OUTPIN.0_INPIN.2#{e7cce071-3c03-423f-88d3-f1045d02552b}":
    {
        "userSuppliedName" : "Pete's Kontrol S61",
        "userSuppliedDescription" : "This is my most favorite MIDI 2.0 controller in the whole world!"
    }
    ...
},
```

Of those, the identification method `SWD` is the most important. This controls how we identify a matching device. In cases where the manufacturer doesn't supply a unique iSerialNumber in USB, unplugging your device from one USB port and plugging it into another can result in a new Id. Similarly, if you have two or more of the same device, and they do not have unique serial numbers, it can be impossible for Windows to distinguish between them.

Valid values for the identification method prefix

- `SWD: ` : (The colon and trailing space are required). Use the full Windows Endpoint Device Interface Id. For example `\\\\?\\SWD#MIDISRV#MIDIU_KS_BIDI_16024944077548273316_OUTPIN.0_INPIN.2#{e7cce071-3c03-423f-88d3-f1045d02552b}`. (Note how the backslashes have to be escaped with additional backslashes.) If the device has an `iSerialNumber` or you never move it between USB ports, this tends to work fine.
- (other methods to be added here when we implement them)

Valid properties you can set across all supported endpoints

| Property | Type | Description |
| -------- | ---- | ----------- |
| `userSuppliedName` | Quoted Text | The name you want to use for the endpoint. This will override the name displayed in correctly-coded applications, but won't necessarily change what you see in Device Manager. These names should be relatively short so they display fully in all/most applications, but meaningful to you. |
| `userSuppliedDescription` | Quoted Text | A text description and/or notes about the endpoint. Applications may or may not use this data |
| `forceSingleClientOnly` | Boolean true/false (no quotes) | Most endpoints are multi-client (more than one applicatation can use them simultaneously) by default. This is for forcing an endpoint to be single-client only (true). It's unusual to need this, but a typical use may be to disable multi-client for a device which has a custom driver which doesn't gracefully handle multiple client applications at the same time. |

## Plugin-specific settings

This is not an exhaustive list, because the transport and processing plugins may be created by anyone.

### Virtual MIDI 

Virtual MIDI includes three different sections inside its transport bucket.

```json
"{8FEAAD91-70E1-4A19-997A-377720A719C1}":
{
    "_comment": "Virtual MIDI",
    "add":
    {
    },
    "update":
    {
    },
    "remove":
    {
       
    }

}
```

For the persistent configuration file, typically "add" is all that is specified, as it doesn't make sense to update or remove endpoints or routing on service start.

> NOTE: This document is not yet complete. We'll add more details as the schemas are finalized

### KS (USB etc) MIDI

TODO: Show how to update endpoint names and provide other properties here
