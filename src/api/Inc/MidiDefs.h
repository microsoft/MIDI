// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <Devpropdef.h>


#define MIDI_TRACE_EVENT_ERROR                      "Midi.Error"
#define MIDI_TRACE_EVENT_WARNING                    "Midi.Warning"
#define MIDI_TRACE_EVENT_INFO                       "Midi.Info"
#define MIDI_TRACE_EVENT_VERBOSE                    "Midi.Verbose"
#define MIDI_TRACE_EVENT_METRICS                    "Midi.Metrics"


#define MIDI_TRACE_EVENT_MESSAGE_FIELD              "Message"
#define MIDI_TRACE_EVENT_LOCATION_FIELD             "Location"
#define MIDI_TRACE_EVENT_HRESULT_FIELD              "HResult"
#define MIDI_TRACE_EVENT_INTERFACE_FIELD            "Interface"

#define MIDI_TRACE_EVENT_MESSAGE_TIMESTAMP_FIELD    "Midi Timestamp"
#define MIDI_TRACE_EVENT_MESSAGE_DATA_FIELD         "Midi Data"
#define MIDI_TRACE_EVENT_MIDI_WORD0_FIELD           "Midi Word 0"


#define MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD        "Endpoint Device Interface Id (SWD)"
#define MIDI_TRACE_EVENT_DEVICE_INSTANCE_ID_FIELD   "Device Instance Id"




#ifndef PAGE_SIZE
#define PAGE_SIZE 0x1000
#endif

// smallest UMP is 4 bytes, smallest bytestream is 1 byte (clock, etc.)
#define MINIMUM_LOOPED_DATASIZE 1

// largest UMP is 16 bytes
#define MAXIMUM_LOOPED_UMP_DATASIZE 16

// Libmidi2 chunks UMP to bytestream sysex messages with a max size of 20 bytes
// per message
#define MAXIMUM_LIBMIDI2_BYTESTREAM_DATASIZE 20

// largest supported bytestream is 2048
// TODO: revisit this, possibly have wdmaud2 chunk down large sysex messages to
// something more manageable
#define MAXIMUM_LOOPED_BYTESTREAM_DATASIZE 2048

#define MIDI_TIMESTAMP_SEND_IMMEDIATELY 0

// we can't let the memory usage run away. This many messages is a 
// lot, and performance will suffer above 5000 or so. This number is
// used by the scheduler and then also by the client API
#define MIDI_OUTGOING_MESSAGE_QUEUE_MAX_MESSAGE_COUNT 10000


#define MIDI_PROTOCOL_MANAGER_ENDPOINT_CREATION_CONTEXT (LONGLONG)3263827


//
// Registry keys for global configuration. The settings app can write to some of these, so including in MidiDefs
// Everything is under HKLM
//
#define MIDI_ROOT_REG_KEY                               L"Software\\Microsoft\\Windows MIDI Services"

// DWORD value. 0 to not use MMCSS. > 0 to use it. This is used in the midixproc project
// HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows MIDI Services\UseMMCSS (DWORD)
#define MIDI_USE_MMCSS_REG_VALUE                        L"UseMMCSS"
#define MIDI_USE_MMCSS_REG_DEFAULT_VALUE                0x00000000

#define MIDI_CONFIG_FILE_REG_VALUE                      L"CurrentConfig"



#define MIDI_ROOT_ENDPOINT_APP_SDK_REG_KEY              MIDI_ROOT_REG_KEY L"\\Desktop App SDK Runtime"
#define MIDI_APP_SDK_ARM64_REG_VALUE                    L"Arm64"
#define MIDI_APP_SDK_ARM64EC_REG_VALUE                  L"Arm64EC"
#define MIDI_APP_SDK_X64_REG_VALUE                      L"x64"


#define MIDI_ROOT_TRANSPORT_PLUGINS_REG_KEY             MIDI_ROOT_REG_KEY L"\\Transport Plugins"
#define MIDI_ROOT_ENDPOINT_PROCESSING_PLUGINS_REG_KEY   MIDI_ROOT_REG_KEY L"\\Message Processing Plugins"
#define MIDI_PLUGIN_ENABLED_REG_VALUE                   L"Enabled"
#define MIDI_PLUGIN_CLSID_REG_VALUE                     L"CLSID"



// we force this root so the service can't be told to open some other random file on the system
// note that this is a restricted folder. The installer has to create this folder for us and
// give rights to the users in the system so the service *and* the setup applications can 
// read/write there.
#define MIDI_CONFIG_FILE_FOLDER L"%ALLUSERSPROFILE%\\Microsoft\\MIDI\\"

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
#define STRING_PKEY_MIDI_TransportCode MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"2"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_TransportCode, 2);     // DEVPROP_TYPE_STRING

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
// NOTE: These are actually in the MidiDataFormat enum, slightly different than the defines mentioned above.
#define STRING_PKEY_MIDI_SupportedDataFormats MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"6"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_SupportedDataFormats, 6);     // DEVPROP_TYPE_UINT32

#define STRING_PKEY_MIDI_ManufacturerName MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"7"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_ManufacturerName, 7);     // DEVPROP_TYPE_STRING

#define STRING_PKEY_MIDI_GenerateIncomingTimestamp MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"10"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_GenerateIncomingTimestamp, 10);     // DEVPROP_TYPE_STRING



// this is the device-supplied name in the case of device-based transports
// we have a copy here because we may rewrite FriendlyName
#define STRING_PKEY_MIDI_TransportSuppliedEndpointName MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"14"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_TransportSuppliedEndpointName, 14);     // DEVPROP_TYPE_STRING

#define STRING_PKEY_MIDI_TransportSuppliedDescription MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"15"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_TransportSuppliedDescription, 15);     // DEVPROP_TYPE_STRING

#define STRING_PKEY_MIDI_UserSuppliedPortNumber MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"16"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_UserSuppliedPortNumber, 16);     // DEVPROP_TYPE_UINT32

#define STRING_PKEY_MIDI_ServiceAssignedPortNumber MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"17"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_ServiceAssignedPortNumber, 17);     // DEVPROP_TYPE_UINT32

#define STRING_PKEY_MIDI_PortAssignedGroupIndex MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"18"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_PortAssignedGroupIndex, 18);     // DEVPROP_TYPE_

// USB / KS Properties ============================================================================
// Starts at 50

// TODO: May need to combine these into one, depending on what comes back from the driver
#define STRING_PKEY_MIDI_IN_GroupTerminalBlocks MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"50"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_IN_GroupTerminalBlocks, 50);     // DEVPROP_TYPE_BINARY

#define STRING_PKEY_MIDI_OUT_GroupTerminalBlocks MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"51"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_OUT_GroupTerminalBlocks, 51);     // DEVPROP_TYPE_BINARY

#define STRING_PKEY_MIDI_AssociatedUMP MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"52"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_AssociatedUMP, 52);     // DEVPROP_TYPE_STRING

// iSerialNumber for USB, but can be supplied by other endpoints as a config value or a value read from advertising
#define STRING_PKEY_MIDI_SerialNumber MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"53"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_SerialNumber, 53);     // DEVPROP_TYPE_STRING

// idVendor from the USB device. Used in some field-based lookups
#define STRING_PKEY_MIDI_UsbVID MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"54"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_UsbVID, 54);     // DEVPROP_TYPE_UINT16

// idProduct from the USB device. Used in some field-based lookups
#define STRING_PKEY_MIDI_UsbPID MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"55"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_UsbPID, 55);     // DEVPROP_TYPE_UINT16



// Major Known Endpoint Types =====================================================================
// Starts at 100

// Valid values are in MidiEndpointDevicePurposePropertyValue
#define STRING_PKEY_MIDI_EndpointDevicePurpose MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"100"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_EndpointDevicePurpose, 100);     // DEVPROP_TYPE_ uint32



// if this is true or missing, we add an endpoint metadata listener
//#define STRING_PKEY_MIDI_EndpointRequiresMetadataHandler MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"120"
//DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_EndpointRequiresMetadataHandler, 120);     // DEVPROP_TYPE_BOOLEAN



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

#define STRING_PKEY_MIDI_EndpointInformationLastUpdateTime MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"156"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_EndpointInformationLastUpdateTime, 156);     // DEVPROP_TYPE_FILETIME



// binary device information structure holding sysex id, device family id, sw revision, etc.
#define STRING_PKEY_MIDI_DeviceIdentity MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"160"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_DeviceIdentity, 160);     // DEVPROP_TYPE_BINARY

#define STRING_PKEY_MIDI_DeviceIdentityLastUpdateTime MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"161"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_DeviceIdentityLastUpdateTime, 161);     // DEVPROP_TYPE_FILETIME


// In-protocol endpoint strings ================================================================
// Starts at 170

// name provided by the endpoint through endpoint discovery
// Note that it is supplied by protocol as utf8, and we need to convert to unicode
#define STRING_PKEY_MIDI_EndpointProvidedName MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"170"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_EndpointProvidedName, 170);     // DEVPROP_TYPE_STRING

// Product instance Id provided by the endpoint through endpoint discovery
// Note that it is supplied by protocol as utf8, and we need to convert to unicode
#define STRING_PKEY_MIDI_EndpointProvidedProductInstanceId MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"171"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_EndpointProvidedProductInstanceId, 171);     // DEVPROP_TYPE_STRING

#define STRING_PKEY_MIDI_EndpointProvidedNameLastUpdateTime MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"172"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_EndpointProvidedNameLastUpdateTime, 172);     // DEVPROP_TYPE_FILETIME

#define STRING_PKEY_MIDI_EndpointProvidedProductInstanceIdLastUpdateTime MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"173"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_EndpointProvidedProductInstanceIdLastUpdateTime, 173);     // DEVPROP_TYPE_FILETIME



// In-protocol Function Block information ================================================================
// starts at 180

// value provided during endpoint discovery. Need this to know when we receive all function block data
#define STRING_PKEY_MIDI_FunctionBlockCount MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"180"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_FunctionBlockCount, 180);     // DEVPROP_TYPE_BYTE

// true if function blocks are static
#define STRING_PKEY_MIDI_FunctionBlocksAreStatic MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"182"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_FunctionBlocksAreStatic, 182);     // DEVPROP_TYPE_BOOLEAN



#define STRING_PKEY_MIDI_FunctionBlocksLastUpdateTime MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"183"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_FunctionBlocksLastUpdateTime, 183);     // DEVPROP_TYPE_FILETIME

// Actual Function Blocks ================================================================
// starts at 200
// 
// Went back and forth on storing these similar to how GTBs are stored vs discrete properties.
// in the end, the retrieval method of the two dictates the best way. It looks cumbersome here, but
// it works out in the end.
// 
// We store this way to make property change notifications make more sense, especially given that
// a consumer of the API can ask for an update of a single function block, or all function blocks. We
// have to take a runtime hit and retrieve the existing ones first, update, and then rewrite, or
// we queue updates for a few seconds until we're sure we have all of them (which complicates the
// process and introduces a delay), or we just treat each one as its own property.
// 
// There are a maximum of 32 function blocks, numbered 0-31
// For a looped example of how to ref these, see MidiEndpointDeviceInformation::GetAdditionalPropertiesList()

#define MIDI_MAX_FUNCTION_BLOCKS 32
#define MIDI_FUNCTION_BLOCK_PROPERTY_INDEX_START 200

#define STRING_PKEY_MIDI_FunctionBlock00 MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"200"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_FunctionBlock00, 200);     // DEVPROP_TYPE_BINARY

#define STRING_PKEY_MIDI_FunctionBlock01 MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"201"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_FunctionBlock01, 201);     // DEVPROP_TYPE_BINARY

#define STRING_PKEY_MIDI_FunctionBlock02 MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"202"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_FunctionBlock02, 202);     // DEVPROP_TYPE_BINARY

#define STRING_PKEY_MIDI_FunctionBlock03 MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"203"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_FunctionBlock03, 203);     // DEVPROP_TYPE_BINARY

#define STRING_PKEY_MIDI_FunctionBlock04 MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"204"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_FunctionBlock04, 204);     // DEVPROP_TYPE_BINARY

#define STRING_PKEY_MIDI_FunctionBlock05 MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"205"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_FunctionBlock05, 205);     // DEVPROP_TYPE_BINARY

#define STRING_PKEY_MIDI_FunctionBlock06 MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"206"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_FunctionBlock06, 206);     // DEVPROP_TYPE_BINARY

#define STRING_PKEY_MIDI_FunctionBlock07 MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"207"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_FunctionBlock07, 207);     // DEVPROP_TYPE_BINARY

#define STRING_PKEY_MIDI_FunctionBlock08 MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"208"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_FunctionBlock08, 208);     // DEVPROP_TYPE_BINARY

#define STRING_PKEY_MIDI_FunctionBlock09 MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"209"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_FunctionBlock09, 209);     // DEVPROP_TYPE_BINARY

#define STRING_PKEY_MIDI_FunctionBlock10 MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"210"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_FunctionBlock10, 210);     // DEVPROP_TYPE_BINARY

#define STRING_PKEY_MIDI_FunctionBlock11 MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"211"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_FunctionBlock11, 211);     // DEVPROP_TYPE_BINARY

#define STRING_PKEY_MIDI_FunctionBlock12 MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"212"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_FunctionBlock12, 212);     // DEVPROP_TYPE_BINARY

#define STRING_PKEY_MIDI_FunctionBlock13 MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"213"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_FunctionBlock13, 213);     // DEVPROP_TYPE_BINARY

#define STRING_PKEY_MIDI_FunctionBlock14 MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"214"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_FunctionBlock14, 214);     // DEVPROP_TYPE_BINARY

#define STRING_PKEY_MIDI_FunctionBlock15 MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"215"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_FunctionBlock15, 215);     // DEVPROP_TYPE_BINARY

#define STRING_PKEY_MIDI_FunctionBlock16 MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"216"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_FunctionBlock16, 216);     // DEVPROP_TYPE_BINARY

#define STRING_PKEY_MIDI_FunctionBlock17 MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"217"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_FunctionBlock17, 217);     // DEVPROP_TYPE_BINARY

#define STRING_PKEY_MIDI_FunctionBlock18 MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"218"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_FunctionBlock18, 218);     // DEVPROP_TYPE_BINARY

#define STRING_PKEY_MIDI_FunctionBlock19 MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"219"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_FunctionBlock19, 219);     // DEVPROP_TYPE_BINARY

#define STRING_PKEY_MIDI_FunctionBlock20 MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"220"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_FunctionBlock20, 220);     // DEVPROP_TYPE_BINARY

#define STRING_PKEY_MIDI_FunctionBlock21 MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"221"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_FunctionBlock21, 221);     // DEVPROP_TYPE_BINARY

#define STRING_PKEY_MIDI_FunctionBlock22 MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"222"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_FunctionBlock22, 222);     // DEVPROP_TYPE_BINARY

#define STRING_PKEY_MIDI_FunctionBlock23 MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"223"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_FunctionBlock23, 223);     // DEVPROP_TYPE_BINARY

#define STRING_PKEY_MIDI_FunctionBlock24 MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"224"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_FunctionBlock24, 224);     // DEVPROP_TYPE_BINARY

#define STRING_PKEY_MIDI_FunctionBlock25 MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"225"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_FunctionBlock25, 225);     // DEVPROP_TYPE_BINARY

#define STRING_PKEY_MIDI_FunctionBlock26 MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"226"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_FunctionBlock26, 226);     // DEVPROP_TYPE_BINARY

#define STRING_PKEY_MIDI_FunctionBlock27 MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"227"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_FunctionBlock27, 227);     // DEVPROP_TYPE_BINARY

#define STRING_PKEY_MIDI_FunctionBlock28 MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"228"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_FunctionBlock28, 228);     // DEVPROP_TYPE_BINARY

#define STRING_PKEY_MIDI_FunctionBlock29 MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"229"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_FunctionBlock29, 229);     // DEVPROP_TYPE_BINARY

#define STRING_PKEY_MIDI_FunctionBlock30 MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"230"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_FunctionBlock30, 230);     // DEVPROP_TYPE_BINARY

#define STRING_PKEY_MIDI_FunctionBlock31 MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"231"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_FunctionBlock31, 231);     // DEVPROP_TYPE_BINARY


// Function Block Names start at 300
#define MIDI_FUNCTION_BLOCK_NAME_PROPERTY_INDEX_START 300

#define STRING_PKEY_MIDI_FunctionBlockName00 MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"300"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_FunctionBlockName00, 300);     // DEVPROP_TYPE_STRING

#define STRING_PKEY_MIDI_FunctionBlockName01 MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"301"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_FunctionBlockName01, 301);     // DEVPROP_TYPE_STRING

#define STRING_PKEY_MIDI_FunctionBlockName02 MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"302"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_FunctionBlockName02, 302);     // DEVPROP_TYPE_STRING

#define STRING_PKEY_MIDI_FunctionBlockName03 MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"303"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_FunctionBlockName03, 303);     // DEVPROP_TYPE_STRING

#define STRING_PKEY_MIDI_FunctionBlockName04 MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"304"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_FunctionBlockName04, 304);     // DEVPROP_TYPE_STRING

#define STRING_PKEY_MIDI_FunctionBlockName05 MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"305"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_FunctionBlockName05, 305);     // DEVPROP_TYPE_STRING

#define STRING_PKEY_MIDI_FunctionBlockName06 MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"306"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_FunctionBlockName06, 306);     // DEVPROP_TYPE_STRING

#define STRING_PKEY_MIDI_FunctionBlockName07 MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"307"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_FunctionBlockName07, 307);     // DEVPROP_TYPE_STRING

#define STRING_PKEY_MIDI_FunctionBlockName08 MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"308"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_FunctionBlockName08, 308);     // DEVPROP_TYPE_STRING

#define STRING_PKEY_MIDI_FunctionBlockName09 MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"309"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_FunctionBlockName09, 309);     // DEVPROP_TYPE_STRING

#define STRING_PKEY_MIDI_FunctionBlockName10 MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"310"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_FunctionBlockName10, 310);     // DEVPROP_TYPE_STRING

#define STRING_PKEY_MIDI_FunctionBlockName11 MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"311"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_FunctionBlockName11, 311);     // DEVPROP_TYPE_STRING

#define STRING_PKEY_MIDI_FunctionBlockName12 MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"312"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_FunctionBlockName12, 312);     // DEVPROP_TYPE_STRING

#define STRING_PKEY_MIDI_FunctionBlockName13 MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"313"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_FunctionBlockName13, 313);     // DEVPROP_TYPE_STRING

#define STRING_PKEY_MIDI_FunctionBlockName14 MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"314"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_FunctionBlockName14, 314);     // DEVPROP_TYPE_STRING

#define STRING_PKEY_MIDI_FunctionBlockName15 MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"315"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_FunctionBlockName15, 315);     // DEVPROP_TYPE_STRING

#define STRING_PKEY_MIDI_FunctionBlockName16 MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"316"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_FunctionBlockName16, 316);     // DEVPROP_TYPE_STRING

#define STRING_PKEY_MIDI_FunctionBlockName17 MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"317"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_FunctionBlockName17, 317);     // DEVPROP_TYPE_STRING

#define STRING_PKEY_MIDI_FunctionBlockName18 MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"318"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_FunctionBlockName18, 318);     // DEVPROP_TYPE_STRING

#define STRING_PKEY_MIDI_FunctionBlockName19 MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"319"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_FunctionBlockName19, 319);     // DEVPROP_TYPE_STRING

#define STRING_PKEY_MIDI_FunctionBlockName20 MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"320"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_FunctionBlockName20, 320);     // DEVPROP_TYPE_STRING

#define STRING_PKEY_MIDI_FunctionBlockName21 MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"321"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_FunctionBlockName21, 321);     // DEVPROP_TYPE_STRING

#define STRING_PKEY_MIDI_FunctionBlockName22 MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"322"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_FunctionBlockName22, 322);     // DEVPROP_TYPE_STRING

#define STRING_PKEY_MIDI_FunctionBlockName23 MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"323"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_FunctionBlockName23, 323);     // DEVPROP_TYPE_STRING

#define STRING_PKEY_MIDI_FunctionBlockName24 MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"324"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_FunctionBlockName24, 324);     // DEVPROP_TYPE_STRING

#define STRING_PKEY_MIDI_FunctionBlockName25 MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"325"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_FunctionBlockName25, 325);     // DEVPROP_TYPE_STRING

#define STRING_PKEY_MIDI_FunctionBlockName26 MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"326"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_FunctionBlockName26, 326);     // DEVPROP_TYPE_STRING

#define STRING_PKEY_MIDI_FunctionBlockName27 MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"327"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_FunctionBlockName27, 327);     // DEVPROP_TYPE_STRING

#define STRING_PKEY_MIDI_FunctionBlockName28 MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"328"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_FunctionBlockName28, 328);     // DEVPROP_TYPE_STRING

#define STRING_PKEY_MIDI_FunctionBlockName29 MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"329"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_FunctionBlockName29, 329);     // DEVPROP_TYPE_STRING

#define STRING_PKEY_MIDI_FunctionBlockName30 MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"330"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_FunctionBlockName30, 330);     // DEVPROP_TYPE_STRING

#define STRING_PKEY_MIDI_FunctionBlockName31 MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"331"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_FunctionBlockName31, 331);     // DEVPROP_TYPE_STRING




// In-protocol Negotiated information ================================================================
// starts at 400

#define MIDI_PROP_CONFIGURED_PROTOCOL_MIDI1 (uint8_t)0x01
#define MIDI_PROP_CONFIGURED_PROTOCOL_MIDI2 (uint8_t)0x02

#define STRING_PKEY_MIDI_EndpointConfiguredProtocol MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"400"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_EndpointConfiguredProtocol, 400);     // DEVPROP_TYPE_BYTE


#define STRING_PKEY_MIDI_EndpointConfiguredToSendJRTimestamps MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"405"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_EndpointConfiguredToSendJRTimestamps, 405);     // DEVPROP_TYPE_BOOLEAN

#define STRING_PKEY_MIDI_EndpointConfiguredToReceiveJRTimestamps MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"406"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_EndpointConfiguredToReceiveJRTimestamps, 406);     // DEVPROP_TYPE_BOOLEAN

#define STRING_PKEY_MIDI_EndpointConfigurationLastUpdateTime MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"407"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_EndpointConfigurationLastUpdateTime, 407);     // DEVPROP_TYPE_FILETIME




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



// Capability properties which can impact which plugins we instantiate =======================
// This is only for things that endpoints or user may supply, not something with in-protocol discovery
// Starts at 600

// Requires note off translation (Mackie). Translate Note On with zero velocity (MIDI 1.0) to note off
#define STRING_PKEY_MIDI_RequiresNoteOffTranslation MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"610"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_RequiresNoteOffTranslation, 610);     // DEVPROP_TYPE_BOOLEAN

#define STRING_PKEY_MIDI_RecommendedCCAutomationIntervalMS MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"611"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_RecommendedCCAutomationIntervalMS, 611);     // DEVPROP_TYPE_INT

#define STRING_PKEY_MIDI_SupportsMidiPolyphonicExpression MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"612"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_SupportsMidiPolyphonicExpression, 612);     // DEVPROP_TYPE_BOOLEAN

// TODO: Add in the other properties like
// - Can receive MIDI clock
// - Can use MIDI clock start
// - Can use MIDI timecode
// - Default SysEx delay between messages
// These values would be set by the user in the settings app, and then used by DAWs instead of keeping their own metadata



// Additional metrics ========================================================================
// Starts at 800

// Calculated latency for sending a message to a device. 
// We may be able to calculate this using the jitter-reduction timestamps.
#define STRING_PKEY_MIDI_MidiOutCalculatedLatencyTicks MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"800"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_MidiOutCalculatedLatencyTicks, 800);     // DEVPROP_TYPE_UINT64

// user-supplied latency ticks for a device
#define STRING_PKEY_MIDI_MidiOutUserSuppliedLatencyTicks MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"801"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_MidiOutUserSuppliedLatencyTicks, 801);     // DEVPROP_TYPE_UINT64

// true if we should use the user-supplied latency instead of the calculated latency
#define STRING_PKEY_MIDI_MidiOutLatencyTicksUserOverride MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"802"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_MidiOutLatencyTicksUserOverride, 802);     // DEVPROP_TYPE_BOOL




// Additional Transport-specific properties ==================================================
// Starts at 900

#define STRING_PKEY_MIDI_VirtualMidiEndpointAssociator MIDI_STRING_PKEY_GUID MIDI_STRING_PKEY_PID_SEPARATOR L"900"
DEFINE_MIDIDEVPROPKEY(PKEY_MIDI_VirtualMidiEndpointAssociator, 900);     // DEVPROP_TYPE_GUID




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

// this should probably just be an enum
#define MIDI_FUNCTION_BLOCK_DIRECTION_BLOCK_UNKNOWN         0x0
#define MIDI_FUNCTION_BLOCK_DIRECTION_BLOCK_INPUT           0x1
#define MIDI_FUNCTION_BLOCK_DIRECTION_BLOCK_OUTPUT          0x2
#define MIDI_FUNCTION_BLOCK_DIRECTION_BLOCK_BIDIRECTIONAL   0x3


// for PKEY_MIDI_FunctionBlocks
// these properties are raw from the messages
struct MidiFunctionBlockProperty
{
    uint16_t Size{ 0 };             // total size for this function block
    bool IsActive{ false };
    uint8_t BlockNumber{ 0 };
    uint8_t Reserved0{ 0 };         // unused in UMP 1.1
    uint8_t Direction{ 0 };         // one of the MIDI_FUNCTION_BLOCK_DIRECTION_xxx values
    uint8_t Midi1{ 0 };
    uint8_t UIHint{ 0 };
    uint8_t FirstGroup{ 0 };
    uint8_t NumberOfGroupsSpanned{ 0 };
    uint8_t MidiCIMessageVersionFormat{ 0 };
    uint8_t MaxSysEx8Streams{ 0 };

    uint32_t Reserved1{ 0 };        // unused in UMP 1.1
    uint32_t Reserved2{ 0 };        // unused in UMP 1.1
};

#define HRESULT_FROM_RPCSTATUS(status) status == RPC_S_OK ? S_OK : MAKE_HRESULT(SEVERITY_ERROR, FACILITY_RPC, status)

#define SAFE_CLOSEHANDLE(h) if (h) { CloseHandle(h); h = NULL; }


