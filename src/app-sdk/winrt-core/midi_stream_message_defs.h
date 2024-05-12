// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#define MIDI_CACHE_PROPERTY_ENDPOINT_PRODUCT_INSTANCE_ID L"Endpoint.ProductInstanceId"
#define MIDI_CACHE_PROPERTY_ENDPOINT_NAME L"Endpoint.Name"
#define MIDI_CACHE_PROPERTY_ENDPOINT_DEVICE_IDENTITY L"Endpoint.DeviceIdentity"


// function block serialization

#define JSON_KEY_FB_NUMBER L"functionBlockNumber"
#define JSON_KEY_FB_NAME L"name"
#define JSON_KEY_FB_ACTIVE L"active"
#define JSON_KEY_FB_UIHINT L"uiHint"
#define JSON_KEY_FB_MIDI10 L"midi10"
#define JSON_KEY_FB_DIRECTION L"direction"
#define JSON_KEY_FB_FIRSTGROUP L"firstGroupIndex"
#define JSON_KEY_FB_NUMGROUPSSPANNED L"countGroupsSpanned"
#define JSON_KEY_FB_MIDICIFORMAT L"midiCIMessageFormat"


// endpoint serialization


#define JSON_KEY_EP_NAME L"name"
#define JSON_KEY_EP_PID L"productInstanceId"
#define JSON_KEY_EP_UMPVERMAJ L"umpVersionMajor"
#define JSON_KEY_EP_UMPVERMIN L"umpVersionMinor"
#define JSON_KEY_EP_STATICFB L"staticFunctionBlocks"
#define JSON_KEY_EP_NUMFB L"numberOfFunctionBlocks"
#define JSON_KEY_EP_MIDI2 L"midi2ProtocolCapability"
#define JSON_KEY_EP_MIDI1 L"midi1ProtocolCapability"
#define JSON_KEY_EP_RECJR L"receiveJRTimestampCapability"
#define JSON_KEY_EP_SENDJR L"sendJRTimestampCapability"


