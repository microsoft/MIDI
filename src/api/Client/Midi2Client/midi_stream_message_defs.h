// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================



// this is just to make intent clear
#define MIDI_RESERVED_WORD (uint32_t)0
#define MIDI_RESERVED_FIELD 0

#define MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_DISCOVERY 0x00
#define MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_INFO_NOTIFICATION 0x01
#define MIDI_STREAM_MESSAGE_STATUS_DEVICE_IDENTITY_NOTIFICATION 0x02
#define MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_NAME_NOTIFICATION 0x03
#define MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_PRODUCT_INSTANCE_ID_NOTIFICATION 0x04
#define MIDI_STREAM_MESSAGE_STATUS_STREAM_CONFIGURATION_REQUEST 0x05
#define MIDI_STREAM_MESSAGE_STATUS_STREAM_CONFIGURATION_NOTIFICATION 0x06

#define MIDI_STREAM_MESSAGE_STATUS_FUNCTION_BLOCK_INFO_NOTIFICATION 0x11
#define MIDI_STREAM_MESSAGE_STATUS_FUNCTION_BLOCK_NAME_NOTIFICATION 0x12


#define MIDI_STREAM_MESSAGE_STANDARD_FORM0 0x0

#define MIDI_STREAM_MESSAGE_MULTI_FORM_COMPLETE 0x0
#define MIDI_STREAM_MESSAGE_MULTI_FORM_START 0x1
#define MIDI_STREAM_MESSAGE_MULTI_FORM_CONTINUE 0x2
#define MIDI_STREAM_MESSAGE_MULTI_FORM_END 0x3

#define MIDI_STREAM_MESSAGE_ENDPOINT_NAME_MAX_LENGTH 98
#define MIDI_STREAM_MESSAGE_ENDPOINT_NAME_CHARACTERS_PER_PACKET 14

#define MIDI_STREAM_MESSAGE_FUNCTION_BLOCK_NAME_MAX_LENGTH 91
#define MIDI_STREAM_MESSAGE_FUNCTION_BLOCK_NAME_CHARACTERS_PER_PACKET 13

#define MIDI_STREAM_MESSAGE_PRODUCT_INSTANCE_ID_MAX_LENGTH 42
#define MIDI_STREAM_MESSAGE_PRODUCT_INSTANCE_ID_CHARACTERS_PER_PACKET 14


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
#define JSON_KEY_FB_NUMGROUPSSPANNED L"numberOfGroupsSpanned"
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





