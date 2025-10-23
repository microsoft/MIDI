// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#ifndef MIDI_SDK_IDL_DEFS
#define MIDI_SDK_IDL_DEFS


#define UUID_IMidiUniqueId									8087b303-0519-31d1-c0de-dd0000001000
#define UUID_IMidiUniqueIdStatics							8087b303-0519-31d1-c0de-ee0000001000

#define UUID_IMidiChannelEndpointListener					8087b303-0519-31d1-c0de-dd0000002000
#define UUID_IMidiGroupEndpointListener						8087b303-0519-31d1-c0de-dd0000003000
#define UUID_IMidiMessageTypeEndpointListener				8087b303-0519-31d1-c0de-dd0000004000

#define UUID_IMidiChannel									8087b303-0519-31d1-c0de-dd0000005000
#define UUID_IMidiChannelStatics							8087b303-0519-31d1-c0de-ee0000005000

#define UUID_IMidiClockStatics								8087b303-0519-31d1-c0de-ee0000006000

#define UUID_IMidiGroup										8087b303-0519-31d1-c0de-dd0000007000
#define UUID_IMidiGroupStatics								8087b303-0519-31d1-c0de-ee0000007000

#define UUID_IMidiUniversalSystemExclusiveChannel			8087b303-0519-31d1-c0de-dd0000008000
#define UUID_IMidiUniversalSystemExclusiveChannelStatics	8087b303-0519-31d1-c0de-ee0000008000

#define UUID_IMidiEndpointDeviceIdHelperStatics				8087b303-0519-31d1-c0de-ee0000009000

#define UUID_IMidiEndpointDeviceInformation					8087b303-0519-31d1-c0de-dd000000a000
#define UUID_IMidiEndpointDeviceInformationStatics			8087b303-0519-31d1-c0de-ee000000a000

#define UUID_IMidiEndpointDeviceInformationAddedEventArgs	8087b303-0519-31d1-c0de-dd000000b000
#define UUID_IMidiEndpointDeviceInformationRemovedEventArgs	8087b303-0519-31d1-c0de-dd000000c000
#define UUID_IMidiEndpointDeviceInformationUpdatedEventArgs	8087b303-0519-31d1-c0de-dd000000d000

#define UUID_IMidiEndpointDeviceWatcher						8087b303-0519-31d1-c0de-dd000000e000
#define UUID_IMidiEndpointDeviceWatcherStatics				8087b303-0519-31d1-c0de-ee000000e000

#define UUID_IMidiFunctionBlock								8087b303-0519-31d1-c0de-dd000000f000
#define UUID_IMidiFunctionBlockStatics						8087b303-0519-31d1-c0de-ee000000f000

#define UUID_IMidiGroupTerminalBlock						8087b303-0519-31d1-c0de-dd0000010000
#define UUID_IMidiGroupTerminalBlockStatics					8087b303-0519-31d1-c0de-ee0000010000

#define UUID_IMidiEndpointAssociatedPortDeviceInformation	8087b303-0519-31d1-c0de-dd0000020000

#define UUID_IMidiEndpointDevicePropertyHelperStatics		8087b303-0519-31d1-c0de-ee0000030000

#define UUID_IMidiMessage32									8087b303-0519-31d1-c0de-dd0000040032
#define UUID_IMidiMessage32Statics							8087b303-0519-31d1-c0de-ee0000040032
#define UUID_IMidiMessage64									8087b303-0519-31d1-c0de-dd0000040064
#define UUID_IMidiMessage64Statics							8087b303-0519-31d1-c0de-ee0000040064
#define UUID_IMidiMessage96									8087b303-0519-31d1-c0de-dd0000040096
#define UUID_IMidiMessage96Statics							8087b303-0519-31d1-c0de-ee0000040096
#define UUID_IMidiMessage128								8087b303-0519-31d1-c0de-dd0000040128
#define UUID_IMidiMessage128Statics							8087b303-0519-31d1-c0de-ee0000040128

#define UUID_IMidiEndpointConnection						8087b303-0519-31d1-c0de-dd0000050000
#define UUID_IMidiEndpointConnectionStatics					8087b303-0519-31d1-c0de-ee0000050000

#define UUID_IMidiEndpointConnectionBasicSettings			8087b303-0519-31d1-c0de-dd0000060000

#define UUID_IMidiMessageReceivedEventArgs					8087b303-0519-31d1-c0de-dd0000070000

#define UUID_IMidiSession									8087b303-0519-31d1-c0de-dd0000080000
#define UUID_IMidiSessionStatics							8087b303-0519-31d1-c0de-ee0000080000

// loopback endpoints
#define UUID_IMidiLoopbackEndpointCreationConfig			8087b303-0519-31d1-c0de-dd0000190000
#define UUID_IMidiLoopbackEndpointRemovalConfig				8087b303-0519-31d1-c0de-dd00001a0000
#define UUID_IMidiLoopbackEndpointManagerStatics			8087b303-0519-31d1-c0de-ee00001b0000

// virtual device
#define UUID_IMidiStreamConfigRequestReceivedEventArgs		8087b303-0519-31d1-c0de-dd00002a0000
#define UUID_IMidiVirtualDevice								8087b303-0519-31d1-c0de-dd00002b0000
#define UUID_IMidiVirtualDeviceCreationConfig				8087b303-0519-31d1-c0de-dd00002c0000
#define UUID_IMidiVirtualDeviceManagerStatics				8087b303-0519-31d1-c0de-ee00002d0000


// TODO: Virtual Patch bay

// TODO: Network MIDI 2.0 endpoints


// Service config

#define UUID_IMidiServiceConfigStatics						    8087b303-0519-31d1-c0de-ee0010000000
#define UUID_IMidiServiceMidiServiceEndpointCustomizationConfig 8087b303-0519-31d1-c0de-dd0011000000
#define UUID_IMidiServiceTransportCommand                       8087b303-0519-31d1-c0de-dd0012000000
#define UUID_IMidiServiceTransportCommonCommandsStatics         8087b303-0519-31d1-c0de-ee0013000000

#define UUID_IMidiServiceConfigEndpointMatchCriteria            8087b303-0519-31d1-c0de-dd0014000000
#define UUID_IMidiServiceConfigEndpointMatchCriteriaStatics     8087b303-0519-31d1-c0de-ee0014000000

// Utilities

#define UUID_IMidiDiagnosticsStatics						8087b303-0519-31d1-c0de-ee0020001000
#define UUID_IMidiServicePingResponseSummary				8087b303-0519-31d1-c0de-dd0020002000

#define UUID_IMidiMessageBuilderStatics						8087b303-0519-31d1-c0de-ee0030001000
#define UUID_IMidiMessageConverterStatics					8087b303-0519-31d1-c0de-ee0030002000
#define UUID_IMidiMessageHelperStatics						8087b303-0519-31d1-c0de-ee0030003000
#define UUID_IMidiStreamMessageBuilderStatics				8087b303-0519-31d1-c0de-ee0030004000

#define UUID_IMidiReportingStatics							8087b303-0519-31d1-c0de-ee0040001000
#define UUID_IMidiServiceSessionInfo						8087b303-0519-31d1-c0de-dd0040002000

#define UUID_IMidiRuntimeInformationStatics					8087b303-0519-31d1-c0de-ee0050001000
#define UUID_IMidiRuntimeVersion							8087b303-0519-31d1-c0de-dd0050002000

#define UUID_IMidiRuntimeRelease							8087b303-0519-31d1-c0de-dd0060001000
#define UUID_IMidiRuntimeUpdateUtilityStatics				8087b303-0519-31d1-c0de-ee0060002000




#define UUID_IMidiUniversalPacket							"8087b303-0519-31d1-c0de-ff0000000010"
#define UUID_IMidiEndpointConnectionSettings				"8087b303-0519-31d1-c0de-ff0000000020"
#define UUID_IMidiEndpointConnectionSource					"8087b303-0519-31d1-c0de-ff0000000030"
#define UUID_IMidiEndpointMessageProcessingPlugin			"8087b303-0519-31d1-c0de-ff0000000040"
#define UUID_IMidiMessageReceivedEventSource				"8087b303-0519-31d1-c0de-ff0000000050"
#define UUID_IMidiServiceTransportPluginConfig				"8087b303-0519-31d1-c0de-ff0000000060"


#endif