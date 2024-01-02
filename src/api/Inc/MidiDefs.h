// Copyright (c) Microsoft Corporation. All rights reserved.

#pragma once

#include <Devpropdef.h>

#ifndef PAGE_SIZE
#define PAGE_SIZE 0x1000
#endif

// smallest UMP is 4 bytes, smallest bytestream is 1 byte (clock, etc.)
#define MINIMUM_LOOPED_DATASIZE 1

// largest UMP is 16 bytes
// we'll reuse that for the largest bytestream
#define MAXIMUM_LOOPED_DATASIZE 16


// we can't let the memory usage run away. This many messages is a 
// lot, and performance will suffer above 5000 or so. This number is
// used by the scheduler and then also by the client API
#define MIDI_OUTGOING_MESSAGE_QUEUE_MAX_MESSAGE_COUNT 10000


//
// Registry keys for global configuration. The settings app can write to some of these, so including in MidiDefs
//
#define MIDI_ROOT_REG_KEY L"Software\\Microsoft\\Windows MIDI Services"
#define MIDI_ROOT_TRANSPORT_PLUGINS_REG_KEY MIDI_ROOT_REG_KEY L"\\Transport Plugins"
#define MIDI_ROOT_ENDPOINT_PROCESSING_PLUGINS_REG_KEY MIDI_ROOT_REG_KEY L"\\Message Processing Plugins"
#define MIDI_PLUGIN_ENABLED_REG_VALUE L"Enabled"
#define MIDI_PLUGIN_CLSID_REG_VALUE L"CLSID"

#define MIDI_CONFIG_FILE_REG_VALUE L"CurrentConfig"

// we force this root so the service can't be told to open some other random file on the system
// note that this is a restricted folder. The installer has to create this folder for us and
// give rights to the users in the system so the service *and* the setup applications can 
// read/write there.
#define MIDI_CONFIG_FILE_FOLDER L"%ALLUSERSPROFILE%\\Microsoft\\MIDI\\"

#define MIDI_CONFIG_JSON_HEADER_OBJECT L"header"
#define MIDI_CONFIG_JSON_TRANSPORT_PLUGIN_SETTINGS_OBJECT L"endpointTransportPluginSettings"
#define MIDI_CONFIG_JSON_ENDPOINT_PROCESSING_PLUGIN_SETTINGS_OBJECT L"endpointProcessingPluginSettings"


//
// SendMidiMessage HRESULT codes (these are not exposed through the API, and are just internal)
//

#define HR_S_MIDI_SENDMSG_IMMEDIATE                 MAKE_HRESULT(SEVERITY_SUCCESS, FACILITY_WINDOWS, 0x601)
#define HR_S_MIDI_SENDMSG_SCHEDULED                 MAKE_HRESULT(SEVERITY_SUCCESS, FACILITY_WINDOWS, 0x602)
#define HR_S_MIDI_SENDMSG_SYSEX_PARKED              MAKE_HRESULT(SEVERITY_SUCCESS, FACILITY_WINDOWS, 0x603)

#define HR_E_MIDI_SENDMSG_BUFFER_FULL               MAKE_HRESULT(SEVERITY_ERROR,   FACILITY_WINDOWS, 0x620)
#define HR_E_MIDI_SENDMSG_SCHEDULER_QUEUE_FULL      MAKE_HRESULT(SEVERITY_ERROR,   FACILITY_WINDOWS, 0x621)
#define HR_E_MIDI_SENDMSG_INVALID_MESSAGE           MAKE_HRESULT(SEVERITY_ERROR,   FACILITY_WINDOWS, 0x622)
// the SendMidiMessage function can also return E_FAIL


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

// NOTE: Because these use GUID/PID format, they cannot be used in Windows.Devices.Enumeration AQS filter strings in WinRT
// We should probably create a .prodesc schema per these links, if they can even be applied to devices
//     https://learn.microsoft.com/en-us/windows/win32/properties/building-property-handlers-property-schemas
// and https://learn.microsoft.com/en-us/windows/win32/properties/propdesc-schema-entry

#define MIDI_STRING_PKEY_GUID L"{3F114A6A-11FA-4BD0-9D2C-6B7780CD80AD}"
#define MIDI_STRING_PKEY_PID_SEPARATOR L" "
#define DEFINE_MIDIDEVPROPKEY(k, i) DEFINE_DEVPROPKEY(k, 0x3f114a6a, 0x11fa, 0x4bd0, 0x9d, 0x2c, 0x6b, 0x77, 0x80, 0xcd, 0x80, 0xad, i)

// The single space before the index is important for these to 
// match the keys returned from Windows.Devices.Enumeration in the client API

// required so MidiSrv knows which abstraction layer to call into for any given endpoint
#define STRING_PKEY_MIDI_AbstractionLayer MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"1"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_AbstractionLayer, 1); // DEVPROP_TYPE_GUID

// Provided as a property for convenience. BLE, NET, USB, etc.
#define STRING_PKEY_MIDI_TransportMnemonic MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"2"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_TransportMnemonic, 2);     // DEVPROP_TYPE_STRING

// Match MidiDataFormat defined in MidiDeviceManagerInterface.idl
// TODO: Can these be converged to use the same enum?
#define MIDI_PROP_NATIVEDATAFORMAT_BYTESTREAM (uint8_t)0x01
#define MIDI_PROP_NATIVEDATAFORMAT_UMP (uint8_t)0x02

// The data format that the connected device uses to talk to the abstraction layer
// For a MIDI 1 device, it is MIDI_NATIVEDATAFORMAT_BYTESTREAM
// For a MIDI 2 device, it is MIDI_NATIVEDATAFORMAT_UMP
#define STRING_PKEY_MIDI_NativeDataFormat MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"3"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_NativeDataFormat, 3);     // DEVPROP_TYPE_BYTE uint8_t


// True if the device supports multi-client. Especially in the case of app-to-app MIDI, there are times when an
// endpoint should be exclusive to the app that creates/opens it. There may be other cases where we know a
// transport shouldn't use multi-client for endpoints (this is up for discussion in the Network MIDI 2.0 Working Group, for example)
#define STRING_PKEY_MIDI_SupportsMulticlient MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"5"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_SupportsMulticlient, 5);     // DEVPROP_TYPE_BOOLEAN

// A bitmask of the data format(s) the abstraction layer can use to talk to the system
// For a MIDI 1 device, it can support MIDI_NATIVEDATAFORMAT_UMP or MIDI_NATIVEDATAFORMAT_BYTESTREAM
// For a MIDI 2 device, it will support MIDI_NATIVEDATAFORMAT_UMP
#define STRING_PKEY_MIDI_SupportedDataFormats MIDI_STRING_PKEY_GUID L",6"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_SupportedDataFormats, 6);     // DEVPROP_TYPE_BYTE uint8_t

#define STRING_PKEY_MIDI_ManufacturerName MIDI_STRING_PKEY_GUID L",7"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_ManufacturerName, 7);     // DEVPROP_TYPE_STRING




// this is the device-supplied name in the case of device-based transports
// we have a copy here because we may rewrite FriendlyName
#define STRING_PKEY_MIDI_TransportSuppliedEndpointName MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"14"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_TransportSuppliedEndpointName, 14);     // DEVPROP_TYPE_STRING

// USB / KS Properties ============================================================================
// Starts at 50

// TODO: May need to combine these into one, depending on what comes back from the driver
#define STRING_PKEY_MIDI_IN_GroupTerminalBlocks MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"50"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_IN_GroupTerminalBlocks, 50);     // DEVPROP_TYPE_BINARY

#define STRING_PKEY_MIDI_OUT_GroupTerminalBlocks MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"51"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_OUT_GroupTerminalBlocks, 51);     // DEVPROP_TYPE_BINARY

#define STRING_PKEY_MIDI_AssociatedUMP MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"52"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_AssociatedUMP, 52);     // DEVPROP_TYPE_UINT64

// iSerialNumber Some as iSerialNumber
#define STRING_PKEY_MIDI_SerialNumber MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"53"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_SerialNumber, 53);     // DEVPROP_TYPE_STRING


// Major Known Endpoint Types =====================================================================
// Starts at 100

// If you change these, you should also update
// the WinRT MidiEndpointDevicePurpose enum, which is projected through the API
enum MidiEndpointDevicePurposePropertyValue
{
    NormalMessageEndpoint = 0,
    VirtualDeviceResponder = 100,
    InBoxGeneralMidiSynth = 400,
    DiagnosticLoopback = 500,
    DiagnosticPing = 510,
};

// Valid values are in MidiEndpointDevicePurposePropertyValue
#define STRING_PKEY_MIDI_EndpointDevicePurpose MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"100"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_EndpointDevicePurpose, 100);     // DEVPROP_TYPE_ uint32

// In-protocol Endpoint information ================================================================
// Starts at 150

#define STRING_PKEY_MIDI_EndpointSupportsMidi2Protocol MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"150"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_EndpointSupportsMidi2Protocol, 150);     // DEVPROP_TYPE_BOOLEAN

#define STRING_PKEY_MIDI_EndpointSupportsMidi1Protocol MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"151"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_EndpointSupportsMidi1Protocol, 151);     // DEVPROP_TYPE_BOOLEAN

#define STRING_PKEY_MIDI_EndpointSupportsReceivingJRTimestamps MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"152"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_EndpointSupportsReceivingJRTimestamps, 152);     // DEVPROP_TYPE_BOOLEAN

#define STRING_PKEY_MIDI_EndpointSupportsSendingJRTimestamps MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"153"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_EndpointSupportsSendingJRTimestamps, 153);     // DEVPROP_TYPE_BOOLEAN

#define STRING_PKEY_MIDI_EndpointUmpVersionMajor MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"154"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_EndpointUmpVersionMajor, 154);     // DEVPROP_TYPE_BYTE

#define STRING_PKEY_MIDI_EndpointUmpVersionMinor MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"155"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_EndpointUmpVersionMinor, 155);     // DEVPROP_TYPE_BYTE

// name provided by the endpoint through endpoint discovery
// Note that it is supplied by protocol as utf8, and we need to convert to unicode
#define STRING_PKEY_MIDI_EndpointProvidedName MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"156"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_EndpointProvidedName, 156);     // DEVPROP_TYPE_STRING

// Product instance Id provided by the endpoint through endpoint discovery
// Note that it is supplied by protocol as utf8, and we need to convert to unicode
#define STRING_PKEY_MIDI_EndpointProvidedProductInstanceId MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"157"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_EndpointProvidedProductInstanceId, 157);     // DEVPROP_TYPE_STRING

// full list of function blocks for this ump endpoint
#define STRING_PKEY_MIDI_FunctionBlocks MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"158"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_FunctionBlocks, 158);     // DEVPROP_TYPE_BINARY

// true if function blocks are static
#define STRING_PKEY_MIDI_FunctionBlocksAreStatic MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"159"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_FunctionBlocksAreStatic, 159);     // DEVPROP_TYPE_BOOLEAN

// binary device information structure holding sysex id etc.
#define STRING_PKEY_MIDI_DeviceIdentification MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"160"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_DeviceIdentification, 160);     // DEVPROP_TYPE_BINARY

#define MIDI_PROP_CONFIGURED_PROTOCOL_MIDI1 (uint8_t)0x01
#define MIDI_PROP_CONFIGURED_PROTOCOL_MIDI2 (uint8_t)0x02

#define STRING_PKEY_MIDI_EndpointConfiguredProtocol MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"161"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_EndpointConfiguredProtocol, 161);     // DEVPROP_TYPE_BYTE




// User-supplied metadata ==================================================================
// Starts at 500

#define STRING_PKEY_MIDI_UserSuppliedEndpointName MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"500"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_UserSuppliedEndpointName, 500);     // DEVPROP_TYPE_STRING

// large image of most any size. Mostly used just by the settings app
#define STRING_PKEY_MIDI_UserSuppliedLargeImagePath MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"501"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_UserSuppliedLargeImagePath, 501);     // DEVPROP_TYPE_STRING

// 32x32 image
#define STRING_PKEY_MIDI_UserSuppliedSmallImagePath MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"502"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_UserSuppliedSmallImagePath, 502);     // DEVPROP_TYPE_STRING


#define STRING_PKEY_MIDI_UserSuppliedDescription MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"503"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_UserSuppliedDescription, 503);     // DEVPROP_TYPE_STRING



// Additional metrics ==================================================================
// Starts at 600

// Calculated latency for sending a message to a device. 
// We may be able to calculate this using the jitter-reduction timestamps.
#define STRING_PKEY_MIDI_MidiOutCalculatedLatencyTicks MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"600"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_MidiOutCalculatedLatencyTicks, 600);     // DEVPROP_TYPE_UINT64

// user-supplied latency ticks for a device
#define STRING_PKEY_MIDI_MidiOutUserSuppliedLatencyTicks MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"601"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_MidiOutUserSuppliedLatencyTicks, 601);     // DEVPROP_TYPE_UINT64

// true if we should use the user-supplied latency instead of the calculated latency
#define STRING_PKEY_MIDI_MidiOutLatencyTicksUserOverride MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"602"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_MidiOutLatencyTicksUserOverride, 602);     // DEVPROP_TYPE_BOOL


// TODO: Add in the other properties like
// - Should receive MIDI clock
// - Should use MIDI clock start
// - Should use MIDI timecode
// - Recommended CC Automation Interval (ms)
// - Supports MPE
// - Requires note off translation (Mackie)
// - 





// Structures for properties ================================================================





// GTB structures Copied from driver code. This should be a shared file instead.

// Group Terminal Block structure defines
#pragma pack(push)
#pragma pack(1)

/* Based on current definitions in USB Class Definition for MIDI Devices Release 2.0

For Group Terminal Block Type (sect. A.6)
BIDIRECTIONAL                       0x00
INPUT_ONLY                          0x01
OUTPUT_ONLY                         0x02

For Group Terminal Default MIDI Protocol (sect. A.7)
USE_MIDI_CI                         0x00
MIDI_1_0_UP_TO_64_BITS              0x01
MIDI_1_0_UP_TO_64_BITS_AND_JRTS     0x02
MIDI_1_0_UP_TO_128_BITS             0x03
MIDI_1_0_UP_TO_128_BITS_AND_JRTS    0x04
MIDI_2_0                            0x11
MIDI_2_0_AND_JRTS                   0x12

Group Terminal Number
GROUP_1                             0x00
GROUP_2                             0x01
GROUP_3                             0x02
...
GROUP_16                            0x0F
*/

typedef struct
{
    WORD                            Size;
    BYTE                            Number;             // Group Terminal Block ID
    BYTE                            Direction;          // Group Terminal Block Type
    BYTE                            FirstGroupIndex;    // The first group in this block
    BYTE                            GroupCount;         // The number of groups spanned
    BYTE                            Protocol;           // The MIDI Protocol
    WORD                            MaxInputBandwidth;  ///< Maximum Input Bandwidth Capability in 4KB/second
    WORD                            MaxOutputBandwidth; ///< Maximum Output Bandwidth Capability in 4KB/second
} UMP_GROUP_TERMINAL_BLOCK_HEADER;



typedef struct
{
    UMP_GROUP_TERMINAL_BLOCK_HEADER GrpTrmBlock;
    WCHAR                           Name[1];             // NULL Terminated string, blank indicates none available
    // from USB Device
} UMP_GROUP_TERMINAL_BLOCK_DEFINITION, * PUMP_GROUP_TERMINAL_BLOCK_DEFINITION;


#pragma pack(pop)



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
    uint8_t BlockNumber{ 0 };
    uint8_t Reserved0{ 0 };         // unused in UMP 1.1
    uint8_t Direction{ 0 };
    uint8_t Midi1{ 0 };
    uint8_t UIHint{ 0 };
    uint8_t FirstGroup{ 0 };
    uint8_t NumberOfGroupsSpanned{ 0 };
    uint8_t MidiCIMessageVersionFormat{ 0 };
    uint8_t MaxSysEx8Streams{ 0 };

    uint32_t Reserved1{ 0 };        // unused in UMP 1.1
    uint32_t Reserved2{ 0 };        // unused in UMP 1.1

    // +1 because we zero-terminate the name even though it's fixed max length.
    // most function blocks will have much shorter names. Should we instead have
    // a more complicated variable-length scheme here?
    char Name[MIDI_FUNCTION_BLOCK_NAME_MAX_LENGTH+1]{0};
};




#define HRESULT_FROM_RPCSTATUS(status) status == RPC_S_OK ? S_OK : MAKE_HRESULT(SEVERITY_ERROR, FACILITY_RPC, status)

#define SAFE_CLOSEHANDLE(h) if (h) { CloseHandle(h); h = NULL; }


// note that this produces a GUID with uppercase letters and enclosing braces
inline std::wstring GuidToString(_In_ GUID guid)
{
    LPOLESTR str;
    if (SUCCEEDED(StringFromCLSID(guid, &str)))
    {

        // TODO: Is this copying or acquiring?
        std::wstring guidString{ str };

        ::CoTaskMemFree(str);

        return guidString;
    }
    else
    {
        return L"";
    }
}

