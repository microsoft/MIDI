# Endpoint Device Ids

The Endpoint Device Id (also referred to as a Device Id) is the way we identify individual devices and interfaces in Windows. 

Example for one of the built-in loopback endpoints: 
`\\?\SWD#MIDISRV#MIDIU_LOOPBACK_A#{e7cce071-3c03-423f-88d3-f1045d02552b}`

| Part | Description |
| ----- | -- |
| SWD | Software device |
| MIDISRV | The name of the enumerator. For Windows MIDI Services, this is the MidiSrv Windows Service |
| MIDIU_LOOPBACK_A | Arbitrary unique identification string provided by the transport. Typically includes a unique identifier. |
| GUID | The interface Id. For Windows MIDI Services, every interface is a bidirectional interface, even if the connected device is MIDI 1.0 with a single unidirectional interface. For MIDI 1.0 devices, you can look at the group terminal blocks to identify active groups/directions. For MIDI 2.0 devices, you can look at the function blocks for the same information and more.|

If you look at the device in Device Manager, and look at Details/Device Instance Path, you'll see all of the information here except for the interface Id. When you enumerate devices through Windows::Devices::Enumeration, the interface Id is included and required.

> Tip: Although it was required in the past, we don't recommend parsing these strings. If there's information you need about the device which is not contained in the enumerated properties, please let us know and we'll look into whether or not we can create a custom property to hold that.
