// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#ifndef MIDI_SDK_IDL_DEFS
#define MIDI_SDK_IDL_DEFS


// Windows MIDI Services Client API UUID format
// Standard first four fields 8087b303-0519-c0de-31d1
// last field format ddxxxxxxxxxx for exclusive-to non-static interfaces
// last field format eexxxxxxxxxx for exclusive-to static interfaces
// last field format ffxxxxxxxxxx for stand-alone interfaces


// ============================================================================
// Windows.Devices.Midi2 : Interface number 001

#define UUID_IMidiApiStatics                                        8087b303-0519-c0de-31d1-ee0010000000

#define UUID_IMidiChannel									        8087b303-0519-c0de-31d1-dd0010001000
#define UUID_IMidiChannelStatics							        8087b303-0519-c0de-31d1-ee0010001000

#define UUID_IMidiGroup										        8087b303-0519-c0de-31d1-dd0010002000
#define UUID_IMidiGroupStatics								        8087b303-0519-c0de-31d1-ee0010002000

#define UUID_IMidiClockStatics								        8087b303-0519-c0de-31d1-ee0010003000

#define UUID_IMidiMessage32									        8087b303-0519-c0de-31d1-dd0010004032
#define UUID_IMidiMessage32Statics							        8087b303-0519-c0de-31d1-ee0010004032
#define UUID_IMidiMessage64									        8087b303-0519-c0de-31d1-dd0010004064
#define UUID_IMidiMessage64Statics							        8087b303-0519-c0de-31d1-ee0010004064
#define UUID_IMidiMessage96									        8087b303-0519-c0de-31d1-dd0010004096
#define UUID_IMidiMessage96Statics							        8087b303-0519-c0de-31d1-ee0010004096
#define UUID_IMidiMessage128								        8087b303-0519-c0de-31d1-dd0010004128
#define UUID_IMidiMessage128Statics							        8087b303-0519-c0de-31d1-ee0010004128

#define UUID_IMidiEndpointConnection						        8087b303-0519-c0de-31d1-dd0010005000
#define UUID_IMidiEndpointConnectionStatics					        8087b303-0519-c0de-31d1-ee0010005000

#define UUID_IMidiEndpointConnectionBasicSettings			        8087b303-0519-c0de-31d1-dd0010006000

#define UUID_IMidiMessageReceivedEventArgs					        8087b303-0519-c0de-31d1-dd0010007000

#define UUID_IMidiSession									        8087b303-0519-c0de-31d1-dd0010008000
#define UUID_IMidiSessionStatics							        8087b303-0519-c0de-31d1-ee0010009000

#define UUID_IMidiUniversalPacket							        "8087b303-0519-c0de-31d1-ff001000F010"
#define UUID_IMidiEndpointConnectionSettings				        "8087b303-0519-c0de-31d1-ff001000F020"
#define UUID_IMidiEndpointConnectionSource					        "8087b303-0519-c0de-31d1-ff001000F030"
#define UUID_IMidiEndpointMessageProcessingPlugin			        "8087b303-0519-c0de-31d1-ff001000F040"
#define UUID_IMidiMessageReceivedEventSource				        "8087b303-0519-c0de-31d1-ff001000F050"


// ============================================================================
// Windows.Devices.Midi2.ClientPlugins : Interface number 002

#define UUID_IMidiChannelEndpointListener					        8087b303-0519-c0de-31d1-dd0020001000
#define UUID_IMidiGroupEndpointListener						        8087b303-0519-c0de-31d1-dd0020002000
#define UUID_IMidiMessageTypeEndpointListener				        8087b303-0519-c0de-31d1-dd0020003000


// ============================================================================
// Windows.Devices.Midi2.Diagnostics  : Interface number 003

#define UUID_IMidiDiagnosticsStatics						        8087b303-0519-c0de-31d1-ee0030001000
#define UUID_IMidiServicePingResponse                               8087b303-0519-c0de-31d1-dd0030002000
#define UUID_IMidiServicePingResponseSummary				        8087b303-0519-c0de-31d1-dd0030003000

// ============================================================================
// Windows.Devices.Midi2.Enumeration : Interface number 004

#define UUID_IMidiEndpointDeviceInformation					        8087b303-0519-c0de-31d1-dd004000a000
#define UUID_IMidiEndpointDeviceInformationStatics			        8087b303-0519-c0de-31d1-ee004000a000

#define UUID_IMidiEndpointDeviceInformationAddedEventArgs	        8087b303-0519-c0de-31d1-dd004000b000
#define UUID_IMidiEndpointDeviceInformationRemovedEventArgs	        8087b303-0519-c0de-31d1-dd004000c000
#define UUID_IMidiEndpointDeviceInformationUpdatedEventArgs	        8087b303-0519-c0de-31d1-dd004000d000

#define UUID_IMidiEndpointDeviceWatcher						        8087b303-0519-c0de-31d1-dd004000e000
#define UUID_IMidiEndpointDeviceWatcherStatics				        8087b303-0519-c0de-31d1-ee004000e000

#define UUID_IMidiFunctionBlock								        8087b303-0519-c0de-31d1-dd004000f000
#define UUID_IMidiFunctionBlockStatics						        8087b303-0519-c0de-31d1-ee004000f000

#define UUID_IMidiGroupTerminalBlock						        8087b303-0519-c0de-31d1-dd0040010000
#define UUID_IMidiGroupTerminalBlockStatics					        8087b303-0519-c0de-31d1-ee0040010000

#define UUID_IMidiDeclaredDeviceIdentity						    8087b303-0519-c0de-31d1-dd0040011000
#define UUID_IMidiDeclaredEndpointInfo                              8087b303-0519-c0de-31d1-dd0040012000
#define UUID_IMidiDeclaredStreamConfiguration                       8087b303-0519-c0de-31d1-dd0040013000
#define UUID_IMidiEndpointTransportSuppliedInfo                     8087b303-0519-c0de-31d1-dd0040014000
#define UUID_IMidiEndpointUserSuppliedInfo                          8087b303-0519-c0de-31d1-dd0040015000

#define UUID_IMidi1PortNameTableEntry                               8087b303-0519-c0de-31d1-dd0040016000
#define UUID_IMidiEndpointAssociatedPortDeviceInformation	        8087b303-0519-c0de-31d1-dd0040017000
#define UUID_IMidiEndpointDevicePropertyHelperStatics		        8087b303-0519-c0de-31d1-ee0040018000
#define UUID_IMidiEndpointDeviceIdHelperStatics				        8087b303-0519-c0de-31d1-ee0040019000
#define UUID_IMidiLegacyPortDeviceInformation					    8087b303-0519-c0de-31d1-dd004001A000


#define UUID_IMidiLegacyPortDeviceInformationAddedEventArgs	        8087b303-0519-c0de-31d1-dd004001B000
#define UUID_IMidiLegacyPortDeviceInformationRemovedEventArgs	    8087b303-0519-c0de-31d1-dd004001C000
#define UUID_IMidiLegacyPortDeviceInformationUpdatedEventArgs	    8087b303-0519-c0de-31d1-dd004001D000

#define UUID_IMidiLegacyPortDeviceWatcher                           8087b303-0519-c0de-31d1-dd004001E000
#define UUID_IMidiLegacyPortDeviceWatcherStatics				    8087b303-0519-c0de-31d1-ee004001E000

#define UUID_IMidiParentDeviceInformation                           8087b303-0519-c0de-31d1-dd004001F000
#define UUID_IMidiParentDeviceInformationStatics                    8087b303-0519-c0de-31d1-ee004001F000



// ============================================================================
// Windows.Devices.Midi2.Reporting : Interface number 005

#define UUID_IMidiReportingStatics							        8087b303-0519-c0de-31d1-ee0050001000
#define UUID_IMidiServiceSessionInfo						        8087b303-0519-c0de-31d1-dd0050002000
#define UUID_IMidiServiceSessionConnectionInfo                      8087b303-0519-c0de-31d1-dd0050003000
#define UUID_IMidiServiceTransportPluginInfo                        8087b303-0519-c0de-31d1-dd0050004000

// ============================================================================
// Windows.Devices.Midi2.ServiceConfig : Interface number 006

#define UUID_IMidiServiceTransportPluginConfigManagerStatics        8087b303-0519-c0de-31d1-ee0060001000
#define UUID_IMidiServiceMidiServiceEndpointCustomizationConfig     8087b303-0519-c0de-31d1-dd0060002000

#define UUID_IMidiServiceTransportCommand                           8087b303-0519-c0de-31d1-dd0060003000
#define UUID_IMidiServiceTransportCommonCommandsStatics             8087b303-0519-c0de-31d1-ee0060003000

#define UUID_IMidiServiceConfigEndpointMatchCriteria                8087b303-0519-c0de-31d1-dd0060004000
#define UUID_IMidiServiceConfigEndpointMatchCriteriaStatics         8087b303-0519-c0de-31d1-ee0060004000

#define UUID_IMidiServiceTransportPluginConfig				        "8087b303-0519-c0de-31d1-ff0060005000"

#define UUID_IMidiServiceConfigResponse                             8087b303-0519-c0de-31d1-ee0060005000

// ============================================================================
// Windows.Devices.Midi2.CapabilityInquiry : Interface number 007

#define UUID_IMidiUniqueId									        8087b303-0519-c0de-31d1-dd0070001000
#define UUID_IMidiUniqueIdStatics							        8087b303-0519-c0de-31d1-ee0070001000



// ========== Utilities : Interface number 0E ========================================================================


// ============================================================================
// Windows.Devices.Midi2.Utilities.Message : Interface number 00E01

#define UUID_IMidiMessageBuilderStatics						        8087b303-0519-c0de-31d1-ee00E0101000
#define UUID_IMidiMessageConverterStatics					        8087b303-0519-c0de-31d1-ee00E0102000
#define UUID_IMidiMessageHelperStatics						        8087b303-0519-c0de-31d1-ee00E0103000
#define UUID_IMidiStreamMessageBuilderStatics				        8087b303-0519-c0de-31d1-ee00E0104000
#define UUID_IMidiSystemExclusive7MessageHelperStatics              8087b303-0519-c0de-31d1-dd00E0105000
#define UUID_IMidiUniversalSystemExclusive7MessageBuilderStatics    8087b303-0519-c0de-31d1-dd00E0106000

#define UUID_IMidiUniversalSystemExclusiveChannel			        8087b303-0519-c0de-31d1-dd00E0107000
#define UUID_IMidiUniversalSystemExclusiveChannelStatics	        8087b303-0519-c0de-31d1-ee00E0107000



// ============================================================================
// Windows.Devices.Midi2.Utilities.Sequencing : Interface number 00E02


// ============================================================================
// Windows.Devices.Midi2.Utilities.SysExTransfer : Interface number 00E03



// ========== Transports : Interface number 0F ========================================================================


// ============================================================================
// Windows.Devices.Midi2.Endpoints.BasicLoopback  : Interface number 00F01

#define UUID_IMidiBasicLoopbackEndpointCreationConfig		8087b303-0519-c0de-31d1-dd00F0101000
#define UUID_IMidiBasicLoopbackEndpointRemovalConfig		8087b303-0519-c0de-31d1-dd00F0102000
#define UUID_IMidiBasicLoopbackEndpointManagerStatics		8087b303-0519-c0de-31d1-ee00F0103000
#define UUID_IMidiBasicLoopbackEndpointCreationResult       8087b303-0519-c0de-31d1-dd00F0104000
#define UUID_IMidiBasicLoopbackEntry                        8087b303-0519-c0de-31d1-dd00F0105000   



// ============================================================================
// Windows.Devices.Midi2.Endpoints.Loopback : Interface number 00F02

#define UUID_IMidiLoopbackEndpointCreationConfig			8087b303-0519-c0de-31d1-dd00F0201000
#define UUID_IMidiLoopbackEndpointRemovalConfig				8087b303-0519-c0de-31d1-dd00F0202000
#define UUID_IMidiLoopbackEndpointManagerStatics			8087b303-0519-c0de-31d1-ee00F0203000
#define UUID_IMidiLoopbackEndpointCreationResult            8087b303-0519-c0de-31d1-dd00F0204000
#define UUID_IMidiLoopbackEntry                             8087b303-0519-c0de-31d1-dd00F0205000
#define UUID_IMidiLoopbackEndpointEntry                     8087b303-0519-c0de-31d1-dd00F0206000
#define UUID_IMidiLoopbackEndpointDefinition                8087b303-0519-c0de-31d1-dd00F0207000


// ============================================================================
// Windows.Devices.Midi2.Endpoints.Network : Interface number 00F03

// ============================================================================
// Windows.Devices.Midi2.Endpoints.Virtual : Interface number 00F04

#define UUID_IMidiStreamConfigRequestReceivedEventArgs		8087b303-0519-c0de-31d1-dd00F0401000
#define UUID_IMidiVirtualDevice								8087b303-0519-c0de-31d1-dd00F0402000
#define UUID_IMidiVirtualDeviceCreationConfig				8087b303-0519-c0de-31d1-dd00F0403000
#define UUID_IMidiVirtualDeviceManagerStatics				8087b303-0519-c0de-31d1-ee00F0404000




// ============================================================================
// Windows.Devices.Midi2.VirtualPatchBay



#endif