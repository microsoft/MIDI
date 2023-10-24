# JSON Config File


It's best to use the Settings application and the transport / processing plugins for Settings to manipulate the file. However, if you edit it by hand, here are some notes.

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

## The File location is Restricted

The JSON configuration files are all stored in `%allusersprofile%\Microsoft\MIDI` which typically resolves to `C:\ProgramData\Microsoft\MIDI`. For security reasons, we don't allow the file to be stored in any other location. However, you can have as many files in that folder as you want, and switch between them as needed.

The default config file is typically named `Default.midiconfig.json`. The actual name is stored in the registry under `HKLM\SOFTWARE\Microsoft\Windows MIDI Services` in the `CurrentConfig` value. This value must not contain any non-filename path characters (no backslashes, colons, etc.).

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
