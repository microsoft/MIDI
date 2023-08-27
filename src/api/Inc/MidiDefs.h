// Copyright (c) Microsoft Corporation. All rights reserved.

#pragma once

#ifndef PAGE_SIZE
#define PAGE_SIZE 0x1000
#endif

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
#define STRING_PKEY_MIDI_AbstractionLayer L"{3F114A6A-11FA-4BD0-9D2C-6B7780CD80AD},3"
DEFINE_DEVPROPKEY(PKEY_MIDI_AbstractionLayer, 0x3f114a6a, 0x11fa, 0x4bd0, 0x9d, 0x2c, 0x6b, 0x77, 0x80, 0xcd, 0x80, 0xad, 3);     // DEVPROP_TYPE_UINT64

#define STRING_PKEY_MIDI_AssociatedUMP L"{3F114A6A-11FA-4BD0-9D2C-6B7780CD80AD},2"
DEFINE_DEVPROPKEY(PKEY_MIDI_AssociatedUMP, 0x3f114a6a, 0x11fa, 0x4bd0, 0x9d, 0x2c, 0x6b, 0x77, 0x80, 0xcd, 0x80, 0xad, 2);     // DEVPROP_TYPE_UINT64

// Provided as a property for convenience. BLE, NET, USB, etc.
#define STRING_PKEY_MIDI_TransportMnemonic L"{3F114A6A-11FA-4BD0-9D2C-6B7780CD80AD},10"
DEFINE_DEVPROPKEY(PKEY_MIDI_TransportMnemonic, 0x3f114a6a, 0x11fa, 0x4bd0, 0x9d, 0x2c, 0x6b, 0x77, 0x80, 0xcd, 0x80, 0xad, 10);     // DEVPROP_TYPE_string


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











using unique_mmcss_handle = wil::unique_any<HANDLE, decltype(&::AvRevertMmThreadCharacteristics), AvRevertMmThreadCharacteristics>;
using unique_viewoffile = wil::unique_any<LPVOID, decltype(&::UnmapViewOfFile), UnmapViewOfFile>;

#define HRESULT_FROM_RPCSTATUS(status) status == RPC_S_OK ? S_OK : MAKE_HRESULT(SEVERITY_ERROR, FACILITY_RPC, status)

#define SAFE_CLOSEHANDLE(h) if (h) { CloseHandle(h); h = NULL; }

