// Copyright (c) Microsoft Corporation. All rights reserved.

#pragma once

#ifndef PAGE_SIZE
#define PAGE_SIZE 0x1000
#endif

// UMP 32 is 4 bytes
#define MINIMUM_UMP_DATASIZE 4

// UMP 128 is 16 bytes
#define MAXIMUM_UMP_DATASIZE 16

//
// Defining new interface categories
//
#define STRING_DEVINTERFACE_UNIVERSALMIDIPACKET_INPUT L"{AE174174-6396-4DEE-AC9E-1E9C6F403230}"
DEFINE_GUID(DEVINTERFACE_UNIVERSALMIDIPACKET_INPUT, 0xae174174, 0x6396, 0x4dee, 0xac, 0x9e, 0x1e, 0x9c, 0x6f, 0x40, 0x32, 0x30);

#define STRING_DEVINTERFACE_UNIVERSALMIDIPACKET_OUTPUT L"{3705DC2B-17A7-4452-98CE-BF12C6F48A0B}"
DEFINE_GUID(DEVINTERFACE_UNIVERSALMIDIPACKET_OUTPUT, 0x3705dc2b, 0x17a7, 0x4452, 0x98, 0xce, 0xbf, 0x12, 0xc6, 0xf4, 0x8a, 0xb);

#define STRING_DEVINTERFACE_UNIVERSALMIDIPACKET_BIDI L"{E7CCE071-3C03-423f-88D3-F1045D02552B}"
DEFINE_GUID(DEVINTERFACE_UNIVERSALMIDIPACKET_BIDI, 0xe7cce071, 0x3c03, 0x423f, 0x88, 0xd3, 0xf1, 0x4, 0x5d, 0x2, 0x55, 0x2b);

//
// Defining common midi interface properties
//

// NOTE: Becuase these use GUID/PID format, they cannot be used in Windows.Devices.Enumeration AQS filter strings in WinRT
// We should probably create a .prodesc schema per these links, if they can even be applied to devices
//     https://learn.microsoft.com/en-us/windows/win32/properties/building-property-handlers-property-schemas
// and https://learn.microsoft.com/en-us/windows/win32/properties/propdesc-schema-entry

#define STRING_PKEY_MIDI_AbstractionLayer L"{3F114A6A-11FA-4BD0-9D2C-6B7780CD80AD},1"
DEFINE_DEVPROPKEY(PKEY_MIDI_AbstractionLayer, 0x3f114a6a, 0x11fa, 0x4bd0, 0x9d, 0x2c, 0x6b, 0x77, 0x80, 0xcd, 0x80, 0xad, 1);     // DEVPROP_TYPE_GUID

#define STRING_PKEY_MIDI_AssociatedUMP L"{3F114A6A-11FA-4BD0-9D2C-6B7780CD80AD},2"
DEFINE_DEVPROPKEY(PKEY_MIDI_AssociatedUMP, 0x3f114a6a, 0x11fa, 0x4bd0, 0x9d, 0x2c, 0x6b, 0x77, 0x80, 0xcd, 0x80, 0xad, 2);     // DEVPROP_TYPE_UINT64

// Provided as a property for convenience. BLE, NET, USB, etc.
#define STRING_PKEY_MIDI_TransportMnemonic L"{3F114A6A-11FA-4BD0-9D2C-6B7780CD80AD},10"
DEFINE_DEVPROPKEY(PKEY_MIDI_TransportMnemonic, 0x3f114a6a, 0x11fa, 0x4bd0, 0x9d, 0x2c, 0x6b, 0x77, 0x80, 0xcd, 0x80, 0xad, 10);     // DEVPROP_TYPE_STRING

// true if this device is a standard MIDI 2loopback device or part of a loopback pair of devices
#define STRING_PKEY_MIDI_UmpLoopback L"{3F114A6A-11FA-4BD0-9D2C-6B7780CD80AD},20"
DEFINE_DEVPROPKEY(PKEY_MIDI_UmpLoopback, 0x3f114a6a, 0x11fa, 0x4bd0, 0x9d, 0x2c, 0x6b, 0x77, 0x80, 0xcd, 0x80, 0xad, 20);     // DEVPROP_TYPE_BOOLEAN


// The unique ID for the device. Not all transports supply this. Some do as ProductInstanceId. Some as iSerialNumber
#define STRING_PKEY_MIDI_UniqueIdentifier L"{3F114A6A-11FA-4BD0-9D2C-6B7780CD80AD},21"
DEFINE_DEVPROPKEY(PKEY_MIDI_UniqueIdentifier, 0x3f114a6a, 0x11fa, 0x4bd0, 0x9d, 0x2c, 0x6b, 0x77, 0x80, 0xcd, 0x80, 0xad, 21);     // DEVPROP_TYPE_STRING

// True if the device supports multi-client. Especially in the case of app-to-app MIDI, there are times when an
// endpoint should be exclusive to the app that creates/opens it. There may be other cases where we know a
// transport shouldn't use multi-client for endpoints (this is up for discussion in the Network MIDI 2.0 Working Group, for example)
#define STRING_PKEY_MIDI_SupportsMultiClient L"{3F114A6A-11FA-4BD0-9D2C-6B7780CD80AD},22"
DEFINE_DEVPROPKEY(PKEY_MIDI_SupportsMultiClient, 0x3f114a6a, 0x11fa, 0x4bd0, 0x9d, 0x2c, 0x6b, 0x77, 0x80, 0xcd, 0x80, 0xad, 22);     // DEVPROP_TYPE_BOOLEAN

// true if this device is the standard ping Bidi device
#define STRING_PKEY_MIDI_UmpPing L"{3F114A6A-11FA-4BD0-9D2C-6B7780CD80AD},23"
DEFINE_DEVPROPKEY(PKEY_MIDI_UmpPing, 0x3f114a6a, 0x11fa, 0x4bd0, 0x9d, 0x2c, 0x6b, 0x77, 0x80, 0xcd, 0x80, 0xad, 23);     // DEVPROP_TYPE_BOOLEAN

#define STRING_PKEY_MIDI_NativeDataFormat L"{3F114A6A-11FA-4BD0-9D2C-6B7780CD80AD},24"
DEFINE_DEVPROPKEY(PKEY_MIDI_NativeDataFormat, 0x3f114a6a, 0x11fa, 0x4bd0, 0x9d, 0x2c, 0x6b, 0x77, 0x80, 0xcd, 0x80, 0xad, 24);     // DEVPROP_TYPE_GUID


#define STRING_PKEY_MIDI_IN_GroupTerminalBlocks L"{3F114A6A-11FA-4BD0-9D2C-6B7780CD80AD},25"
DEFINE_DEVPROPKEY(PKEY_MIDI_IN_GroupTerminalBlocks, 0x3f114a6a, 0x11fa, 0x4bd0, 0x9d, 0x2c, 0x6b, 0x77, 0x80, 0xcd, 0x80, 0xad, 25);     // DEVPROP_TYPE_BINARY

#define STRING_PKEY_MIDI_OUT_GroupTerminalBlocks L"{3F114A6A-11FA-4BD0-9D2C-6B7780CD80AD},26"
DEFINE_DEVPROPKEY(PKEY_MIDI_OUT_GroupTerminalBlocks, 0x3f114a6a, 0x11fa, 0x4bd0, 0x9d, 0x2c, 0x6b, 0x77, 0x80, 0xcd, 0x80, 0xad, 26);     // DEVPROP_TYPE_BINARY


// In-protocol Endpoint information ==================================================================

#define STRING_PKEY_MIDI_EndpointSupportsMidi2Protocol L"{3F114A6A-11FA-4BD0-9D2C-6B7780CD80AD},50"
DEFINE_DEVPROPKEY(PKEY_MIDI_EndpointSupportsMidi2Protocol, 0x3f114a6a, 0x11fa, 0x4bd0, 0x9d, 0x2c, 0x6b, 0x77, 0x80, 0xcd, 0x80, 0xad, 50);     // DEVPROP_TYPE_BOOLEAN

#define STRING_PKEY_MIDI_EndpointSupportsMidi1Protocol L"{3F114A6A-11FA-4BD0-9D2C-6B7780CD80AD},51"
DEFINE_DEVPROPKEY(PKEY_MIDI_EndpointSupportsMidi1Protocol, 0x3f114a6a, 0x11fa, 0x4bd0, 0x9d, 0x2c, 0x6b, 0x77, 0x80, 0xcd, 0x80, 0xad, 51);     // DEVPROP_TYPE_BOOLEAN

#define STRING_PKEY_MIDI_EndpointSupportsReceivingJRTimestamps L"{3F114A6A-11FA-4BD0-9D2C-6B7780CD80AD},52"
DEFINE_DEVPROPKEY(PKEY_MIDI_EndpointSupportsReceivingJRTimestamps, 0x3f114a6a, 0x11fa, 0x4bd0, 0x9d, 0x2c, 0x6b, 0x77, 0x80, 0xcd, 0x80, 0xad, 52);     // DEVPROP_TYPE_BOOLEAN

#define STRING_PKEY_MIDI_EndpointSupportsSendingJRTimestamps L"{3F114A6A-11FA-4BD0-9D2C-6B7780CD80AD},53"
DEFINE_DEVPROPKEY(PKEY_MIDI_EndpointSupportsSendingJRTimestamps, 0x3f114a6a, 0x11fa, 0x4bd0, 0x9d, 0x2c, 0x6b, 0x77, 0x80, 0xcd, 0x80, 0xad, 53);     // DEVPROP_TYPE_BOOLEAN

#define STRING_PKEY_MIDI_EndpointUmpVersionMajor L"{3F114A6A-11FA-4BD0-9D2C-6B7780CD80AD},54"
DEFINE_DEVPROPKEY(PKEY_MIDI_EndpointUmpVersionMajor, 0x3f114a6a, 0x11fa, 0x4bd0, 0x9d, 0x2c, 0x6b, 0x77, 0x80, 0xcd, 0x80, 0xad, 54);     // DEVPROP_TYPE_BYTE

#define STRING_PKEY_MIDI_EndpointUmpVersionMinor L"{3F114A6A-11FA-4BD0-9D2C-6B7780CD80AD},55"
DEFINE_DEVPROPKEY(PKEY_MIDI_EndpointUmpVersionMinor, 0x3f114a6a, 0x11fa, 0x4bd0, 0x9d, 0x2c, 0x6b, 0x77, 0x80, 0xcd, 0x80, 0xad, 55);     // DEVPROP_TYPE_BYTE

// name provided by the endpoint through endpoint discovery
#define STRING_PKEY_MIDI_EndpointProvidedName L"{3F114A6A-11FA-4BD0-9D2C-6B7780CD80AD},56"
DEFINE_DEVPROPKEY(PKEY_MIDI_EndpointProvidedName, 0x3f114a6a, 0x11fa, 0x4bd0, 0x9d, 0x2c, 0x6b, 0x77, 0x80, 0xcd, 0x80, 0xad, 56);     // DEVPROP_TYPE_STRING

// Product instance Id provided by the endpoint through endpoint discovery
#define STRING_PKEY_MIDI_EndpointProvidedProductInstanceId L"{3F114A6A-11FA-4BD0-9D2C-6B7780CD80AD},57"
DEFINE_DEVPROPKEY(PKEY_MIDI_EndpointProvidedProductInstanceId, 0x3f114a6a, 0x11fa, 0x4bd0, 0x9d, 0x2c, 0x6b, 0x77, 0x80, 0xcd, 0x80, 0xad, 57);     // DEVPROP_TYPE_STRING


// full list of function blocks for this ump endpoint
#define STRING_PKEY_MIDI_FunctionBlocks L"{3F114A6A-11FA-4BD0-9D2C-6B7780CD80AD},58"
DEFINE_DEVPROPKEY(PKEY_MIDI_FunctionBlocks, 0x3f114a6a, 0x11fa, 0x4bd0, 0x9d, 0x2c, 0x6b, 0x77, 0x80, 0xcd, 0x80, 0xad, 58);     // DEVPROP_TYPE_BINARY

// true if function blocks are static
#define STRING_PKEY_MIDI_FunctionBlocksAreStatic L"{3F114A6A-11FA-4BD0-9D2C-6B7780CD80AD},59"
DEFINE_DEVPROPKEY(PKEY_MIDI_FunctionBlocksAreStatic, 0x3f114a6a, 0x11fa, 0x4bd0, 0x9d, 0x2c, 0x6b, 0x77, 0x80, 0xcd, 0x80, 0xad, 59);     // DEVPROP_TYPE_BOOLEAN

// full list of function blocks for this ump endpoint
#define STRING_PKEY_MIDI_DeviceIdentification L"{3F114A6A-11FA-4BD0-9D2C-6B7780CD80AD},60"
DEFINE_DEVPROPKEY(PKEY_MIDI_DeviceIdentification, 0x3f114a6a, 0x11fa, 0x4bd0, 0x9d, 0x2c, 0x6b, 0x77, 0x80, 0xcd, 0x80, 0xad, 60);     // DEVPROP_TYPE_BINARY




// Structures for properties ================================================================

// for PKEY_MIDI_DeviceIdentification
struct MidiDeviceIdentityProperty
{
    uint16_t Reserved0{ 0 };
    uint8_t Reserved1{ 0 };

    uint8_t ManufacturerSysExIdByte1{ 0 };
    uint8_t ManufacturerSysExIdByte2{ 0 };
    uint8_t ManufacturerSysExIdByte3{ 0 };

    uint8_t DeviceFamilyLsb{ 0 };
    uint8_t DeviceFamilyMsb{ 0 };

    uint8_t DeviceFamilyModelNumberLsb{ 0 };
    uint8_t DeviceFamilyModelNumberMsb{ 0 };

    uint8_t SoftwareRevisionLevelByte1{ 0 };
    uint8_t SoftwareRevisionLevelByte2{ 0 };
    uint8_t SoftwareRevisionLevelByte3{ 0 };
    uint8_t SoftwareRevisionLevelByte4{ 0 };
};

// TODO: double check this number
#define MIDI_FUNCTION_BLOCK_NAME_MAX_LENGTH 96

// for PKEY_MIDI_FunctionBlocks
// these properties are raw from the messages, with the exception of the name
// which is assembled from multiple in-protocol messages. Name is utf8 encoded.
struct MidiDevicePropertyFunctionBlock
{
    bool IsActive{ false };
    uint8_t BlockNumber{0};
    uint8_t Direction{0};
    uint8_t Midi1{0};
    uint8_t UIHint{0};
    uint8_t FirstGroup{0};
    uint8_t NumberOfGroupsSpanned{0};
    uint8_t MidiCIMessageVersionFormat{0};
    uint8_t MaxSysEx8Streams{0};

    // +1 because we zero-terminate the name even though it's fixed max length
    // most function blocks will have much shorter names. Should we instead have
    // a more complicated variable-length scheme here?
    char Name[MIDI_FUNCTION_BLOCK_NAME_MAX_LENGTH+1]{0};
};














#define HRESULT_FROM_RPCSTATUS(status) status == RPC_S_OK ? S_OK : MAKE_HRESULT(SEVERITY_ERROR, FACILITY_RPC, status)

#define SAFE_CLOSEHANDLE(h) if (h) { CloseHandle(h); h = NULL; }



