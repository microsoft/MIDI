// Copyright (c) The NodeRT Contributors
// All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the ""License""); you may
// not use this file except in compliance with the License. You may obtain a
// copy of the License at http://www.apache.org/licenses/LICENSE-2.0
//
// THIS CODE IS PROVIDED ON AN  *AS IS* BASIS, WITHOUT WARRANTIES OR CONDITIONS
// OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION ANY
// IMPLIED WARRANTIES OR CONDITIONS OF TITLE, FITNESS FOR A PARTICULAR PURPOSE,
// MERCHANTABLITY OR NON-INFRINGEMENT.
//
// See the Apache Version 2.0 License for specific language governing permissions
// and limitations under the License.

// TODO: Verify that this is is still needed..
#define NTDDI_VERSION 0x06010000

#include <v8.h>
#include "nan.h"
#include <string>
#include <ppltasks.h>
#include "CollectionsConverter.h"
#include "CollectionsWrap.h"
#include "node-async.h"
#include "NodeRtUtils.h"
#include "OpaqueWrapper.h"
#include "WrapperBase.h"

#using <Windows.Devices.Midi2.WinMD>

// this undefs fixes the issues of compiling Windows.Data.Json, Windows.Storag.FileProperties, and Windows.Stroage.Search
// Some of the node header files brings windows definitions with the same names as some of the WinRT methods
#undef DocumentProperties
#undef GetObject
#undef CreateEvent
#undef FindText
#undef SendMessage

const char* REGISTRATION_TOKEN_MAP_PROPERTY_NAME = "__registrationTokenMap__";

using v8::Array;
using v8::String;
using v8::Value;
using v8::Boolean;
using v8::Integer;
using v8::FunctionTemplate;
using v8::Object;
using v8::Local;
using v8::Function;
using v8::Date;
using v8::Number;
using v8::PropertyAttribute;
using v8::Primitive;
using Nan::HandleScope;
using Nan::Persistent;
using Nan::Undefined;
using Nan::True;
using Nan::False;
using Nan::Null;
using Nan::MaybeLocal;
using Nan::EscapableHandleScope;
using Nan::HandleScope;
using Nan::TryCatch;
using namespace concurrency;

namespace NodeRT { namespace Windows { namespace Devices { namespace Midi2 { 
  v8::Local<v8::Value> WrapIMidiEndpointConnectionSource(::Windows::Devices::Midi2::IMidiEndpointConnectionSource^ wintRtInstance);
  ::Windows::Devices::Midi2::IMidiEndpointConnectionSource^ UnwrapIMidiEndpointConnectionSource(Local<Value> value);
  
  v8::Local<v8::Value> WrapIMidiEndpointDefinedConnectionSettings(::Windows::Devices::Midi2::IMidiEndpointDefinedConnectionSettings^ wintRtInstance);
  ::Windows::Devices::Midi2::IMidiEndpointDefinedConnectionSettings^ UnwrapIMidiEndpointDefinedConnectionSettings(Local<Value> value);
  
  v8::Local<v8::Value> WrapIMidiEndpointMessageProcessingPlugin(::Windows::Devices::Midi2::IMidiEndpointMessageProcessingPlugin^ wintRtInstance);
  ::Windows::Devices::Midi2::IMidiEndpointMessageProcessingPlugin^ UnwrapIMidiEndpointMessageProcessingPlugin(Local<Value> value);
  
  v8::Local<v8::Value> WrapIMidiMessageReceivedEventSource(::Windows::Devices::Midi2::IMidiMessageReceivedEventSource^ wintRtInstance);
  ::Windows::Devices::Midi2::IMidiMessageReceivedEventSource^ UnwrapIMidiMessageReceivedEventSource(Local<Value> value);
  
  v8::Local<v8::Value> WrapIMidiTransportSettingsData(::Windows::Devices::Midi2::IMidiTransportSettingsData^ wintRtInstance);
  ::Windows::Devices::Midi2::IMidiTransportSettingsData^ UnwrapIMidiTransportSettingsData(Local<Value> value);
  
  v8::Local<v8::Value> WrapIMidiUniversalPacket(::Windows::Devices::Midi2::IMidiUniversalPacket^ wintRtInstance);
  ::Windows::Devices::Midi2::IMidiUniversalPacket^ UnwrapIMidiUniversalPacket(Local<Value> value);
  
  v8::Local<v8::Value> WrapMidiChannel(::Windows::Devices::Midi2::MidiChannel^ wintRtInstance);
  ::Windows::Devices::Midi2::MidiChannel^ UnwrapMidiChannel(Local<Value> value);
  
  v8::Local<v8::Value> WrapMidiChannelEndpointListener(::Windows::Devices::Midi2::MidiChannelEndpointListener^ wintRtInstance);
  ::Windows::Devices::Midi2::MidiChannelEndpointListener^ UnwrapMidiChannelEndpointListener(Local<Value> value);
  
  v8::Local<v8::Value> WrapMidiClock(::Windows::Devices::Midi2::MidiClock^ wintRtInstance);
  ::Windows::Devices::Midi2::MidiClock^ UnwrapMidiClock(Local<Value> value);
  
  v8::Local<v8::Value> WrapMidiEndpointConnection(::Windows::Devices::Midi2::MidiEndpointConnection^ wintRtInstance);
  ::Windows::Devices::Midi2::MidiEndpointConnection^ UnwrapMidiEndpointConnection(Local<Value> value);
  
  v8::Local<v8::Value> WrapMidiEndpointConnectionOptions(::Windows::Devices::Midi2::MidiEndpointConnectionOptions^ wintRtInstance);
  ::Windows::Devices::Midi2::MidiEndpointConnectionOptions^ UnwrapMidiEndpointConnectionOptions(Local<Value> value);
  
  v8::Local<v8::Value> WrapMidiEndpointDeviceInformation(::Windows::Devices::Midi2::MidiEndpointDeviceInformation^ wintRtInstance);
  ::Windows::Devices::Midi2::MidiEndpointDeviceInformation^ UnwrapMidiEndpointDeviceInformation(Local<Value> value);
  
  v8::Local<v8::Value> WrapMidiEndpointDeviceInformationUpdateEventArgs(::Windows::Devices::Midi2::MidiEndpointDeviceInformationUpdateEventArgs^ wintRtInstance);
  ::Windows::Devices::Midi2::MidiEndpointDeviceInformationUpdateEventArgs^ UnwrapMidiEndpointDeviceInformationUpdateEventArgs(Local<Value> value);
  
  v8::Local<v8::Value> WrapMidiEndpointDeviceWatcher(::Windows::Devices::Midi2::MidiEndpointDeviceWatcher^ wintRtInstance);
  ::Windows::Devices::Midi2::MidiEndpointDeviceWatcher^ UnwrapMidiEndpointDeviceWatcher(Local<Value> value);
  
  v8::Local<v8::Value> WrapMidiFunctionBlock(::Windows::Devices::Midi2::MidiFunctionBlock^ wintRtInstance);
  ::Windows::Devices::Midi2::MidiFunctionBlock^ UnwrapMidiFunctionBlock(Local<Value> value);
  
  v8::Local<v8::Value> WrapMidiGroup(::Windows::Devices::Midi2::MidiGroup^ wintRtInstance);
  ::Windows::Devices::Midi2::MidiGroup^ UnwrapMidiGroup(Local<Value> value);
  
  v8::Local<v8::Value> WrapMidiGroupEndpointListener(::Windows::Devices::Midi2::MidiGroupEndpointListener^ wintRtInstance);
  ::Windows::Devices::Midi2::MidiGroupEndpointListener^ UnwrapMidiGroupEndpointListener(Local<Value> value);
  
  v8::Local<v8::Value> WrapMidiGroupTerminalBlock(::Windows::Devices::Midi2::MidiGroupTerminalBlock^ wintRtInstance);
  ::Windows::Devices::Midi2::MidiGroupTerminalBlock^ UnwrapMidiGroupTerminalBlock(Local<Value> value);
  
  v8::Local<v8::Value> WrapMidiMessage128(::Windows::Devices::Midi2::MidiMessage128^ wintRtInstance);
  ::Windows::Devices::Midi2::MidiMessage128^ UnwrapMidiMessage128(Local<Value> value);
  
  v8::Local<v8::Value> WrapMidiMessage32(::Windows::Devices::Midi2::MidiMessage32^ wintRtInstance);
  ::Windows::Devices::Midi2::MidiMessage32^ UnwrapMidiMessage32(Local<Value> value);
  
  v8::Local<v8::Value> WrapMidiMessage64(::Windows::Devices::Midi2::MidiMessage64^ wintRtInstance);
  ::Windows::Devices::Midi2::MidiMessage64^ UnwrapMidiMessage64(Local<Value> value);
  
  v8::Local<v8::Value> WrapMidiMessage96(::Windows::Devices::Midi2::MidiMessage96^ wintRtInstance);
  ::Windows::Devices::Midi2::MidiMessage96^ UnwrapMidiMessage96(Local<Value> value);
  
  v8::Local<v8::Value> WrapMidiMessageBuilder(::Windows::Devices::Midi2::MidiMessageBuilder^ wintRtInstance);
  ::Windows::Devices::Midi2::MidiMessageBuilder^ UnwrapMidiMessageBuilder(Local<Value> value);
  
  v8::Local<v8::Value> WrapMidiMessageConverter(::Windows::Devices::Midi2::MidiMessageConverter^ wintRtInstance);
  ::Windows::Devices::Midi2::MidiMessageConverter^ UnwrapMidiMessageConverter(Local<Value> value);
  
  v8::Local<v8::Value> WrapMidiMessageProcessingPluginInformation(::Windows::Devices::Midi2::MidiMessageProcessingPluginInformation^ wintRtInstance);
  ::Windows::Devices::Midi2::MidiMessageProcessingPluginInformation^ UnwrapMidiMessageProcessingPluginInformation(Local<Value> value);
  
  v8::Local<v8::Value> WrapMidiMessageReceivedEventArgs(::Windows::Devices::Midi2::MidiMessageReceivedEventArgs^ wintRtInstance);
  ::Windows::Devices::Midi2::MidiMessageReceivedEventArgs^ UnwrapMidiMessageReceivedEventArgs(Local<Value> value);
  
  v8::Local<v8::Value> WrapMidiMessageTranslator(::Windows::Devices::Midi2::MidiMessageTranslator^ wintRtInstance);
  ::Windows::Devices::Midi2::MidiMessageTranslator^ UnwrapMidiMessageTranslator(Local<Value> value);
  
  v8::Local<v8::Value> WrapMidiMessageTypeEndpointListener(::Windows::Devices::Midi2::MidiMessageTypeEndpointListener^ wintRtInstance);
  ::Windows::Devices::Midi2::MidiMessageTypeEndpointListener^ UnwrapMidiMessageTypeEndpointListener(Local<Value> value);
  
  v8::Local<v8::Value> WrapMidiMessageUtility(::Windows::Devices::Midi2::MidiMessageUtility^ wintRtInstance);
  ::Windows::Devices::Midi2::MidiMessageUtility^ UnwrapMidiMessageUtility(Local<Value> value);
  
  v8::Local<v8::Value> WrapMidiService(::Windows::Devices::Midi2::MidiService^ wintRtInstance);
  ::Windows::Devices::Midi2::MidiService^ UnwrapMidiService(Local<Value> value);
  
  v8::Local<v8::Value> WrapMidiServicePingResponse(::Windows::Devices::Midi2::MidiServicePingResponse^ wintRtInstance);
  ::Windows::Devices::Midi2::MidiServicePingResponse^ UnwrapMidiServicePingResponse(Local<Value> value);
  
  v8::Local<v8::Value> WrapMidiServicePingResponseSummary(::Windows::Devices::Midi2::MidiServicePingResponseSummary^ wintRtInstance);
  ::Windows::Devices::Midi2::MidiServicePingResponseSummary^ UnwrapMidiServicePingResponseSummary(Local<Value> value);
  
  v8::Local<v8::Value> WrapMidiSession(::Windows::Devices::Midi2::MidiSession^ wintRtInstance);
  ::Windows::Devices::Midi2::MidiSession^ UnwrapMidiSession(Local<Value> value);
  
  v8::Local<v8::Value> WrapMidiSessionSettings(::Windows::Devices::Midi2::MidiSessionSettings^ wintRtInstance);
  ::Windows::Devices::Midi2::MidiSessionSettings^ UnwrapMidiSessionSettings(Local<Value> value);
  
  v8::Local<v8::Value> WrapMidiStreamConfigurationRequestedSettings(::Windows::Devices::Midi2::MidiStreamConfigurationRequestedSettings^ wintRtInstance);
  ::Windows::Devices::Midi2::MidiStreamConfigurationRequestedSettings^ UnwrapMidiStreamConfigurationRequestedSettings(Local<Value> value);
  
  v8::Local<v8::Value> WrapMidiStreamMessageBuilder(::Windows::Devices::Midi2::MidiStreamMessageBuilder^ wintRtInstance);
  ::Windows::Devices::Midi2::MidiStreamMessageBuilder^ UnwrapMidiStreamMessageBuilder(Local<Value> value);
  
  v8::Local<v8::Value> WrapMidiTransportPluginInformation(::Windows::Devices::Midi2::MidiTransportPluginInformation^ wintRtInstance);
  ::Windows::Devices::Midi2::MidiTransportPluginInformation^ UnwrapMidiTransportPluginInformation(Local<Value> value);
  
  v8::Local<v8::Value> WrapMidiUniqueId(::Windows::Devices::Midi2::MidiUniqueId^ wintRtInstance);
  ::Windows::Devices::Midi2::MidiUniqueId^ UnwrapMidiUniqueId(Local<Value> value);
  
  v8::Local<v8::Value> WrapMidiVirtualDevice(::Windows::Devices::Midi2::MidiVirtualDevice^ wintRtInstance);
  ::Windows::Devices::Midi2::MidiVirtualDevice^ UnwrapMidiVirtualDevice(Local<Value> value);
  



  static void InitMidi1ChannelVoiceMessageStatusEnum(const Local<Object> exports) {
    HandleScope scope;

    Local<Object> enumObject = Nan::New<Object>();

    Nan::Set(exports, Nan::New<String>("Midi1ChannelVoiceMessageStatus").ToLocalChecked(), enumObject);
    Nan::Set(enumObject, Nan::New<String>("noteOff").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::Midi1ChannelVoiceMessageStatus::NoteOff)));
    Nan::Set(enumObject, Nan::New<String>("noteOn").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::Midi1ChannelVoiceMessageStatus::NoteOn)));
    Nan::Set(enumObject, Nan::New<String>("polyPressure").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::Midi1ChannelVoiceMessageStatus::PolyPressure)));
    Nan::Set(enumObject, Nan::New<String>("controlChange").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::Midi1ChannelVoiceMessageStatus::ControlChange)));
    Nan::Set(enumObject, Nan::New<String>("programChange").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::Midi1ChannelVoiceMessageStatus::ProgramChange)));
    Nan::Set(enumObject, Nan::New<String>("channelPressure").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::Midi1ChannelVoiceMessageStatus::ChannelPressure)));
    Nan::Set(enumObject, Nan::New<String>("pitchBend").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::Midi1ChannelVoiceMessageStatus::PitchBend)));
  }

  static void InitMidi2ChannelVoiceMessageStatusEnum(const Local<Object> exports) {
    HandleScope scope;

    Local<Object> enumObject = Nan::New<Object>();

    Nan::Set(exports, Nan::New<String>("Midi2ChannelVoiceMessageStatus").ToLocalChecked(), enumObject);
    Nan::Set(enumObject, Nan::New<String>("registeredPerNoteController").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::Midi2ChannelVoiceMessageStatus::RegisteredPerNoteController)));
    Nan::Set(enumObject, Nan::New<String>("assignablePerNoteController").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::Midi2ChannelVoiceMessageStatus::AssignablePerNoteController)));
    Nan::Set(enumObject, Nan::New<String>("registeredController").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::Midi2ChannelVoiceMessageStatus::RegisteredController)));
    Nan::Set(enumObject, Nan::New<String>("assignableController").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::Midi2ChannelVoiceMessageStatus::AssignableController)));
    Nan::Set(enumObject, Nan::New<String>("relativeRegisteredController").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::Midi2ChannelVoiceMessageStatus::RelativeRegisteredController)));
    Nan::Set(enumObject, Nan::New<String>("relativeAssignableController").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::Midi2ChannelVoiceMessageStatus::RelativeAssignableController)));
    Nan::Set(enumObject, Nan::New<String>("perNotePitchBend").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::Midi2ChannelVoiceMessageStatus::PerNotePitchBend)));
    Nan::Set(enumObject, Nan::New<String>("noteOff").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::Midi2ChannelVoiceMessageStatus::NoteOff)));
    Nan::Set(enumObject, Nan::New<String>("noteOn").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::Midi2ChannelVoiceMessageStatus::NoteOn)));
    Nan::Set(enumObject, Nan::New<String>("polyPressure").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::Midi2ChannelVoiceMessageStatus::PolyPressure)));
    Nan::Set(enumObject, Nan::New<String>("controlChange").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::Midi2ChannelVoiceMessageStatus::ControlChange)));
    Nan::Set(enumObject, Nan::New<String>("programChange").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::Midi2ChannelVoiceMessageStatus::ProgramChange)));
    Nan::Set(enumObject, Nan::New<String>("channelPressure").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::Midi2ChannelVoiceMessageStatus::ChannelPressure)));
    Nan::Set(enumObject, Nan::New<String>("pitchBend").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::Midi2ChannelVoiceMessageStatus::PitchBend)));
    Nan::Set(enumObject, Nan::New<String>("perNoteManagement").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::Midi2ChannelVoiceMessageStatus::PerNoteManagement)));
  }

  static void InitMidiEndpointConnectionSharingEnum(const Local<Object> exports) {
    HandleScope scope;

    Local<Object> enumObject = Nan::New<Object>();

    Nan::Set(exports, Nan::New<String>("MidiEndpointConnectionSharing").ToLocalChecked(), enumObject);
    Nan::Set(enumObject, Nan::New<String>("unknown").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiEndpointConnectionSharing::Unknown)));
    Nan::Set(enumObject, Nan::New<String>("multiClient").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiEndpointConnectionSharing::MultiClient)));
    Nan::Set(enumObject, Nan::New<String>("exclusive").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiEndpointConnectionSharing::Exclusive)));
  }

  static void InitMidiEndpointConnectionSharingPreferenceEnum(const Local<Object> exports) {
    HandleScope scope;

    Local<Object> enumObject = Nan::New<Object>();

    Nan::Set(exports, Nan::New<String>("MidiEndpointConnectionSharingPreference").ToLocalChecked(), enumObject);
    Nan::Set(enumObject, Nan::New<String>("default").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiEndpointConnectionSharingPreference::Default)));
    Nan::Set(enumObject, Nan::New<String>("preferMultiClient").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiEndpointConnectionSharingPreference::PreferMultiClient)));
    Nan::Set(enumObject, Nan::New<String>("requireMultiClient").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiEndpointConnectionSharingPreference::RequireMultiClient)));
    Nan::Set(enumObject, Nan::New<String>("preferExclusiveMode").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiEndpointConnectionSharingPreference::PreferExclusiveMode)));
    Nan::Set(enumObject, Nan::New<String>("requireExclusiveMode").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiEndpointConnectionSharingPreference::RequireExclusiveMode)));
  }

  static void InitMidiEndpointDeviceInformationFilterEnum(const Local<Object> exports) {
    HandleScope scope;

    Local<Object> enumObject = Nan::New<Object>();

    Nan::Set(exports, Nan::New<String>("MidiEndpointDeviceInformationFilter").ToLocalChecked(), enumObject);
    Nan::Set(enumObject, Nan::New<String>("includeClientUmpNative").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiEndpointDeviceInformationFilter::IncludeClientUmpNative)));
    Nan::Set(enumObject, Nan::New<String>("includeClientByteStreamNative").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiEndpointDeviceInformationFilter::IncludeClientByteStreamNative)));
    Nan::Set(enumObject, Nan::New<String>("includeVirtualDeviceResponder").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiEndpointDeviceInformationFilter::IncludeVirtualDeviceResponder)));
    Nan::Set(enumObject, Nan::New<String>("includeDiagnosticLoopback").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiEndpointDeviceInformationFilter::IncludeDiagnosticLoopback)));
    Nan::Set(enumObject, Nan::New<String>("includeDiagnosticPing").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiEndpointDeviceInformationFilter::IncludeDiagnosticPing)));
  }

  static void InitMidiEndpointDeviceInformationSortOrderEnum(const Local<Object> exports) {
    HandleScope scope;

    Local<Object> enumObject = Nan::New<Object>();

    Nan::Set(exports, Nan::New<String>("MidiEndpointDeviceInformationSortOrder").ToLocalChecked(), enumObject);
    Nan::Set(enumObject, Nan::New<String>("none").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiEndpointDeviceInformationSortOrder::None)));
    Nan::Set(enumObject, Nan::New<String>("name").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiEndpointDeviceInformationSortOrder::Name)));
    Nan::Set(enumObject, Nan::New<String>("deviceInstanceId").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiEndpointDeviceInformationSortOrder::DeviceInstanceId)));
    Nan::Set(enumObject, Nan::New<String>("containerThenName").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiEndpointDeviceInformationSortOrder::ContainerThenName)));
    Nan::Set(enumObject, Nan::New<String>("containerThenDeviceInstanceId").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiEndpointDeviceInformationSortOrder::ContainerThenDeviceInstanceId)));
  }

  static void InitMidiEndpointDevicePurposeEnum(const Local<Object> exports) {
    HandleScope scope;

    Local<Object> enumObject = Nan::New<Object>();

    Nan::Set(exports, Nan::New<String>("MidiEndpointDevicePurpose").ToLocalChecked(), enumObject);
    Nan::Set(enumObject, Nan::New<String>("normalMessageEndpoint").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiEndpointDevicePurpose::NormalMessageEndpoint)));
    Nan::Set(enumObject, Nan::New<String>("virtualDeviceResponder").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiEndpointDevicePurpose::VirtualDeviceResponder)));
    Nan::Set(enumObject, Nan::New<String>("inBoxGeneralMidiSynth").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiEndpointDevicePurpose::InBoxGeneralMidiSynth)));
    Nan::Set(enumObject, Nan::New<String>("diagnosticLoopback").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiEndpointDevicePurpose::DiagnosticLoopback)));
    Nan::Set(enumObject, Nan::New<String>("diagnosticPing").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiEndpointDevicePurpose::DiagnosticPing)));
  }

  static void InitMidiEndpointDiscoveryFilterFlagsEnum(const Local<Object> exports) {
    HandleScope scope;

    Local<Object> enumObject = Nan::New<Object>();

    Nan::Set(exports, Nan::New<String>("MidiEndpointDiscoveryFilterFlags").ToLocalChecked(), enumObject);
    Nan::Set(enumObject, Nan::New<String>("none").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiEndpointDiscoveryFilterFlags::None)));
    Nan::Set(enumObject, Nan::New<String>("requestEndpointInformation").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiEndpointDiscoveryFilterFlags::RequestEndpointInformation)));
    Nan::Set(enumObject, Nan::New<String>("requestDeviceIdentity").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiEndpointDiscoveryFilterFlags::RequestDeviceIdentity)));
    Nan::Set(enumObject, Nan::New<String>("requestEndpointName").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiEndpointDiscoveryFilterFlags::RequestEndpointName)));
    Nan::Set(enumObject, Nan::New<String>("requestProductInstanceId").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiEndpointDiscoveryFilterFlags::RequestProductInstanceId)));
    Nan::Set(enumObject, Nan::New<String>("requestStreamConfiguration").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiEndpointDiscoveryFilterFlags::RequestStreamConfiguration)));
  }

  static void InitMidiEndpointNativeDataFormatEnum(const Local<Object> exports) {
    HandleScope scope;

    Local<Object> enumObject = Nan::New<Object>();

    Nan::Set(exports, Nan::New<String>("MidiEndpointNativeDataFormat").ToLocalChecked(), enumObject);
    Nan::Set(enumObject, Nan::New<String>("unknown").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiEndpointNativeDataFormat::Unknown)));
    Nan::Set(enumObject, Nan::New<String>("byteStream").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiEndpointNativeDataFormat::ByteStream)));
    Nan::Set(enumObject, Nan::New<String>("universalMidiPacket").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiEndpointNativeDataFormat::UniversalMidiPacket)));
  }

  static void InitMidiFunctionBlockDirectionEnum(const Local<Object> exports) {
    HandleScope scope;

    Local<Object> enumObject = Nan::New<Object>();

    Nan::Set(exports, Nan::New<String>("MidiFunctionBlockDirection").ToLocalChecked(), enumObject);
    Nan::Set(enumObject, Nan::New<String>("undefined").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiFunctionBlockDirection::Undefined)));
    Nan::Set(enumObject, Nan::New<String>("blockInput").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiFunctionBlockDirection::BlockInput)));
    Nan::Set(enumObject, Nan::New<String>("blockOutput").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiFunctionBlockDirection::BlockOutput)));
    Nan::Set(enumObject, Nan::New<String>("bidirectional").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiFunctionBlockDirection::Bidirectional)));
  }

  static void InitMidiFunctionBlockDiscoveryFilterFlagsEnum(const Local<Object> exports) {
    HandleScope scope;

    Local<Object> enumObject = Nan::New<Object>();

    Nan::Set(exports, Nan::New<String>("MidiFunctionBlockDiscoveryFilterFlags").ToLocalChecked(), enumObject);
    Nan::Set(enumObject, Nan::New<String>("none").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiFunctionBlockDiscoveryFilterFlags::None)));
    Nan::Set(enumObject, Nan::New<String>("requestFunctionBlockInformation").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiFunctionBlockDiscoveryFilterFlags::RequestFunctionBlockInformation)));
    Nan::Set(enumObject, Nan::New<String>("requestFunctionBlockName").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiFunctionBlockDiscoveryFilterFlags::RequestFunctionBlockName)));
  }

  static void InitMidiFunctionBlockMidi10Enum(const Local<Object> exports) {
    HandleScope scope;

    Local<Object> enumObject = Nan::New<Object>();

    Nan::Set(exports, Nan::New<String>("MidiFunctionBlockMidi10").ToLocalChecked(), enumObject);
    Nan::Set(enumObject, Nan::New<String>("not10").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiFunctionBlockMidi10::Not10)));
    Nan::Set(enumObject, Nan::New<String>("yesBandwidthUnrestricted").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiFunctionBlockMidi10::YesBandwidthUnrestricted)));
    Nan::Set(enumObject, Nan::New<String>("yesBandwidthRestricted").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiFunctionBlockMidi10::YesBandwidthRestricted)));
    Nan::Set(enumObject, Nan::New<String>("reserved").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiFunctionBlockMidi10::Reserved)));
  }

  static void InitMidiFunctionBlockUIHintEnum(const Local<Object> exports) {
    HandleScope scope;

    Local<Object> enumObject = Nan::New<Object>();

    Nan::Set(exports, Nan::New<String>("MidiFunctionBlockUIHint").ToLocalChecked(), enumObject);
    Nan::Set(enumObject, Nan::New<String>("unknown").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiFunctionBlockUIHint::Unknown)));
    Nan::Set(enumObject, Nan::New<String>("receiver").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiFunctionBlockUIHint::Receiver)));
    Nan::Set(enumObject, Nan::New<String>("sender").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiFunctionBlockUIHint::Sender)));
    Nan::Set(enumObject, Nan::New<String>("bidirectional").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiFunctionBlockUIHint::Bidirectional)));
  }

  static void InitMidiGroupTerminalBlockDirectionEnum(const Local<Object> exports) {
    HandleScope scope;

    Local<Object> enumObject = Nan::New<Object>();

    Nan::Set(exports, Nan::New<String>("MidiGroupTerminalBlockDirection").ToLocalChecked(), enumObject);
    Nan::Set(enumObject, Nan::New<String>("bidirectional").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiGroupTerminalBlockDirection::Bidirectional)));
    Nan::Set(enumObject, Nan::New<String>("blockInput").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiGroupTerminalBlockDirection::BlockInput)));
    Nan::Set(enumObject, Nan::New<String>("blockOutput").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiGroupTerminalBlockDirection::BlockOutput)));
  }

  static void InitMidiGroupTerminalBlockProtocolEnum(const Local<Object> exports) {
    HandleScope scope;

    Local<Object> enumObject = Nan::New<Object>();

    Nan::Set(exports, Nan::New<String>("MidiGroupTerminalBlockProtocol").ToLocalChecked(), enumObject);
    Nan::Set(enumObject, Nan::New<String>("unknown").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiGroupTerminalBlockProtocol::Unknown)));
    Nan::Set(enumObject, Nan::New<String>("midi1Message64").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiGroupTerminalBlockProtocol::Midi1Message64)));
    Nan::Set(enumObject, Nan::New<String>("midi1Message64WithJitterReduction").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiGroupTerminalBlockProtocol::Midi1Message64WithJitterReduction)));
    Nan::Set(enumObject, Nan::New<String>("midi1Message128").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiGroupTerminalBlockProtocol::Midi1Message128)));
    Nan::Set(enumObject, Nan::New<String>("midi1Message128WithJitterReduction").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiGroupTerminalBlockProtocol::Midi1Message128WithJitterReduction)));
    Nan::Set(enumObject, Nan::New<String>("midi2").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiGroupTerminalBlockProtocol::Midi2)));
    Nan::Set(enumObject, Nan::New<String>("midi2WithJitterReduction").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiGroupTerminalBlockProtocol::Midi2WithJitterReduction)));
  }

  static void InitMidiMessageTypeEnum(const Local<Object> exports) {
    HandleScope scope;

    Local<Object> enumObject = Nan::New<Object>();

    Nan::Set(exports, Nan::New<String>("MidiMessageType").ToLocalChecked(), enumObject);
    Nan::Set(enumObject, Nan::New<String>("utilityMessage32").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiMessageType::UtilityMessage32)));
    Nan::Set(enumObject, Nan::New<String>("systemCommon32").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiMessageType::SystemCommon32)));
    Nan::Set(enumObject, Nan::New<String>("midi1ChannelVoice32").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiMessageType::Midi1ChannelVoice32)));
    Nan::Set(enumObject, Nan::New<String>("dataMessage64").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiMessageType::DataMessage64)));
    Nan::Set(enumObject, Nan::New<String>("midi2ChannelVoice64").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiMessageType::Midi2ChannelVoice64)));
    Nan::Set(enumObject, Nan::New<String>("dataMessage128").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiMessageType::DataMessage128)));
    Nan::Set(enumObject, Nan::New<String>("futureReserved632").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiMessageType::FutureReserved632)));
    Nan::Set(enumObject, Nan::New<String>("futureReserved732").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiMessageType::FutureReserved732)));
    Nan::Set(enumObject, Nan::New<String>("futureReserved864").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiMessageType::FutureReserved864)));
    Nan::Set(enumObject, Nan::New<String>("futureReserved964").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiMessageType::FutureReserved964)));
    Nan::Set(enumObject, Nan::New<String>("futureReservedA64").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiMessageType::FutureReservedA64)));
    Nan::Set(enumObject, Nan::New<String>("futureReservedB96").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiMessageType::FutureReservedB96)));
    Nan::Set(enumObject, Nan::New<String>("futureReservedC96").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiMessageType::FutureReservedC96)));
    Nan::Set(enumObject, Nan::New<String>("flexData128").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiMessageType::FlexData128)));
    Nan::Set(enumObject, Nan::New<String>("futureReservedE128").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiMessageType::FutureReservedE128)));
    Nan::Set(enumObject, Nan::New<String>("stream128").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiMessageType::Stream128)));
  }

  static void InitMidiPacketTypeEnum(const Local<Object> exports) {
    HandleScope scope;

    Local<Object> enumObject = Nan::New<Object>();

    Nan::Set(exports, Nan::New<String>("MidiPacketType").ToLocalChecked(), enumObject);
    Nan::Set(enumObject, Nan::New<String>("unknownOrInvalid").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiPacketType::UnknownOrInvalid)));
    Nan::Set(enumObject, Nan::New<String>("universalMidiPacket32").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiPacketType::UniversalMidiPacket32)));
    Nan::Set(enumObject, Nan::New<String>("universalMidiPacket64").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiPacketType::UniversalMidiPacket64)));
    Nan::Set(enumObject, Nan::New<String>("universalMidiPacket96").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiPacketType::UniversalMidiPacket96)));
    Nan::Set(enumObject, Nan::New<String>("universalMidiPacket128").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiPacketType::UniversalMidiPacket128)));
  }

  static void InitMidiProtocolEnum(const Local<Object> exports) {
    HandleScope scope;

    Local<Object> enumObject = Nan::New<Object>();

    Nan::Set(exports, Nan::New<String>("MidiProtocol").ToLocalChecked(), enumObject);
    Nan::Set(enumObject, Nan::New<String>("default").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiProtocol::Default)));
    Nan::Set(enumObject, Nan::New<String>("midi1").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiProtocol::Midi1)));
    Nan::Set(enumObject, Nan::New<String>("midi2").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiProtocol::Midi2)));
  }

  static void InitMidiSendMessageResultEnum(const Local<Object> exports) {
    HandleScope scope;

    Local<Object> enumObject = Nan::New<Object>();

    Nan::Set(exports, Nan::New<String>("MidiSendMessageResult").ToLocalChecked(), enumObject);
    Nan::Set(enumObject, Nan::New<String>("bufferFull").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiSendMessageResult::BufferFull)));
    Nan::Set(enumObject, Nan::New<String>("endpointConnectionClosedOrInvalid").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiSendMessageResult::EndpointConnectionClosedOrInvalid)));
    Nan::Set(enumObject, Nan::New<String>("invalidMessageTypeForWordCount").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiSendMessageResult::InvalidMessageTypeForWordCount)));
    Nan::Set(enumObject, Nan::New<String>("invalidMessageOther").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiSendMessageResult::InvalidMessageOther)));
    Nan::Set(enumObject, Nan::New<String>("dataIndexOutOfRange").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiSendMessageResult::DataIndexOutOfRange)));
    Nan::Set(enumObject, Nan::New<String>("timestampOutOfRange").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiSendMessageResult::TimestampOutOfRange)));
    Nan::Set(enumObject, Nan::New<String>("other").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiSendMessageResult::Other)));
    Nan::Set(enumObject, Nan::New<String>("failed").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiSendMessageResult::Failed)));
    Nan::Set(enumObject, Nan::New<String>("succeeded").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiSendMessageResult::Succeeded)));
  }

  static void InitMidiSystemExclusive8StatusEnum(const Local<Object> exports) {
    HandleScope scope;

    Local<Object> enumObject = Nan::New<Object>();

    Nan::Set(exports, Nan::New<String>("MidiSystemExclusive8Status").ToLocalChecked(), enumObject);
    Nan::Set(enumObject, Nan::New<String>("completeMessageInSingleUmp").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiSystemExclusive8Status::CompleteMessageInSingleUmp)));
    Nan::Set(enumObject, Nan::New<String>("startUmp").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiSystemExclusive8Status::StartUmp)));
    Nan::Set(enumObject, Nan::New<String>("continueUmp").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiSystemExclusive8Status::ContinueUmp)));
    Nan::Set(enumObject, Nan::New<String>("endUmp").ToLocalChecked(), Nan::New<Integer>(static_cast<int>(::Windows::Devices::Midi2::MidiSystemExclusive8Status::EndUmp)));
  }

  static bool IsMidiMessageStructJsObject(Local<Value> value) {
    if (!value->IsObject()) {
      return false;
    }

    Local<String> symbol;
    Local<Object> obj = Nan::To<Object>(value).ToLocalChecked();

    symbol = Nan::New<String>("word0").ToLocalChecked();
    if (Nan::Has(obj, symbol).FromMaybe(false)) {
      if (!Nan::Get(obj,symbol).ToLocalChecked()->IsUint32()) {
        return false;
      }
    }
    
    symbol = Nan::New<String>("word1").ToLocalChecked();
    if (Nan::Has(obj, symbol).FromMaybe(false)) {
      if (!Nan::Get(obj,symbol).ToLocalChecked()->IsUint32()) {
        return false;
      }
    }
    
    symbol = Nan::New<String>("word2").ToLocalChecked();
    if (Nan::Has(obj, symbol).FromMaybe(false)) {
      if (!Nan::Get(obj,symbol).ToLocalChecked()->IsUint32()) {
        return false;
      }
    }
    
    symbol = Nan::New<String>("word3").ToLocalChecked();
    if (Nan::Has(obj, symbol).FromMaybe(false)) {
      if (!Nan::Get(obj,symbol).ToLocalChecked()->IsUint32()) {
        return false;
      }
    }
    
    return true;
  }

  ::Windows::Devices::Midi2::MidiMessageStruct MidiMessageStructFromJsObject(Local<Value> value) {
    HandleScope scope;
    ::Windows::Devices::Midi2::MidiMessageStruct returnValue;

    if (!value->IsObject()) {
      Nan::ThrowError(Nan::TypeError(NodeRT::Utils::NewString(L"Unexpected type, expected an object")));
      return returnValue;
    }

    Local<Object> obj = Nan::To<Object>(value).ToLocalChecked();
    Local<String> symbol;

    symbol = Nan::New<String>("word0").ToLocalChecked();
    if (Nan::Has(obj, symbol).FromMaybe(false)) {
      returnValue.Word0 = static_cast<unsigned int>(Nan::To<uint32_t>(Nan::Get(obj,symbol).ToLocalChecked()).FromMaybe(0));
    }
    
    symbol = Nan::New<String>("word1").ToLocalChecked();
    if (Nan::Has(obj, symbol).FromMaybe(false)) {
      returnValue.Word1 = static_cast<unsigned int>(Nan::To<uint32_t>(Nan::Get(obj,symbol).ToLocalChecked()).FromMaybe(0));
    }
    
    symbol = Nan::New<String>("word2").ToLocalChecked();
    if (Nan::Has(obj, symbol).FromMaybe(false)) {
      returnValue.Word2 = static_cast<unsigned int>(Nan::To<uint32_t>(Nan::Get(obj,symbol).ToLocalChecked()).FromMaybe(0));
    }
    
    symbol = Nan::New<String>("word3").ToLocalChecked();
    if (Nan::Has(obj, symbol).FromMaybe(false)) {
      returnValue.Word3 = static_cast<unsigned int>(Nan::To<uint32_t>(Nan::Get(obj,symbol).ToLocalChecked()).FromMaybe(0));
    }
    
    return returnValue;
  }

  Local<Value> MidiMessageStructToJsObject(::Windows::Devices::Midi2::MidiMessageStruct value) {
    EscapableHandleScope scope;

    Local<Object> obj = Nan::New<Object>();

    Nan::Set(obj, Nan::New<String>("word0").ToLocalChecked(), Nan::New<Integer>(value.Word0));
    Nan::Set(obj, Nan::New<String>("word1").ToLocalChecked(), Nan::New<Integer>(value.Word1));
    Nan::Set(obj, Nan::New<String>("word2").ToLocalChecked(), Nan::New<Integer>(value.Word2));
    Nan::Set(obj, Nan::New<String>("word3").ToLocalChecked(), Nan::New<Integer>(value.Word3));

    return scope.Escape(obj);
  }


  class IMidiEndpointConnectionSource : public WrapperBase {
    public:
      
      static void Init(const Local<Object> exports) {
        HandleScope scope;

        Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(New);
        s_constructorTemplate.Reset(localRef);
        localRef->SetClassName(Nan::New<String>("IMidiEndpointConnectionSource").ToLocalChecked());
        localRef->InstanceTemplate()->SetInternalFieldCount(1);






        Local<Object> constructor = Nan::To<Object>(Nan::GetFunction(localRef).ToLocalChecked()).ToLocalChecked();
        Nan::SetMethod(constructor, "castFrom", CastFrom);



        Nan::Set(exports, Nan::New<String>("IMidiEndpointConnectionSource").ToLocalChecked(), constructor);
      }

      virtual ::Platform::Object^ GetObjectInstance() const override {
        return _instance;
      }

    private:

      IMidiEndpointConnectionSource(::Windows::Devices::Midi2::IMidiEndpointConnectionSource^ instance) {
        _instance = instance;
      }

      
    static void New(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(s_constructorTemplate);

      // in case the constructor was called without the new operator
      if (!localRef->HasInstance(info.This())) {
        if (info.Length() > 0) {
          std::unique_ptr<Local<Value> []> constructorArgs(new Local<Value>[info.Length()]);

          Local<Value> *argsPtr = constructorArgs.get();
          for (int i = 0; i < info.Length(); i++) {
            argsPtr[i] = info[i];
          }

          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), constructorArgs.get());
          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        } else {
          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), nullptr);

          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        }
      }

      ::Windows::Devices::Midi2::IMidiEndpointConnectionSource^ winRtInstance;


      if (info.Length() == 1 && OpaqueWrapper::IsOpaqueWrapper(info[0]) &&
        NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::IMidiEndpointConnectionSource^>(info[0])) {
        try {
          winRtInstance = (::Windows::Devices::Midi2::IMidiEndpointConnectionSource^) NodeRT::Utils::GetObjectInstance(info[0]);
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no suitable constructor found")));
        return;
      }

      NodeRT::Utils::SetHiddenValue(info.This(), Nan::New<String>("__winRtInstance__").ToLocalChecked(), True());

      IMidiEndpointConnectionSource *wrapperInstance = new IMidiEndpointConnectionSource(winRtInstance);
      wrapperInstance->Wrap(info.This());

      info.GetReturnValue().Set(info.This());
    }


      
    static void CastFrom(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;
      if (info.Length() < 1 || !NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::IMidiEndpointConnectionSource^>(info[0])) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no object provided, or given object could not be casted to requested type")));
        return;
      }

      ::Windows::Devices::Midi2::IMidiEndpointConnectionSource^ winRtInstance;
      try {
        winRtInstance = (::Windows::Devices::Midi2::IMidiEndpointConnectionSource^) NodeRT::Utils::GetObjectInstance(info[0]);
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }

      info.GetReturnValue().Set(WrapIMidiEndpointConnectionSource(winRtInstance));
    }







    private:
      ::Windows::Devices::Midi2::IMidiEndpointConnectionSource^ _instance;
      static Persistent<FunctionTemplate> s_constructorTemplate;

      friend v8::Local<v8::Value> WrapIMidiEndpointConnectionSource(::Windows::Devices::Midi2::IMidiEndpointConnectionSource^ wintRtInstance);
      friend ::Windows::Devices::Midi2::IMidiEndpointConnectionSource^ UnwrapIMidiEndpointConnectionSource(Local<Value> value);
  };

  Persistent<FunctionTemplate> IMidiEndpointConnectionSource::s_constructorTemplate;

  v8::Local<v8::Value> WrapIMidiEndpointConnectionSource(::Windows::Devices::Midi2::IMidiEndpointConnectionSource^ winRtInstance) {
    EscapableHandleScope scope;

    if (winRtInstance == nullptr) {
      return scope.Escape(Undefined());
    }

    Local<Value> opaqueWrapper = CreateOpaqueWrapper(winRtInstance);
    Local<Value> args[] = {opaqueWrapper};
    Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(IMidiEndpointConnectionSource::s_constructorTemplate);
    return scope.Escape(Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(),_countof(args), args).ToLocalChecked());
  }

  ::Windows::Devices::Midi2::IMidiEndpointConnectionSource^ UnwrapIMidiEndpointConnectionSource(Local<Value> value) {
     return IMidiEndpointConnectionSource::Unwrap<IMidiEndpointConnectionSource>(Nan::To<Object>(value).ToLocalChecked())->_instance;
  }

  void InitIMidiEndpointConnectionSource(Local<Object> exports) {
    IMidiEndpointConnectionSource::Init(exports);
  }

  class IMidiEndpointDefinedConnectionSettings : public WrapperBase {
    public:
      
      static void Init(const Local<Object> exports) {
        HandleScope scope;

        Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(New);
        s_constructorTemplate.Reset(localRef);
        localRef->SetClassName(Nan::New<String>("IMidiEndpointDefinedConnectionSettings").ToLocalChecked());
        localRef->InstanceTemplate()->SetInternalFieldCount(1);





          
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("isDirty").ToLocalChecked(), IsDirtyGetter, IsDirtySetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("settingsJson").ToLocalChecked(), SettingsJsonGetter, SettingsJsonSetter);

        Local<Object> constructor = Nan::To<Object>(Nan::GetFunction(localRef).ToLocalChecked()).ToLocalChecked();
        Nan::SetMethod(constructor, "castFrom", CastFrom);



        Nan::Set(exports, Nan::New<String>("IMidiEndpointDefinedConnectionSettings").ToLocalChecked(), constructor);
      }

      virtual ::Platform::Object^ GetObjectInstance() const override {
        return _instance;
      }

    private:

      IMidiEndpointDefinedConnectionSettings(::Windows::Devices::Midi2::IMidiEndpointDefinedConnectionSettings^ instance) {
        _instance = instance;
      }

      
    static void New(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(s_constructorTemplate);

      // in case the constructor was called without the new operator
      if (!localRef->HasInstance(info.This())) {
        if (info.Length() > 0) {
          std::unique_ptr<Local<Value> []> constructorArgs(new Local<Value>[info.Length()]);

          Local<Value> *argsPtr = constructorArgs.get();
          for (int i = 0; i < info.Length(); i++) {
            argsPtr[i] = info[i];
          }

          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), constructorArgs.get());
          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        } else {
          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), nullptr);

          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        }
      }

      ::Windows::Devices::Midi2::IMidiEndpointDefinedConnectionSettings^ winRtInstance;


      if (info.Length() == 1 && OpaqueWrapper::IsOpaqueWrapper(info[0]) &&
        NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::IMidiEndpointDefinedConnectionSettings^>(info[0])) {
        try {
          winRtInstance = (::Windows::Devices::Midi2::IMidiEndpointDefinedConnectionSettings^) NodeRT::Utils::GetObjectInstance(info[0]);
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no suitable constructor found")));
        return;
      }

      NodeRT::Utils::SetHiddenValue(info.This(), Nan::New<String>("__winRtInstance__").ToLocalChecked(), True());

      IMidiEndpointDefinedConnectionSettings *wrapperInstance = new IMidiEndpointDefinedConnectionSettings(winRtInstance);
      wrapperInstance->Wrap(info.This());

      info.GetReturnValue().Set(info.This());
    }


      
    static void CastFrom(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;
      if (info.Length() < 1 || !NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::IMidiEndpointDefinedConnectionSettings^>(info[0])) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no object provided, or given object could not be casted to requested type")));
        return;
      }

      ::Windows::Devices::Midi2::IMidiEndpointDefinedConnectionSettings^ winRtInstance;
      try {
        winRtInstance = (::Windows::Devices::Midi2::IMidiEndpointDefinedConnectionSettings^) NodeRT::Utils::GetObjectInstance(info[0]);
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }

      info.GetReturnValue().Set(WrapIMidiEndpointDefinedConnectionSettings(winRtInstance));
    }





    static void IsDirtyGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::IMidiEndpointDefinedConnectionSettings^>(info.This())) {
        return;
      }

      IMidiEndpointDefinedConnectionSettings *wrapper = IMidiEndpointDefinedConnectionSettings::Unwrap<IMidiEndpointDefinedConnectionSettings>(info.This());

      try  {
        bool result = wrapper->_instance->IsDirty;
        info.GetReturnValue().Set(Nan::New<Boolean>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void IsDirtySetter(Local<String> property, Local<Value> value, const Nan::PropertyCallbackInfo<void> &info) {
      HandleScope scope;

      if (!value->IsBoolean()) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Value to set is of unexpected type")));
        return;
      }

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::IMidiEndpointDefinedConnectionSettings^>(info.This())) {
        return;
      }

      IMidiEndpointDefinedConnectionSettings *wrapper = IMidiEndpointDefinedConnectionSettings::Unwrap<IMidiEndpointDefinedConnectionSettings>(info.This());

      try {

        bool winRtValue = Nan::To<bool>(value).FromMaybe(false);

        wrapper->_instance->IsDirty = winRtValue;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
      }
    }
      
    static void SettingsJsonGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::IMidiEndpointDefinedConnectionSettings^>(info.This())) {
        return;
      }

      IMidiEndpointDefinedConnectionSettings *wrapper = IMidiEndpointDefinedConnectionSettings::Unwrap<IMidiEndpointDefinedConnectionSettings>(info.This());

      try  {
        Platform::String^ result = wrapper->_instance->SettingsJson;
        info.GetReturnValue().Set(NodeRT::Utils::NewString(result->Data()));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void SettingsJsonSetter(Local<String> property, Local<Value> value, const Nan::PropertyCallbackInfo<void> &info) {
      HandleScope scope;

      if (!value->IsString()) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Value to set is of unexpected type")));
        return;
      }

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::IMidiEndpointDefinedConnectionSettings^>(info.This())) {
        return;
      }

      IMidiEndpointDefinedConnectionSettings *wrapper = IMidiEndpointDefinedConnectionSettings::Unwrap<IMidiEndpointDefinedConnectionSettings>(info.This());

      try {

        Platform::String^ winRtValue = ref new Platform::String(NodeRT::Utils::StringToWchar(v8::String::Value(v8::Isolate::GetCurrent(), value)));

        wrapper->_instance->SettingsJson = winRtValue;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
      }
    }
      


    private:
      ::Windows::Devices::Midi2::IMidiEndpointDefinedConnectionSettings^ _instance;
      static Persistent<FunctionTemplate> s_constructorTemplate;

      friend v8::Local<v8::Value> WrapIMidiEndpointDefinedConnectionSettings(::Windows::Devices::Midi2::IMidiEndpointDefinedConnectionSettings^ wintRtInstance);
      friend ::Windows::Devices::Midi2::IMidiEndpointDefinedConnectionSettings^ UnwrapIMidiEndpointDefinedConnectionSettings(Local<Value> value);
  };

  Persistent<FunctionTemplate> IMidiEndpointDefinedConnectionSettings::s_constructorTemplate;

  v8::Local<v8::Value> WrapIMidiEndpointDefinedConnectionSettings(::Windows::Devices::Midi2::IMidiEndpointDefinedConnectionSettings^ winRtInstance) {
    EscapableHandleScope scope;

    if (winRtInstance == nullptr) {
      return scope.Escape(Undefined());
    }

    Local<Value> opaqueWrapper = CreateOpaqueWrapper(winRtInstance);
    Local<Value> args[] = {opaqueWrapper};
    Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(IMidiEndpointDefinedConnectionSettings::s_constructorTemplate);
    return scope.Escape(Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(),_countof(args), args).ToLocalChecked());
  }

  ::Windows::Devices::Midi2::IMidiEndpointDefinedConnectionSettings^ UnwrapIMidiEndpointDefinedConnectionSettings(Local<Value> value) {
     return IMidiEndpointDefinedConnectionSettings::Unwrap<IMidiEndpointDefinedConnectionSettings>(Nan::To<Object>(value).ToLocalChecked())->_instance;
  }

  void InitIMidiEndpointDefinedConnectionSettings(Local<Object> exports) {
    IMidiEndpointDefinedConnectionSettings::Init(exports);
  }

  class IMidiEndpointMessageProcessingPlugin : public WrapperBase {
    public:
      
      static void Init(const Local<Object> exports) {
        HandleScope scope;

        Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(New);
        s_constructorTemplate.Reset(localRef);
        localRef->SetClassName(Nan::New<String>("IMidiEndpointMessageProcessingPlugin").ToLocalChecked());
        localRef->InstanceTemplate()->SetInternalFieldCount(1);


          
            Nan::SetPrototypeMethod(localRef, "initialize", Initialize);
            Nan::SetPrototypeMethod(localRef, "onEndpointConnectionOpened", OnEndpointConnectionOpened);
            Nan::SetPrototypeMethod(localRef, "processIncomingMessage", ProcessIncomingMessage);
            Nan::SetPrototypeMethod(localRef, "cleanup", Cleanup);
          



          
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("id").ToLocalChecked(), IdGetter, IdSetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("isEnabled").ToLocalChecked(), IsEnabledGetter, IsEnabledSetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("name").ToLocalChecked(), NameGetter, NameSetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("tag").ToLocalChecked(), TagGetter, TagSetter);

        Local<Object> constructor = Nan::To<Object>(Nan::GetFunction(localRef).ToLocalChecked()).ToLocalChecked();
        Nan::SetMethod(constructor, "castFrom", CastFrom);



        Nan::Set(exports, Nan::New<String>("IMidiEndpointMessageProcessingPlugin").ToLocalChecked(), constructor);
      }

      virtual ::Platform::Object^ GetObjectInstance() const override {
        return _instance;
      }

    private:

      IMidiEndpointMessageProcessingPlugin(::Windows::Devices::Midi2::IMidiEndpointMessageProcessingPlugin^ instance) {
        _instance = instance;
      }

      
    static void New(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(s_constructorTemplate);

      // in case the constructor was called without the new operator
      if (!localRef->HasInstance(info.This())) {
        if (info.Length() > 0) {
          std::unique_ptr<Local<Value> []> constructorArgs(new Local<Value>[info.Length()]);

          Local<Value> *argsPtr = constructorArgs.get();
          for (int i = 0; i < info.Length(); i++) {
            argsPtr[i] = info[i];
          }

          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), constructorArgs.get());
          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        } else {
          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), nullptr);

          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        }
      }

      ::Windows::Devices::Midi2::IMidiEndpointMessageProcessingPlugin^ winRtInstance;


      if (info.Length() == 1 && OpaqueWrapper::IsOpaqueWrapper(info[0]) &&
        NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::IMidiEndpointMessageProcessingPlugin^>(info[0])) {
        try {
          winRtInstance = (::Windows::Devices::Midi2::IMidiEndpointMessageProcessingPlugin^) NodeRT::Utils::GetObjectInstance(info[0]);
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no suitable constructor found")));
        return;
      }

      NodeRT::Utils::SetHiddenValue(info.This(), Nan::New<String>("__winRtInstance__").ToLocalChecked(), True());

      IMidiEndpointMessageProcessingPlugin *wrapperInstance = new IMidiEndpointMessageProcessingPlugin(winRtInstance);
      wrapperInstance->Wrap(info.This());

      info.GetReturnValue().Set(info.This());
    }


      
    static void CastFrom(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;
      if (info.Length() < 1 || !NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::IMidiEndpointMessageProcessingPlugin^>(info[0])) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no object provided, or given object could not be casted to requested type")));
        return;
      }

      ::Windows::Devices::Midi2::IMidiEndpointMessageProcessingPlugin^ winRtInstance;
      try {
        winRtInstance = (::Windows::Devices::Midi2::IMidiEndpointMessageProcessingPlugin^) NodeRT::Utils::GetObjectInstance(info[0]);
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }

      info.GetReturnValue().Set(WrapIMidiEndpointMessageProcessingPlugin(winRtInstance));
    }


    static void Initialize(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::IMidiEndpointMessageProcessingPlugin^>(info.This())) {
        return;
      }

      IMidiEndpointMessageProcessingPlugin *wrapper = IMidiEndpointMessageProcessingPlugin::Unwrap<IMidiEndpointMessageProcessingPlugin>(info.This());

      if (info.Length() == 1
        && NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::IMidiEndpointConnectionSource^>(info[0]))
      {
        try
        {
          ::Windows::Devices::Midi2::IMidiEndpointConnectionSource^ arg0 = UnwrapIMidiEndpointConnectionSource(info[0]);
          
          wrapper->_instance->Initialize(arg0);
          return;
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }
    static void OnEndpointConnectionOpened(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::IMidiEndpointMessageProcessingPlugin^>(info.This())) {
        return;
      }

      IMidiEndpointMessageProcessingPlugin *wrapper = IMidiEndpointMessageProcessingPlugin::Unwrap<IMidiEndpointMessageProcessingPlugin>(info.This());

      if (info.Length() == 0)
      {
        try
        {
          wrapper->_instance->OnEndpointConnectionOpened();
          return;
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }
    static void ProcessIncomingMessage(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::IMidiEndpointMessageProcessingPlugin^>(info.This())) {
        return;
      }

      IMidiEndpointMessageProcessingPlugin *wrapper = IMidiEndpointMessageProcessingPlugin::Unwrap<IMidiEndpointMessageProcessingPlugin>(info.This());

      if (info.Length() == 1
        && NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessageReceivedEventArgs^>(info[0]))
      {
        try
        {
          ::Windows::Devices::Midi2::MidiMessageReceivedEventArgs^ arg0 = UnwrapMidiMessageReceivedEventArgs(info[0]);
          bool arg1;
          bool arg2;
          
          wrapper->_instance->ProcessIncomingMessage(arg0, &arg1, &arg2);
          Local<Object> resObj = Nan::New<Object>();
          Nan::Set(resObj, Nan::New<String>("skipFurtherListeners").ToLocalChecked(), Nan::New<Boolean>(arg1));
          Nan::Set(resObj, Nan::New<String>("skipMainMessageReceivedEvent").ToLocalChecked(), Nan::New<Boolean>(arg2));
          info.GetReturnValue().Set(resObj);
          return;
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }
    static void Cleanup(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::IMidiEndpointMessageProcessingPlugin^>(info.This())) {
        return;
      }

      IMidiEndpointMessageProcessingPlugin *wrapper = IMidiEndpointMessageProcessingPlugin::Unwrap<IMidiEndpointMessageProcessingPlugin>(info.This());

      if (info.Length() == 0)
      {
        try
        {
          wrapper->_instance->Cleanup();
          return;
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }



    static void IdGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::IMidiEndpointMessageProcessingPlugin^>(info.This())) {
        return;
      }

      IMidiEndpointMessageProcessingPlugin *wrapper = IMidiEndpointMessageProcessingPlugin::Unwrap<IMidiEndpointMessageProcessingPlugin>(info.This());

      try  {
        Platform::String^ result = wrapper->_instance->Id;
        info.GetReturnValue().Set(NodeRT::Utils::NewString(result->Data()));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void IdSetter(Local<String> property, Local<Value> value, const Nan::PropertyCallbackInfo<void> &info) {
      HandleScope scope;

      if (!value->IsString()) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Value to set is of unexpected type")));
        return;
      }

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::IMidiEndpointMessageProcessingPlugin^>(info.This())) {
        return;
      }

      IMidiEndpointMessageProcessingPlugin *wrapper = IMidiEndpointMessageProcessingPlugin::Unwrap<IMidiEndpointMessageProcessingPlugin>(info.This());

      try {

        Platform::String^ winRtValue = ref new Platform::String(NodeRT::Utils::StringToWchar(v8::String::Value(v8::Isolate::GetCurrent(), value)));

        wrapper->_instance->Id = winRtValue;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
      }
    }
      
    static void IsEnabledGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::IMidiEndpointMessageProcessingPlugin^>(info.This())) {
        return;
      }

      IMidiEndpointMessageProcessingPlugin *wrapper = IMidiEndpointMessageProcessingPlugin::Unwrap<IMidiEndpointMessageProcessingPlugin>(info.This());

      try  {
        bool result = wrapper->_instance->IsEnabled;
        info.GetReturnValue().Set(Nan::New<Boolean>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void IsEnabledSetter(Local<String> property, Local<Value> value, const Nan::PropertyCallbackInfo<void> &info) {
      HandleScope scope;

      if (!value->IsBoolean()) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Value to set is of unexpected type")));
        return;
      }

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::IMidiEndpointMessageProcessingPlugin^>(info.This())) {
        return;
      }

      IMidiEndpointMessageProcessingPlugin *wrapper = IMidiEndpointMessageProcessingPlugin::Unwrap<IMidiEndpointMessageProcessingPlugin>(info.This());

      try {

        bool winRtValue = Nan::To<bool>(value).FromMaybe(false);

        wrapper->_instance->IsEnabled = winRtValue;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
      }
    }
      
    static void NameGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::IMidiEndpointMessageProcessingPlugin^>(info.This())) {
        return;
      }

      IMidiEndpointMessageProcessingPlugin *wrapper = IMidiEndpointMessageProcessingPlugin::Unwrap<IMidiEndpointMessageProcessingPlugin>(info.This());

      try  {
        Platform::String^ result = wrapper->_instance->Name;
        info.GetReturnValue().Set(NodeRT::Utils::NewString(result->Data()));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void NameSetter(Local<String> property, Local<Value> value, const Nan::PropertyCallbackInfo<void> &info) {
      HandleScope scope;

      if (!value->IsString()) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Value to set is of unexpected type")));
        return;
      }

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::IMidiEndpointMessageProcessingPlugin^>(info.This())) {
        return;
      }

      IMidiEndpointMessageProcessingPlugin *wrapper = IMidiEndpointMessageProcessingPlugin::Unwrap<IMidiEndpointMessageProcessingPlugin>(info.This());

      try {

        Platform::String^ winRtValue = ref new Platform::String(NodeRT::Utils::StringToWchar(v8::String::Value(v8::Isolate::GetCurrent(), value)));

        wrapper->_instance->Name = winRtValue;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
      }
    }
      
    static void TagGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::IMidiEndpointMessageProcessingPlugin^>(info.This())) {
        return;
      }

      IMidiEndpointMessageProcessingPlugin *wrapper = IMidiEndpointMessageProcessingPlugin::Unwrap<IMidiEndpointMessageProcessingPlugin>(info.This());

      try  {
        ::Platform::Object^ result = wrapper->_instance->Tag;
        info.GetReturnValue().Set(CreateOpaqueWrapper(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void TagSetter(Local<String> property, Local<Value> value, const Nan::PropertyCallbackInfo<void> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Platform::Object^>(value)) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Value to set is of unexpected type")));
        return;
      }

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::IMidiEndpointMessageProcessingPlugin^>(info.This())) {
        return;
      }

      IMidiEndpointMessageProcessingPlugin *wrapper = IMidiEndpointMessageProcessingPlugin::Unwrap<IMidiEndpointMessageProcessingPlugin>(info.This());

      try {

        ::Platform::Object^ winRtValue = dynamic_cast<::Platform::Object^>(NodeRT::Utils::GetObjectInstance(value));

        wrapper->_instance->Tag = winRtValue;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
      }
    }
      


    private:
      ::Windows::Devices::Midi2::IMidiEndpointMessageProcessingPlugin^ _instance;
      static Persistent<FunctionTemplate> s_constructorTemplate;

      friend v8::Local<v8::Value> WrapIMidiEndpointMessageProcessingPlugin(::Windows::Devices::Midi2::IMidiEndpointMessageProcessingPlugin^ wintRtInstance);
      friend ::Windows::Devices::Midi2::IMidiEndpointMessageProcessingPlugin^ UnwrapIMidiEndpointMessageProcessingPlugin(Local<Value> value);
  };

  Persistent<FunctionTemplate> IMidiEndpointMessageProcessingPlugin::s_constructorTemplate;

  v8::Local<v8::Value> WrapIMidiEndpointMessageProcessingPlugin(::Windows::Devices::Midi2::IMidiEndpointMessageProcessingPlugin^ winRtInstance) {
    EscapableHandleScope scope;

    if (winRtInstance == nullptr) {
      return scope.Escape(Undefined());
    }

    Local<Value> opaqueWrapper = CreateOpaqueWrapper(winRtInstance);
    Local<Value> args[] = {opaqueWrapper};
    Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(IMidiEndpointMessageProcessingPlugin::s_constructorTemplate);
    return scope.Escape(Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(),_countof(args), args).ToLocalChecked());
  }

  ::Windows::Devices::Midi2::IMidiEndpointMessageProcessingPlugin^ UnwrapIMidiEndpointMessageProcessingPlugin(Local<Value> value) {
     return IMidiEndpointMessageProcessingPlugin::Unwrap<IMidiEndpointMessageProcessingPlugin>(Nan::To<Object>(value).ToLocalChecked())->_instance;
  }

  void InitIMidiEndpointMessageProcessingPlugin(Local<Object> exports) {
    IMidiEndpointMessageProcessingPlugin::Init(exports);
  }

  class IMidiMessageReceivedEventSource : public WrapperBase {
    public:
      
      static void Init(const Local<Object> exports) {
        HandleScope scope;

        Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(New);
        s_constructorTemplate.Reset(localRef);
        localRef->SetClassName(Nan::New<String>("IMidiMessageReceivedEventSource").ToLocalChecked());
        localRef->InstanceTemplate()->SetInternalFieldCount(1);




          
          Nan::SetPrototypeMethod(localRef,"addListener", AddListener);
          Nan::SetPrototypeMethod(localRef,"on", AddListener);
          Nan::SetPrototypeMethod(localRef,"removeListener", RemoveListener);
          Nan::SetPrototypeMethod(localRef, "off", RemoveListener);


        Local<Object> constructor = Nan::To<Object>(Nan::GetFunction(localRef).ToLocalChecked()).ToLocalChecked();
        Nan::SetMethod(constructor, "castFrom", CastFrom);



        Nan::Set(exports, Nan::New<String>("IMidiMessageReceivedEventSource").ToLocalChecked(), constructor);
      }

      virtual ::Platform::Object^ GetObjectInstance() const override {
        return _instance;
      }

    private:

      IMidiMessageReceivedEventSource(::Windows::Devices::Midi2::IMidiMessageReceivedEventSource^ instance) {
        _instance = instance;
      }

      
    static void New(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(s_constructorTemplate);

      // in case the constructor was called without the new operator
      if (!localRef->HasInstance(info.This())) {
        if (info.Length() > 0) {
          std::unique_ptr<Local<Value> []> constructorArgs(new Local<Value>[info.Length()]);

          Local<Value> *argsPtr = constructorArgs.get();
          for (int i = 0; i < info.Length(); i++) {
            argsPtr[i] = info[i];
          }

          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), constructorArgs.get());
          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        } else {
          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), nullptr);

          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        }
      }

      ::Windows::Devices::Midi2::IMidiMessageReceivedEventSource^ winRtInstance;


      if (info.Length() == 1 && OpaqueWrapper::IsOpaqueWrapper(info[0]) &&
        NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::IMidiMessageReceivedEventSource^>(info[0])) {
        try {
          winRtInstance = (::Windows::Devices::Midi2::IMidiMessageReceivedEventSource^) NodeRT::Utils::GetObjectInstance(info[0]);
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no suitable constructor found")));
        return;
      }

      NodeRT::Utils::SetHiddenValue(info.This(), Nan::New<String>("__winRtInstance__").ToLocalChecked(), True());

      IMidiMessageReceivedEventSource *wrapperInstance = new IMidiMessageReceivedEventSource(winRtInstance);
      wrapperInstance->Wrap(info.This());

      info.GetReturnValue().Set(info.This());
    }


      
    static void CastFrom(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;
      if (info.Length() < 1 || !NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::IMidiMessageReceivedEventSource^>(info[0])) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no object provided, or given object could not be casted to requested type")));
        return;
      }

      ::Windows::Devices::Midi2::IMidiMessageReceivedEventSource^ winRtInstance;
      try {
        winRtInstance = (::Windows::Devices::Midi2::IMidiMessageReceivedEventSource^) NodeRT::Utils::GetObjectInstance(info[0]);
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }

      info.GetReturnValue().Set(WrapIMidiMessageReceivedEventSource(winRtInstance));
    }







    static void AddListener(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() < 2 || !info[0]->IsString() || !info[1]->IsFunction()) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"wrong arguments, expected arguments are eventName(string),callback(function)")));
        return;
      }

      String::Value eventName(v8::Isolate::GetCurrent(), info[0]);
      auto str = *eventName;

      Local<Function> callback = info[1].As<Function>();

      ::Windows::Foundation::EventRegistrationToken registrationToken;
      if (NodeRT::Utils::CaseInsenstiveEquals(L"messageReceived", str))
      {
        if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::IMidiMessageReceivedEventSource^>(info.This()))
        {
          Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"The caller of this method isn't of the expected type or internal WinRt object was disposed")));
      return;
        }
        IMidiMessageReceivedEventSource *wrapper = IMidiMessageReceivedEventSource::Unwrap<IMidiMessageReceivedEventSource>(info.This());
      
        try {
          Persistent<Object>* perstPtr = new Persistent<Object>();
          perstPtr->Reset(NodeRT::Utils::CreateCallbackObjectInDomain(callback));
          std::shared_ptr<Persistent<Object>> callbackObjPtr(perstPtr,
            [] (Persistent<Object> *ptr ) {
              NodeUtils::Async::RunOnMain([ptr]() {
                ptr->Reset();
                delete ptr;
            });
          });

          registrationToken = wrapper->_instance->MessageReceived::add(
            ref new ::Windows::Foundation::TypedEventHandler<::Windows::Devices::Midi2::IMidiMessageReceivedEventSource^, ::Windows::Devices::Midi2::MidiMessageReceivedEventArgs^>(
            [callbackObjPtr](::Windows::Devices::Midi2::IMidiMessageReceivedEventSource^ arg0, ::Windows::Devices::Midi2::MidiMessageReceivedEventArgs^ arg1) {
              NodeUtils::Async::RunOnMain([callbackObjPtr , arg0, arg1]() {
                HandleScope scope;


                Local<Value> wrappedArg0;
                Local<Value> wrappedArg1;

                {
                  TryCatch tryCatch;


                  wrappedArg0 = WrapIMidiMessageReceivedEventSource(arg0);
                  wrappedArg1 = WrapMidiMessageReceivedEventArgs(arg1);


                  if (wrappedArg0.IsEmpty()) wrappedArg0 = Undefined();
                  if (wrappedArg1.IsEmpty()) wrappedArg1 = Undefined();
                }

                Local<Value> args[] = { wrappedArg0, wrappedArg1 };
                Local<Object> callbackObjLocalRef = Nan::New<Object>(*callbackObjPtr);
                NodeRT::Utils::CallCallbackInDomain(callbackObjLocalRef, _countof(args), args);
              });
            })
          );
        }
        catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }

      }
 else  {
        Nan::ThrowError(Nan::Error(String::Concat(v8::Isolate::GetCurrent(), NodeRT::Utils::NewString(L"given event name isn't supported: "), info[0].As<String>())));
        return;
      }

      Local<Value> tokenMapVal = NodeRT::Utils::GetHiddenValue(callback, Nan::New<String>(REGISTRATION_TOKEN_MAP_PROPERTY_NAME).ToLocalChecked());
      Local<Object> tokenMap;

      if (tokenMapVal.IsEmpty() || Nan::Equals(tokenMapVal, Undefined()).FromMaybe(false)) {
        tokenMap = Nan::New<Object>();
        NodeRT::Utils::SetHiddenValueWithObject(callback, Nan::New<String>(REGISTRATION_TOKEN_MAP_PROPERTY_NAME).ToLocalChecked(), tokenMap);
      } else {
        tokenMap = Nan::To<Object>(tokenMapVal).ToLocalChecked();
      }

      Nan::Set(tokenMap, info[0], CreateOpaqueWrapper(::Windows::Foundation::PropertyValue::CreateInt64(registrationToken.Value)));
    }

    static void RemoveListener(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() < 2 || !info[0]->IsString() || !info[1]->IsFunction()) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"wrong arguments, expected a string and a callback")));
        return;
      }

      String::Value eventName(v8::Isolate::GetCurrent(), info[0]);
      auto str = *eventName;

      if ((!NodeRT::Utils::CaseInsenstiveEquals(L"messageReceived", str))) {
        Nan::ThrowError(Nan::Error(String::Concat(v8::Isolate::GetCurrent(), NodeRT::Utils::NewString(L"given event name isn't supported: "), info[0].As<String>())));
        return;
      }

      Local<Function> callback = info[1].As<Function>();
      Local<Value> tokenMap = NodeRT::Utils::GetHiddenValue(callback, Nan::New<String>(REGISTRATION_TOKEN_MAP_PROPERTY_NAME).ToLocalChecked());

      if (tokenMap.IsEmpty() || Nan::Equals(tokenMap, Undefined()).FromMaybe(false)) {
        return;
      }

      Local<Value> opaqueWrapperObj =  Nan::Get(Nan::To<Object>(tokenMap).ToLocalChecked(), info[0]).ToLocalChecked();

      if (opaqueWrapperObj.IsEmpty() || Nan::Equals(opaqueWrapperObj,Undefined()).FromMaybe(false)) {
        return;
      }

      OpaqueWrapper *opaqueWrapper = OpaqueWrapper::Unwrap<OpaqueWrapper>(opaqueWrapperObj.As<Object>());

      long long tokenValue = (long long) opaqueWrapper->GetObjectInstance();
      ::Windows::Foundation::EventRegistrationToken registrationToken;
      registrationToken.Value = tokenValue;

      try  {
        if (NodeRT::Utils::CaseInsenstiveEquals(L"messageReceived", str)) {
          if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::IMidiMessageReceivedEventSource^>(info.This()))
          {
            Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"The caller of this method isn't of the expected type or internal WinRt object was disposed")));
            return;
          }
          IMidiMessageReceivedEventSource *wrapper = IMidiMessageReceivedEventSource::Unwrap<IMidiMessageReceivedEventSource>(info.This());
          wrapper->_instance->MessageReceived::remove(registrationToken);
        }
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
      }

      Nan::Delete(Nan::To<Object>(tokenMap).ToLocalChecked(), Nan::To<String>(info[0]).ToLocalChecked());
    }
    private:
      ::Windows::Devices::Midi2::IMidiMessageReceivedEventSource^ _instance;
      static Persistent<FunctionTemplate> s_constructorTemplate;

      friend v8::Local<v8::Value> WrapIMidiMessageReceivedEventSource(::Windows::Devices::Midi2::IMidiMessageReceivedEventSource^ wintRtInstance);
      friend ::Windows::Devices::Midi2::IMidiMessageReceivedEventSource^ UnwrapIMidiMessageReceivedEventSource(Local<Value> value);
  };

  Persistent<FunctionTemplate> IMidiMessageReceivedEventSource::s_constructorTemplate;

  v8::Local<v8::Value> WrapIMidiMessageReceivedEventSource(::Windows::Devices::Midi2::IMidiMessageReceivedEventSource^ winRtInstance) {
    EscapableHandleScope scope;

    if (winRtInstance == nullptr) {
      return scope.Escape(Undefined());
    }

    Local<Value> opaqueWrapper = CreateOpaqueWrapper(winRtInstance);
    Local<Value> args[] = {opaqueWrapper};
    Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(IMidiMessageReceivedEventSource::s_constructorTemplate);
    return scope.Escape(Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(),_countof(args), args).ToLocalChecked());
  }

  ::Windows::Devices::Midi2::IMidiMessageReceivedEventSource^ UnwrapIMidiMessageReceivedEventSource(Local<Value> value) {
     return IMidiMessageReceivedEventSource::Unwrap<IMidiMessageReceivedEventSource>(Nan::To<Object>(value).ToLocalChecked())->_instance;
  }

  void InitIMidiMessageReceivedEventSource(Local<Object> exports) {
    IMidiMessageReceivedEventSource::Init(exports);
  }

  class IMidiTransportSettingsData : public WrapperBase {
    public:
      
      static void Init(const Local<Object> exports) {
        HandleScope scope;

        Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(New);
        s_constructorTemplate.Reset(localRef);
        localRef->SetClassName(Nan::New<String>("IMidiTransportSettingsData").ToLocalChecked());
        localRef->InstanceTemplate()->SetInternalFieldCount(1);





          
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("isDirty").ToLocalChecked(), IsDirtyGetter, IsDirtySetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("settingsJson").ToLocalChecked(), SettingsJsonGetter, SettingsJsonSetter);

        Local<Object> constructor = Nan::To<Object>(Nan::GetFunction(localRef).ToLocalChecked()).ToLocalChecked();
        Nan::SetMethod(constructor, "castFrom", CastFrom);



        Nan::Set(exports, Nan::New<String>("IMidiTransportSettingsData").ToLocalChecked(), constructor);
      }

      virtual ::Platform::Object^ GetObjectInstance() const override {
        return _instance;
      }

    private:

      IMidiTransportSettingsData(::Windows::Devices::Midi2::IMidiTransportSettingsData^ instance) {
        _instance = instance;
      }

      
    static void New(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(s_constructorTemplate);

      // in case the constructor was called without the new operator
      if (!localRef->HasInstance(info.This())) {
        if (info.Length() > 0) {
          std::unique_ptr<Local<Value> []> constructorArgs(new Local<Value>[info.Length()]);

          Local<Value> *argsPtr = constructorArgs.get();
          for (int i = 0; i < info.Length(); i++) {
            argsPtr[i] = info[i];
          }

          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), constructorArgs.get());
          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        } else {
          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), nullptr);

          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        }
      }

      ::Windows::Devices::Midi2::IMidiTransportSettingsData^ winRtInstance;


      if (info.Length() == 1 && OpaqueWrapper::IsOpaqueWrapper(info[0]) &&
        NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::IMidiTransportSettingsData^>(info[0])) {
        try {
          winRtInstance = (::Windows::Devices::Midi2::IMidiTransportSettingsData^) NodeRT::Utils::GetObjectInstance(info[0]);
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no suitable constructor found")));
        return;
      }

      NodeRT::Utils::SetHiddenValue(info.This(), Nan::New<String>("__winRtInstance__").ToLocalChecked(), True());

      IMidiTransportSettingsData *wrapperInstance = new IMidiTransportSettingsData(winRtInstance);
      wrapperInstance->Wrap(info.This());

      info.GetReturnValue().Set(info.This());
    }


      
    static void CastFrom(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;
      if (info.Length() < 1 || !NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::IMidiTransportSettingsData^>(info[0])) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no object provided, or given object could not be casted to requested type")));
        return;
      }

      ::Windows::Devices::Midi2::IMidiTransportSettingsData^ winRtInstance;
      try {
        winRtInstance = (::Windows::Devices::Midi2::IMidiTransportSettingsData^) NodeRT::Utils::GetObjectInstance(info[0]);
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }

      info.GetReturnValue().Set(WrapIMidiTransportSettingsData(winRtInstance));
    }





    static void IsDirtyGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::IMidiTransportSettingsData^>(info.This())) {
        return;
      }

      IMidiTransportSettingsData *wrapper = IMidiTransportSettingsData::Unwrap<IMidiTransportSettingsData>(info.This());

      try  {
        bool result = wrapper->_instance->IsDirty;
        info.GetReturnValue().Set(Nan::New<Boolean>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void IsDirtySetter(Local<String> property, Local<Value> value, const Nan::PropertyCallbackInfo<void> &info) {
      HandleScope scope;

      if (!value->IsBoolean()) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Value to set is of unexpected type")));
        return;
      }

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::IMidiTransportSettingsData^>(info.This())) {
        return;
      }

      IMidiTransportSettingsData *wrapper = IMidiTransportSettingsData::Unwrap<IMidiTransportSettingsData>(info.This());

      try {

        bool winRtValue = Nan::To<bool>(value).FromMaybe(false);

        wrapper->_instance->IsDirty = winRtValue;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
      }
    }
      
    static void SettingsJsonGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::IMidiTransportSettingsData^>(info.This())) {
        return;
      }

      IMidiTransportSettingsData *wrapper = IMidiTransportSettingsData::Unwrap<IMidiTransportSettingsData>(info.This());

      try  {
        Platform::String^ result = wrapper->_instance->SettingsJson;
        info.GetReturnValue().Set(NodeRT::Utils::NewString(result->Data()));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void SettingsJsonSetter(Local<String> property, Local<Value> value, const Nan::PropertyCallbackInfo<void> &info) {
      HandleScope scope;

      if (!value->IsString()) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Value to set is of unexpected type")));
        return;
      }

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::IMidiTransportSettingsData^>(info.This())) {
        return;
      }

      IMidiTransportSettingsData *wrapper = IMidiTransportSettingsData::Unwrap<IMidiTransportSettingsData>(info.This());

      try {

        Platform::String^ winRtValue = ref new Platform::String(NodeRT::Utils::StringToWchar(v8::String::Value(v8::Isolate::GetCurrent(), value)));

        wrapper->_instance->SettingsJson = winRtValue;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
      }
    }
      


    private:
      ::Windows::Devices::Midi2::IMidiTransportSettingsData^ _instance;
      static Persistent<FunctionTemplate> s_constructorTemplate;

      friend v8::Local<v8::Value> WrapIMidiTransportSettingsData(::Windows::Devices::Midi2::IMidiTransportSettingsData^ wintRtInstance);
      friend ::Windows::Devices::Midi2::IMidiTransportSettingsData^ UnwrapIMidiTransportSettingsData(Local<Value> value);
  };

  Persistent<FunctionTemplate> IMidiTransportSettingsData::s_constructorTemplate;

  v8::Local<v8::Value> WrapIMidiTransportSettingsData(::Windows::Devices::Midi2::IMidiTransportSettingsData^ winRtInstance) {
    EscapableHandleScope scope;

    if (winRtInstance == nullptr) {
      return scope.Escape(Undefined());
    }

    Local<Value> opaqueWrapper = CreateOpaqueWrapper(winRtInstance);
    Local<Value> args[] = {opaqueWrapper};
    Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(IMidiTransportSettingsData::s_constructorTemplate);
    return scope.Escape(Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(),_countof(args), args).ToLocalChecked());
  }

  ::Windows::Devices::Midi2::IMidiTransportSettingsData^ UnwrapIMidiTransportSettingsData(Local<Value> value) {
     return IMidiTransportSettingsData::Unwrap<IMidiTransportSettingsData>(Nan::To<Object>(value).ToLocalChecked())->_instance;
  }

  void InitIMidiTransportSettingsData(Local<Object> exports) {
    IMidiTransportSettingsData::Init(exports);
  }

  class IMidiUniversalPacket : public WrapperBase {
    public:
      
      static void Init(const Local<Object> exports) {
        HandleScope scope;

        Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(New);
        s_constructorTemplate.Reset(localRef);
        localRef->SetClassName(Nan::New<String>("IMidiUniversalPacket").ToLocalChecked());
        localRef->InstanceTemplate()->SetInternalFieldCount(1);


          
            Nan::SetPrototypeMethod(localRef, "peekFirstWord", PeekFirstWord);
          



          
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("messageType").ToLocalChecked(), MessageTypeGetter, MessageTypeSetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("packetType").ToLocalChecked(), PacketTypeGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("timestamp").ToLocalChecked(), TimestampGetter, TimestampSetter);

        Local<Object> constructor = Nan::To<Object>(Nan::GetFunction(localRef).ToLocalChecked()).ToLocalChecked();
        Nan::SetMethod(constructor, "castFrom", CastFrom);



        Nan::Set(exports, Nan::New<String>("IMidiUniversalPacket").ToLocalChecked(), constructor);
      }

      virtual ::Platform::Object^ GetObjectInstance() const override {
        return _instance;
      }

    private:

      IMidiUniversalPacket(::Windows::Devices::Midi2::IMidiUniversalPacket^ instance) {
        _instance = instance;
      }

      
    static void New(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(s_constructorTemplate);

      // in case the constructor was called without the new operator
      if (!localRef->HasInstance(info.This())) {
        if (info.Length() > 0) {
          std::unique_ptr<Local<Value> []> constructorArgs(new Local<Value>[info.Length()]);

          Local<Value> *argsPtr = constructorArgs.get();
          for (int i = 0; i < info.Length(); i++) {
            argsPtr[i] = info[i];
          }

          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), constructorArgs.get());
          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        } else {
          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), nullptr);

          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        }
      }

      ::Windows::Devices::Midi2::IMidiUniversalPacket^ winRtInstance;


      if (info.Length() == 1 && OpaqueWrapper::IsOpaqueWrapper(info[0]) &&
        NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::IMidiUniversalPacket^>(info[0])) {
        try {
          winRtInstance = (::Windows::Devices::Midi2::IMidiUniversalPacket^) NodeRT::Utils::GetObjectInstance(info[0]);
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no suitable constructor found")));
        return;
      }

      NodeRT::Utils::SetHiddenValue(info.This(), Nan::New<String>("__winRtInstance__").ToLocalChecked(), True());

      IMidiUniversalPacket *wrapperInstance = new IMidiUniversalPacket(winRtInstance);
      wrapperInstance->Wrap(info.This());

      info.GetReturnValue().Set(info.This());
    }


      
    static void CastFrom(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;
      if (info.Length() < 1 || !NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::IMidiUniversalPacket^>(info[0])) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no object provided, or given object could not be casted to requested type")));
        return;
      }

      ::Windows::Devices::Midi2::IMidiUniversalPacket^ winRtInstance;
      try {
        winRtInstance = (::Windows::Devices::Midi2::IMidiUniversalPacket^) NodeRT::Utils::GetObjectInstance(info[0]);
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }

      info.GetReturnValue().Set(WrapIMidiUniversalPacket(winRtInstance));
    }


    static void PeekFirstWord(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::IMidiUniversalPacket^>(info.This())) {
        return;
      }

      IMidiUniversalPacket *wrapper = IMidiUniversalPacket::Unwrap<IMidiUniversalPacket>(info.This());

      if (info.Length() == 0)
      {
        try
        {
          unsigned int result;
          result = wrapper->_instance->PeekFirstWord();
          info.GetReturnValue().Set(Nan::New<Integer>(result));
          return;
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }



    static void MessageTypeGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::IMidiUniversalPacket^>(info.This())) {
        return;
      }

      IMidiUniversalPacket *wrapper = IMidiUniversalPacket::Unwrap<IMidiUniversalPacket>(info.This());

      try  {
        ::Windows::Devices::Midi2::MidiMessageType result = wrapper->_instance->MessageType;
        info.GetReturnValue().Set(Nan::New<Integer>(static_cast<int>(result)));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void MessageTypeSetter(Local<String> property, Local<Value> value, const Nan::PropertyCallbackInfo<void> &info) {
      HandleScope scope;

      if (!value->IsInt32()) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Value to set is of unexpected type")));
        return;
      }

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::IMidiUniversalPacket^>(info.This())) {
        return;
      }

      IMidiUniversalPacket *wrapper = IMidiUniversalPacket::Unwrap<IMidiUniversalPacket>(info.This());

      try {

        ::Windows::Devices::Midi2::MidiMessageType winRtValue = static_cast<::Windows::Devices::Midi2::MidiMessageType>(Nan::To<int32_t>(value).FromMaybe(0));

        wrapper->_instance->MessageType = winRtValue;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
      }
    }
      
    static void PacketTypeGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::IMidiUniversalPacket^>(info.This())) {
        return;
      }

      IMidiUniversalPacket *wrapper = IMidiUniversalPacket::Unwrap<IMidiUniversalPacket>(info.This());

      try  {
        ::Windows::Devices::Midi2::MidiPacketType result = wrapper->_instance->PacketType;
        info.GetReturnValue().Set(Nan::New<Integer>(static_cast<int>(result)));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void TimestampGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::IMidiUniversalPacket^>(info.This())) {
        return;
      }

      IMidiUniversalPacket *wrapper = IMidiUniversalPacket::Unwrap<IMidiUniversalPacket>(info.This());

      try  {
        unsigned __int64 result = wrapper->_instance->Timestamp;
        info.GetReturnValue().Set(Nan::New<Number>(static_cast<double>(result)));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void TimestampSetter(Local<String> property, Local<Value> value, const Nan::PropertyCallbackInfo<void> &info) {
      HandleScope scope;

      if (!value->IsNumber()) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Value to set is of unexpected type")));
        return;
      }

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::IMidiUniversalPacket^>(info.This())) {
        return;
      }

      IMidiUniversalPacket *wrapper = IMidiUniversalPacket::Unwrap<IMidiUniversalPacket>(info.This());

      try {

        unsigned __int64 winRtValue = static_cast<unsigned __int64>(Nan::To<int64_t>(value).FromMaybe(0));

        wrapper->_instance->Timestamp = winRtValue;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
      }
    }
      


    private:
      ::Windows::Devices::Midi2::IMidiUniversalPacket^ _instance;
      static Persistent<FunctionTemplate> s_constructorTemplate;

      friend v8::Local<v8::Value> WrapIMidiUniversalPacket(::Windows::Devices::Midi2::IMidiUniversalPacket^ wintRtInstance);
      friend ::Windows::Devices::Midi2::IMidiUniversalPacket^ UnwrapIMidiUniversalPacket(Local<Value> value);
  };

  Persistent<FunctionTemplate> IMidiUniversalPacket::s_constructorTemplate;

  v8::Local<v8::Value> WrapIMidiUniversalPacket(::Windows::Devices::Midi2::IMidiUniversalPacket^ winRtInstance) {
    EscapableHandleScope scope;

    if (winRtInstance == nullptr) {
      return scope.Escape(Undefined());
    }

    Local<Value> opaqueWrapper = CreateOpaqueWrapper(winRtInstance);
    Local<Value> args[] = {opaqueWrapper};
    Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(IMidiUniversalPacket::s_constructorTemplate);
    return scope.Escape(Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(),_countof(args), args).ToLocalChecked());
  }

  ::Windows::Devices::Midi2::IMidiUniversalPacket^ UnwrapIMidiUniversalPacket(Local<Value> value) {
     return IMidiUniversalPacket::Unwrap<IMidiUniversalPacket>(Nan::To<Object>(value).ToLocalChecked())->_instance;
  }

  void InitIMidiUniversalPacket(Local<Object> exports) {
    IMidiUniversalPacket::Init(exports);
  }

  class MidiChannel : public WrapperBase {
    public:
      
      static void Init(const Local<Object> exports) {
        HandleScope scope;

        Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(New);
        s_constructorTemplate.Reset(localRef);
        localRef->SetClassName(Nan::New<String>("MidiChannel").ToLocalChecked());
        localRef->InstanceTemplate()->SetInternalFieldCount(1);





          
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("index").ToLocalChecked(), IndexGetter, IndexSetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("numberForDisplay").ToLocalChecked(), NumberForDisplayGetter);

        Local<Object> constructor = Nan::To<Object>(Nan::GetFunction(localRef).ToLocalChecked()).ToLocalChecked();
        Nan::SetMethod(constructor, "castFrom", CastFrom);

        Nan::SetMethod(constructor, "isValidChannelIndex", IsValidChannelIndex);
        Nan::SetAccessor(constructor, Nan::New<String>("labelFull").ToLocalChecked(), LabelFullGetter);
        Nan::SetAccessor(constructor, Nan::New<String>("labelShort").ToLocalChecked(), LabelShortGetter);


        Nan::Set(exports, Nan::New<String>("MidiChannel").ToLocalChecked(), constructor);
      }

      virtual ::Platform::Object^ GetObjectInstance() const override {
        return _instance;
      }

    private:

      MidiChannel(::Windows::Devices::Midi2::MidiChannel^ instance) {
        _instance = instance;
      }

      
    static void New(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(s_constructorTemplate);

      // in case the constructor was called without the new operator
      if (!localRef->HasInstance(info.This())) {
        if (info.Length() > 0) {
          std::unique_ptr<Local<Value> []> constructorArgs(new Local<Value>[info.Length()]);

          Local<Value> *argsPtr = constructorArgs.get();
          for (int i = 0; i < info.Length(); i++) {
            argsPtr[i] = info[i];
          }

          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), constructorArgs.get());
          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        } else {
          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), nullptr);

          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        }
      }

      ::Windows::Devices::Midi2::MidiChannel^ winRtInstance;


      if (info.Length() == 1 && OpaqueWrapper::IsOpaqueWrapper(info[0]) &&
        NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiChannel^>(info[0])) {
        try {
          winRtInstance = (::Windows::Devices::Midi2::MidiChannel^) NodeRT::Utils::GetObjectInstance(info[0]);
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
      else if (info.Length() == 1
        && info[0]->IsInt32())
      {
        try {
          unsigned char arg0 = static_cast<unsigned char>(Nan::To<int32_t>(info[0]).FromMaybe(0));
          
          winRtInstance = ref new ::Windows::Devices::Midi2::MidiChannel(arg0);
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
      else if (info.Length() == 0)
      {
        try {
          winRtInstance = ref new ::Windows::Devices::Midi2::MidiChannel();
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no suitable constructor found")));
        return;
      }

      NodeRT::Utils::SetHiddenValue(info.This(), Nan::New<String>("__winRtInstance__").ToLocalChecked(), True());

      MidiChannel *wrapperInstance = new MidiChannel(winRtInstance);
      wrapperInstance->Wrap(info.This());

      info.GetReturnValue().Set(info.This());
    }


      
    static void CastFrom(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;
      if (info.Length() < 1 || !NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiChannel^>(info[0])) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no object provided, or given object could not be casted to requested type")));
        return;
      }

      ::Windows::Devices::Midi2::MidiChannel^ winRtInstance;
      try {
        winRtInstance = (::Windows::Devices::Midi2::MidiChannel^) NodeRT::Utils::GetObjectInstance(info[0]);
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }

      info.GetReturnValue().Set(WrapMidiChannel(winRtInstance));
    }





    static void IsValidChannelIndex(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 1
        && info[0]->IsInt32())
      {
        try
        {
          unsigned char arg0 = static_cast<unsigned char>(Nan::To<int32_t>(info[0]).FromMaybe(0));
          
          bool result;
          result = ::Windows::Devices::Midi2::MidiChannel::IsValidChannelIndex(arg0);
          info.GetReturnValue().Set(Nan::New<Boolean>(result));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void IndexGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiChannel^>(info.This())) {
        return;
      }

      MidiChannel *wrapper = MidiChannel::Unwrap<MidiChannel>(info.This());

      try  {
        unsigned char result = wrapper->_instance->Index;
        info.GetReturnValue().Set(Nan::New<Integer>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void IndexSetter(Local<String> property, Local<Value> value, const Nan::PropertyCallbackInfo<void> &info) {
      HandleScope scope;

      if (!value->IsInt32()) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Value to set is of unexpected type")));
        return;
      }

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiChannel^>(info.This())) {
        return;
      }

      MidiChannel *wrapper = MidiChannel::Unwrap<MidiChannel>(info.This());

      try {

        unsigned char winRtValue = static_cast<unsigned char>(Nan::To<int32_t>(value).FromMaybe(0));

        wrapper->_instance->Index = winRtValue;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
      }
    }
      
    static void NumberForDisplayGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiChannel^>(info.This())) {
        return;
      }

      MidiChannel *wrapper = MidiChannel::Unwrap<MidiChannel>(info.This());

      try  {
        unsigned char result = wrapper->_instance->NumberForDisplay;
        info.GetReturnValue().Set(Nan::New<Integer>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      


    static void LabelFullGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      try
      {
        Platform::String^ result = ::Windows::Devices::Midi2::MidiChannel::LabelFull;
        info.GetReturnValue().Set(NodeRT::Utils::NewString(result->Data()));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      

    static void LabelShortGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      try
      {
        Platform::String^ result = ::Windows::Devices::Midi2::MidiChannel::LabelShort;
        info.GetReturnValue().Set(NodeRT::Utils::NewString(result->Data()));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      

    private:
      ::Windows::Devices::Midi2::MidiChannel^ _instance;
      static Persistent<FunctionTemplate> s_constructorTemplate;

      friend v8::Local<v8::Value> WrapMidiChannel(::Windows::Devices::Midi2::MidiChannel^ wintRtInstance);
      friend ::Windows::Devices::Midi2::MidiChannel^ UnwrapMidiChannel(Local<Value> value);
  };

  Persistent<FunctionTemplate> MidiChannel::s_constructorTemplate;

  v8::Local<v8::Value> WrapMidiChannel(::Windows::Devices::Midi2::MidiChannel^ winRtInstance) {
    EscapableHandleScope scope;

    if (winRtInstance == nullptr) {
      return scope.Escape(Undefined());
    }

    Local<Value> opaqueWrapper = CreateOpaqueWrapper(winRtInstance);
    Local<Value> args[] = {opaqueWrapper};
    Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(MidiChannel::s_constructorTemplate);
    return scope.Escape(Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(),_countof(args), args).ToLocalChecked());
  }

  ::Windows::Devices::Midi2::MidiChannel^ UnwrapMidiChannel(Local<Value> value) {
     return MidiChannel::Unwrap<MidiChannel>(Nan::To<Object>(value).ToLocalChecked())->_instance;
  }

  void InitMidiChannel(Local<Object> exports) {
    MidiChannel::Init(exports);
  }

  class MidiChannelEndpointListener : public WrapperBase {
    public:
      
      static void Init(const Local<Object> exports) {
        HandleScope scope;

        Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(New);
        s_constructorTemplate.Reset(localRef);
        localRef->SetClassName(Nan::New<String>("MidiChannelEndpointListener").ToLocalChecked());
        localRef->InstanceTemplate()->SetInternalFieldCount(1);


          
            Nan::SetPrototypeMethod(localRef, "initialize", Initialize);
            Nan::SetPrototypeMethod(localRef, "onEndpointConnectionOpened", OnEndpointConnectionOpened);
            Nan::SetPrototypeMethod(localRef, "processIncomingMessage", ProcessIncomingMessage);
            Nan::SetPrototypeMethod(localRef, "cleanup", Cleanup);
          


          
          Nan::SetPrototypeMethod(localRef,"addListener", AddListener);
          Nan::SetPrototypeMethod(localRef,"on", AddListener);
          Nan::SetPrototypeMethod(localRef,"removeListener", RemoveListener);
          Nan::SetPrototypeMethod(localRef, "off", RemoveListener);

          
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("preventFiringMainMessageReceivedEvent").ToLocalChecked(), PreventFiringMainMessageReceivedEventGetter, PreventFiringMainMessageReceivedEventSetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("preventCallingFurtherListeners").ToLocalChecked(), PreventCallingFurtherListenersGetter, PreventCallingFurtherListenersSetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("includeGroup").ToLocalChecked(), IncludeGroupGetter, IncludeGroupSetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("includeChannels").ToLocalChecked(), IncludeChannelsGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("tag").ToLocalChecked(), TagGetter, TagSetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("name").ToLocalChecked(), NameGetter, NameSetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("isEnabled").ToLocalChecked(), IsEnabledGetter, IsEnabledSetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("id").ToLocalChecked(), IdGetter, IdSetter);

        Local<Object> constructor = Nan::To<Object>(Nan::GetFunction(localRef).ToLocalChecked()).ToLocalChecked();
        Nan::SetMethod(constructor, "castFrom", CastFrom);



        Nan::Set(exports, Nan::New<String>("MidiChannelEndpointListener").ToLocalChecked(), constructor);
      }

      virtual ::Platform::Object^ GetObjectInstance() const override {
        return _instance;
      }

    private:

      MidiChannelEndpointListener(::Windows::Devices::Midi2::MidiChannelEndpointListener^ instance) {
        _instance = instance;
      }

      
    static void New(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(s_constructorTemplate);

      // in case the constructor was called without the new operator
      if (!localRef->HasInstance(info.This())) {
        if (info.Length() > 0) {
          std::unique_ptr<Local<Value> []> constructorArgs(new Local<Value>[info.Length()]);

          Local<Value> *argsPtr = constructorArgs.get();
          for (int i = 0; i < info.Length(); i++) {
            argsPtr[i] = info[i];
          }

          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), constructorArgs.get());
          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        } else {
          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), nullptr);

          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        }
      }

      ::Windows::Devices::Midi2::MidiChannelEndpointListener^ winRtInstance;


      if (info.Length() == 1 && OpaqueWrapper::IsOpaqueWrapper(info[0]) &&
        NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiChannelEndpointListener^>(info[0])) {
        try {
          winRtInstance = (::Windows::Devices::Midi2::MidiChannelEndpointListener^) NodeRT::Utils::GetObjectInstance(info[0]);
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
      else if (info.Length() == 0)
      {
        try {
          winRtInstance = ref new ::Windows::Devices::Midi2::MidiChannelEndpointListener();
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no suitable constructor found")));
        return;
      }

      NodeRT::Utils::SetHiddenValue(info.This(), Nan::New<String>("__winRtInstance__").ToLocalChecked(), True());

      MidiChannelEndpointListener *wrapperInstance = new MidiChannelEndpointListener(winRtInstance);
      wrapperInstance->Wrap(info.This());

      info.GetReturnValue().Set(info.This());
    }


      
    static void CastFrom(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;
      if (info.Length() < 1 || !NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiChannelEndpointListener^>(info[0])) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no object provided, or given object could not be casted to requested type")));
        return;
      }

      ::Windows::Devices::Midi2::MidiChannelEndpointListener^ winRtInstance;
      try {
        winRtInstance = (::Windows::Devices::Midi2::MidiChannelEndpointListener^) NodeRT::Utils::GetObjectInstance(info[0]);
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }

      info.GetReturnValue().Set(WrapMidiChannelEndpointListener(winRtInstance));
    }


    static void Initialize(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiChannelEndpointListener^>(info.This())) {
        return;
      }

      MidiChannelEndpointListener *wrapper = MidiChannelEndpointListener::Unwrap<MidiChannelEndpointListener>(info.This());

      if (info.Length() == 1
        && NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::IMidiEndpointConnectionSource^>(info[0]))
      {
        try
        {
          ::Windows::Devices::Midi2::IMidiEndpointConnectionSource^ arg0 = UnwrapIMidiEndpointConnectionSource(info[0]);
          
          wrapper->_instance->Initialize(arg0);
          return;
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }
    static void OnEndpointConnectionOpened(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiChannelEndpointListener^>(info.This())) {
        return;
      }

      MidiChannelEndpointListener *wrapper = MidiChannelEndpointListener::Unwrap<MidiChannelEndpointListener>(info.This());

      if (info.Length() == 0)
      {
        try
        {
          wrapper->_instance->OnEndpointConnectionOpened();
          return;
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }
    static void ProcessIncomingMessage(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiChannelEndpointListener^>(info.This())) {
        return;
      }

      MidiChannelEndpointListener *wrapper = MidiChannelEndpointListener::Unwrap<MidiChannelEndpointListener>(info.This());

      if (info.Length() == 1
        && NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessageReceivedEventArgs^>(info[0]))
      {
        try
        {
          ::Windows::Devices::Midi2::MidiMessageReceivedEventArgs^ arg0 = UnwrapMidiMessageReceivedEventArgs(info[0]);
          bool arg1;
          bool arg2;
          
          wrapper->_instance->ProcessIncomingMessage(arg0, &arg1, &arg2);
          Local<Object> resObj = Nan::New<Object>();
          Nan::Set(resObj, Nan::New<String>("skipFurtherListeners").ToLocalChecked(), Nan::New<Boolean>(arg1));
          Nan::Set(resObj, Nan::New<String>("skipMainMessageReceivedEvent").ToLocalChecked(), Nan::New<Boolean>(arg2));
          info.GetReturnValue().Set(resObj);
          return;
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }
    static void Cleanup(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiChannelEndpointListener^>(info.This())) {
        return;
      }

      MidiChannelEndpointListener *wrapper = MidiChannelEndpointListener::Unwrap<MidiChannelEndpointListener>(info.This());

      if (info.Length() == 0)
      {
        try
        {
          wrapper->_instance->Cleanup();
          return;
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }



    static void PreventFiringMainMessageReceivedEventGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiChannelEndpointListener^>(info.This())) {
        return;
      }

      MidiChannelEndpointListener *wrapper = MidiChannelEndpointListener::Unwrap<MidiChannelEndpointListener>(info.This());

      try  {
        bool result = wrapper->_instance->PreventFiringMainMessageReceivedEvent;
        info.GetReturnValue().Set(Nan::New<Boolean>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void PreventFiringMainMessageReceivedEventSetter(Local<String> property, Local<Value> value, const Nan::PropertyCallbackInfo<void> &info) {
      HandleScope scope;

      if (!value->IsBoolean()) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Value to set is of unexpected type")));
        return;
      }

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiChannelEndpointListener^>(info.This())) {
        return;
      }

      MidiChannelEndpointListener *wrapper = MidiChannelEndpointListener::Unwrap<MidiChannelEndpointListener>(info.This());

      try {

        bool winRtValue = Nan::To<bool>(value).FromMaybe(false);

        wrapper->_instance->PreventFiringMainMessageReceivedEvent = winRtValue;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
      }
    }
      
    static void PreventCallingFurtherListenersGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiChannelEndpointListener^>(info.This())) {
        return;
      }

      MidiChannelEndpointListener *wrapper = MidiChannelEndpointListener::Unwrap<MidiChannelEndpointListener>(info.This());

      try  {
        bool result = wrapper->_instance->PreventCallingFurtherListeners;
        info.GetReturnValue().Set(Nan::New<Boolean>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void PreventCallingFurtherListenersSetter(Local<String> property, Local<Value> value, const Nan::PropertyCallbackInfo<void> &info) {
      HandleScope scope;

      if (!value->IsBoolean()) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Value to set is of unexpected type")));
        return;
      }

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiChannelEndpointListener^>(info.This())) {
        return;
      }

      MidiChannelEndpointListener *wrapper = MidiChannelEndpointListener::Unwrap<MidiChannelEndpointListener>(info.This());

      try {

        bool winRtValue = Nan::To<bool>(value).FromMaybe(false);

        wrapper->_instance->PreventCallingFurtherListeners = winRtValue;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
      }
    }
      
    static void IncludeGroupGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiChannelEndpointListener^>(info.This())) {
        return;
      }

      MidiChannelEndpointListener *wrapper = MidiChannelEndpointListener::Unwrap<MidiChannelEndpointListener>(info.This());

      try  {
        ::Windows::Devices::Midi2::MidiGroup^ result = wrapper->_instance->IncludeGroup;
        info.GetReturnValue().Set(WrapMidiGroup(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void IncludeGroupSetter(Local<String> property, Local<Value> value, const Nan::PropertyCallbackInfo<void> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiGroup^>(value)) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Value to set is of unexpected type")));
        return;
      }

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiChannelEndpointListener^>(info.This())) {
        return;
      }

      MidiChannelEndpointListener *wrapper = MidiChannelEndpointListener::Unwrap<MidiChannelEndpointListener>(info.This());

      try {

        ::Windows::Devices::Midi2::MidiGroup^ winRtValue = dynamic_cast<::Windows::Devices::Midi2::MidiGroup^>(NodeRT::Utils::GetObjectInstance(value));

        wrapper->_instance->IncludeGroup = winRtValue;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
      }
    }
      
    static void IncludeChannelsGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiChannelEndpointListener^>(info.This())) {
        return;
      }

      MidiChannelEndpointListener *wrapper = MidiChannelEndpointListener::Unwrap<MidiChannelEndpointListener>(info.This());

      try  {
        ::Windows::Foundation::Collections::IVector<::Windows::Devices::Midi2::MidiChannel^>^ result = wrapper->_instance->IncludeChannels;
        info.GetReturnValue().Set(NodeRT::Collections::VectorWrapper<::Windows::Devices::Midi2::MidiChannel^>::CreateVectorWrapper(result, 
            [](::Windows::Devices::Midi2::MidiChannel^ val) -> Local<Value> {
              return WrapMidiChannel(val);
            },
            [](Local<Value> value) -> bool {
              return NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiChannel^>(value);
            },
            [](Local<Value> value) -> ::Windows::Devices::Midi2::MidiChannel^ {
              return UnwrapMidiChannel(value);
            }
          ));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void TagGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiChannelEndpointListener^>(info.This())) {
        return;
      }

      MidiChannelEndpointListener *wrapper = MidiChannelEndpointListener::Unwrap<MidiChannelEndpointListener>(info.This());

      try  {
        ::Platform::Object^ result = wrapper->_instance->Tag;
        info.GetReturnValue().Set(CreateOpaqueWrapper(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void TagSetter(Local<String> property, Local<Value> value, const Nan::PropertyCallbackInfo<void> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Platform::Object^>(value)) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Value to set is of unexpected type")));
        return;
      }

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiChannelEndpointListener^>(info.This())) {
        return;
      }

      MidiChannelEndpointListener *wrapper = MidiChannelEndpointListener::Unwrap<MidiChannelEndpointListener>(info.This());

      try {

        ::Platform::Object^ winRtValue = dynamic_cast<::Platform::Object^>(NodeRT::Utils::GetObjectInstance(value));

        wrapper->_instance->Tag = winRtValue;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
      }
    }
      
    static void NameGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiChannelEndpointListener^>(info.This())) {
        return;
      }

      MidiChannelEndpointListener *wrapper = MidiChannelEndpointListener::Unwrap<MidiChannelEndpointListener>(info.This());

      try  {
        Platform::String^ result = wrapper->_instance->Name;
        info.GetReturnValue().Set(NodeRT::Utils::NewString(result->Data()));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void NameSetter(Local<String> property, Local<Value> value, const Nan::PropertyCallbackInfo<void> &info) {
      HandleScope scope;

      if (!value->IsString()) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Value to set is of unexpected type")));
        return;
      }

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiChannelEndpointListener^>(info.This())) {
        return;
      }

      MidiChannelEndpointListener *wrapper = MidiChannelEndpointListener::Unwrap<MidiChannelEndpointListener>(info.This());

      try {

        Platform::String^ winRtValue = ref new Platform::String(NodeRT::Utils::StringToWchar(v8::String::Value(v8::Isolate::GetCurrent(), value)));

        wrapper->_instance->Name = winRtValue;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
      }
    }
      
    static void IsEnabledGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiChannelEndpointListener^>(info.This())) {
        return;
      }

      MidiChannelEndpointListener *wrapper = MidiChannelEndpointListener::Unwrap<MidiChannelEndpointListener>(info.This());

      try  {
        bool result = wrapper->_instance->IsEnabled;
        info.GetReturnValue().Set(Nan::New<Boolean>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void IsEnabledSetter(Local<String> property, Local<Value> value, const Nan::PropertyCallbackInfo<void> &info) {
      HandleScope scope;

      if (!value->IsBoolean()) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Value to set is of unexpected type")));
        return;
      }

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiChannelEndpointListener^>(info.This())) {
        return;
      }

      MidiChannelEndpointListener *wrapper = MidiChannelEndpointListener::Unwrap<MidiChannelEndpointListener>(info.This());

      try {

        bool winRtValue = Nan::To<bool>(value).FromMaybe(false);

        wrapper->_instance->IsEnabled = winRtValue;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
      }
    }
      
    static void IdGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiChannelEndpointListener^>(info.This())) {
        return;
      }

      MidiChannelEndpointListener *wrapper = MidiChannelEndpointListener::Unwrap<MidiChannelEndpointListener>(info.This());

      try  {
        Platform::String^ result = wrapper->_instance->Id;
        info.GetReturnValue().Set(NodeRT::Utils::NewString(result->Data()));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void IdSetter(Local<String> property, Local<Value> value, const Nan::PropertyCallbackInfo<void> &info) {
      HandleScope scope;

      if (!value->IsString()) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Value to set is of unexpected type")));
        return;
      }

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiChannelEndpointListener^>(info.This())) {
        return;
      }

      MidiChannelEndpointListener *wrapper = MidiChannelEndpointListener::Unwrap<MidiChannelEndpointListener>(info.This());

      try {

        Platform::String^ winRtValue = ref new Platform::String(NodeRT::Utils::StringToWchar(v8::String::Value(v8::Isolate::GetCurrent(), value)));

        wrapper->_instance->Id = winRtValue;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
      }
    }
      


    static void AddListener(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() < 2 || !info[0]->IsString() || !info[1]->IsFunction()) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"wrong arguments, expected arguments are eventName(string),callback(function)")));
        return;
      }

      String::Value eventName(v8::Isolate::GetCurrent(), info[0]);
      auto str = *eventName;

      Local<Function> callback = info[1].As<Function>();

      ::Windows::Foundation::EventRegistrationToken registrationToken;
      if (NodeRT::Utils::CaseInsenstiveEquals(L"messageReceived", str))
      {
        if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiChannelEndpointListener^>(info.This()))
        {
          Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"The caller of this method isn't of the expected type or internal WinRt object was disposed")));
      return;
        }
        MidiChannelEndpointListener *wrapper = MidiChannelEndpointListener::Unwrap<MidiChannelEndpointListener>(info.This());
      
        try {
          Persistent<Object>* perstPtr = new Persistent<Object>();
          perstPtr->Reset(NodeRT::Utils::CreateCallbackObjectInDomain(callback));
          std::shared_ptr<Persistent<Object>> callbackObjPtr(perstPtr,
            [] (Persistent<Object> *ptr ) {
              NodeUtils::Async::RunOnMain([ptr]() {
                ptr->Reset();
                delete ptr;
            });
          });

          registrationToken = wrapper->_instance->MessageReceived::add(
            ref new ::Windows::Foundation::TypedEventHandler<::Windows::Devices::Midi2::IMidiMessageReceivedEventSource^, ::Windows::Devices::Midi2::MidiMessageReceivedEventArgs^>(
            [callbackObjPtr](::Windows::Devices::Midi2::IMidiMessageReceivedEventSource^ arg0, ::Windows::Devices::Midi2::MidiMessageReceivedEventArgs^ arg1) {
              NodeUtils::Async::RunOnMain([callbackObjPtr , arg0, arg1]() {
                HandleScope scope;


                Local<Value> wrappedArg0;
                Local<Value> wrappedArg1;

                {
                  TryCatch tryCatch;


                  wrappedArg0 = WrapIMidiMessageReceivedEventSource(arg0);
                  wrappedArg1 = WrapMidiMessageReceivedEventArgs(arg1);


                  if (wrappedArg0.IsEmpty()) wrappedArg0 = Undefined();
                  if (wrappedArg1.IsEmpty()) wrappedArg1 = Undefined();
                }

                Local<Value> args[] = { wrappedArg0, wrappedArg1 };
                Local<Object> callbackObjLocalRef = Nan::New<Object>(*callbackObjPtr);
                NodeRT::Utils::CallCallbackInDomain(callbackObjLocalRef, _countof(args), args);
              });
            })
          );
        }
        catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }

      }
 else  {
        Nan::ThrowError(Nan::Error(String::Concat(v8::Isolate::GetCurrent(), NodeRT::Utils::NewString(L"given event name isn't supported: "), info[0].As<String>())));
        return;
      }

      Local<Value> tokenMapVal = NodeRT::Utils::GetHiddenValue(callback, Nan::New<String>(REGISTRATION_TOKEN_MAP_PROPERTY_NAME).ToLocalChecked());
      Local<Object> tokenMap;

      if (tokenMapVal.IsEmpty() || Nan::Equals(tokenMapVal, Undefined()).FromMaybe(false)) {
        tokenMap = Nan::New<Object>();
        NodeRT::Utils::SetHiddenValueWithObject(callback, Nan::New<String>(REGISTRATION_TOKEN_MAP_PROPERTY_NAME).ToLocalChecked(), tokenMap);
      } else {
        tokenMap = Nan::To<Object>(tokenMapVal).ToLocalChecked();
      }

      Nan::Set(tokenMap, info[0], CreateOpaqueWrapper(::Windows::Foundation::PropertyValue::CreateInt64(registrationToken.Value)));
    }

    static void RemoveListener(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() < 2 || !info[0]->IsString() || !info[1]->IsFunction()) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"wrong arguments, expected a string and a callback")));
        return;
      }

      String::Value eventName(v8::Isolate::GetCurrent(), info[0]);
      auto str = *eventName;

      if ((!NodeRT::Utils::CaseInsenstiveEquals(L"messageReceived", str))) {
        Nan::ThrowError(Nan::Error(String::Concat(v8::Isolate::GetCurrent(), NodeRT::Utils::NewString(L"given event name isn't supported: "), info[0].As<String>())));
        return;
      }

      Local<Function> callback = info[1].As<Function>();
      Local<Value> tokenMap = NodeRT::Utils::GetHiddenValue(callback, Nan::New<String>(REGISTRATION_TOKEN_MAP_PROPERTY_NAME).ToLocalChecked());

      if (tokenMap.IsEmpty() || Nan::Equals(tokenMap, Undefined()).FromMaybe(false)) {
        return;
      }

      Local<Value> opaqueWrapperObj =  Nan::Get(Nan::To<Object>(tokenMap).ToLocalChecked(), info[0]).ToLocalChecked();

      if (opaqueWrapperObj.IsEmpty() || Nan::Equals(opaqueWrapperObj,Undefined()).FromMaybe(false)) {
        return;
      }

      OpaqueWrapper *opaqueWrapper = OpaqueWrapper::Unwrap<OpaqueWrapper>(opaqueWrapperObj.As<Object>());

      long long tokenValue = (long long) opaqueWrapper->GetObjectInstance();
      ::Windows::Foundation::EventRegistrationToken registrationToken;
      registrationToken.Value = tokenValue;

      try  {
        if (NodeRT::Utils::CaseInsenstiveEquals(L"messageReceived", str)) {
          if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiChannelEndpointListener^>(info.This()))
          {
            Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"The caller of this method isn't of the expected type or internal WinRt object was disposed")));
            return;
          }
          MidiChannelEndpointListener *wrapper = MidiChannelEndpointListener::Unwrap<MidiChannelEndpointListener>(info.This());
          wrapper->_instance->MessageReceived::remove(registrationToken);
        }
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
      }

      Nan::Delete(Nan::To<Object>(tokenMap).ToLocalChecked(), Nan::To<String>(info[0]).ToLocalChecked());
    }
    private:
      ::Windows::Devices::Midi2::MidiChannelEndpointListener^ _instance;
      static Persistent<FunctionTemplate> s_constructorTemplate;

      friend v8::Local<v8::Value> WrapMidiChannelEndpointListener(::Windows::Devices::Midi2::MidiChannelEndpointListener^ wintRtInstance);
      friend ::Windows::Devices::Midi2::MidiChannelEndpointListener^ UnwrapMidiChannelEndpointListener(Local<Value> value);
  };

  Persistent<FunctionTemplate> MidiChannelEndpointListener::s_constructorTemplate;

  v8::Local<v8::Value> WrapMidiChannelEndpointListener(::Windows::Devices::Midi2::MidiChannelEndpointListener^ winRtInstance) {
    EscapableHandleScope scope;

    if (winRtInstance == nullptr) {
      return scope.Escape(Undefined());
    }

    Local<Value> opaqueWrapper = CreateOpaqueWrapper(winRtInstance);
    Local<Value> args[] = {opaqueWrapper};
    Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(MidiChannelEndpointListener::s_constructorTemplate);
    return scope.Escape(Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(),_countof(args), args).ToLocalChecked());
  }

  ::Windows::Devices::Midi2::MidiChannelEndpointListener^ UnwrapMidiChannelEndpointListener(Local<Value> value) {
     return MidiChannelEndpointListener::Unwrap<MidiChannelEndpointListener>(Nan::To<Object>(value).ToLocalChecked())->_instance;
  }

  void InitMidiChannelEndpointListener(Local<Object> exports) {
    MidiChannelEndpointListener::Init(exports);
  }

  class MidiClock : public WrapperBase {
    public:
      
      static void Init(const Local<Object> exports) {
        HandleScope scope;

        Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(New);
        s_constructorTemplate.Reset(localRef);
        localRef->SetClassName(Nan::New<String>("MidiClock").ToLocalChecked());
        localRef->InstanceTemplate()->SetInternalFieldCount(1);






        Local<Object> constructor = Nan::To<Object>(Nan::GetFunction(localRef).ToLocalChecked()).ToLocalChecked();
        Nan::SetMethod(constructor, "castFrom", CastFrom);

        Nan::SetMethod(constructor, "convertTimestampToMicroseconds", ConvertTimestampToMicroseconds);
        Nan::SetMethod(constructor, "convertTimestampToMilliseconds", ConvertTimestampToMilliseconds);
        Nan::SetMethod(constructor, "offsetTimestampByTicks", OffsetTimestampByTicks);
        Nan::SetMethod(constructor, "offsetTimestampByMicroseconds", OffsetTimestampByMicroseconds);
        Nan::SetMethod(constructor, "offsetTimestampByMilliseconds", OffsetTimestampByMilliseconds);
        Nan::SetAccessor(constructor, Nan::New<String>("now").ToLocalChecked(), NowGetter);
        Nan::SetAccessor(constructor, Nan::New<String>("timestampFrequency").ToLocalChecked(), TimestampFrequencyGetter);


        Nan::Set(exports, Nan::New<String>("MidiClock").ToLocalChecked(), constructor);
      }

      virtual ::Platform::Object^ GetObjectInstance() const override {
        return _instance;
      }

    private:

      MidiClock(::Windows::Devices::Midi2::MidiClock^ instance) {
        _instance = instance;
      }

      
    static void New(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(s_constructorTemplate);

      // in case the constructor was called without the new operator
      if (!localRef->HasInstance(info.This())) {
        if (info.Length() > 0) {
          std::unique_ptr<Local<Value> []> constructorArgs(new Local<Value>[info.Length()]);

          Local<Value> *argsPtr = constructorArgs.get();
          for (int i = 0; i < info.Length(); i++) {
            argsPtr[i] = info[i];
          }

          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), constructorArgs.get());
          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        } else {
          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), nullptr);

          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        }
      }

      ::Windows::Devices::Midi2::MidiClock^ winRtInstance;


      if (info.Length() == 1 && OpaqueWrapper::IsOpaqueWrapper(info[0]) &&
        NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiClock^>(info[0])) {
        try {
          winRtInstance = (::Windows::Devices::Midi2::MidiClock^) NodeRT::Utils::GetObjectInstance(info[0]);
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no suitable constructor found")));
        return;
      }

      NodeRT::Utils::SetHiddenValue(info.This(), Nan::New<String>("__winRtInstance__").ToLocalChecked(), True());

      MidiClock *wrapperInstance = new MidiClock(winRtInstance);
      wrapperInstance->Wrap(info.This());

      info.GetReturnValue().Set(info.This());
    }


      
    static void CastFrom(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;
      if (info.Length() < 1 || !NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiClock^>(info[0])) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no object provided, or given object could not be casted to requested type")));
        return;
      }

      ::Windows::Devices::Midi2::MidiClock^ winRtInstance;
      try {
        winRtInstance = (::Windows::Devices::Midi2::MidiClock^) NodeRT::Utils::GetObjectInstance(info[0]);
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }

      info.GetReturnValue().Set(WrapMidiClock(winRtInstance));
    }





    static void ConvertTimestampToMicroseconds(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 1
        && info[0]->IsNumber())
      {
        try
        {
          unsigned __int64 arg0 = static_cast<unsigned __int64>(Nan::To<int64_t>(info[0]).FromMaybe(0));
          
          double result;
          result = ::Windows::Devices::Midi2::MidiClock::ConvertTimestampToMicroseconds(arg0);
          info.GetReturnValue().Set(Nan::New<Number>(static_cast<double>(result)));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void ConvertTimestampToMilliseconds(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 1
        && info[0]->IsNumber())
      {
        try
        {
          unsigned __int64 arg0 = static_cast<unsigned __int64>(Nan::To<int64_t>(info[0]).FromMaybe(0));
          
          double result;
          result = ::Windows::Devices::Midi2::MidiClock::ConvertTimestampToMilliseconds(arg0);
          info.GetReturnValue().Set(Nan::New<Number>(static_cast<double>(result)));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void OffsetTimestampByTicks(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 2
        && info[0]->IsNumber()
        && info[1]->IsNumber())
      {
        try
        {
          unsigned __int64 arg0 = static_cast<unsigned __int64>(Nan::To<int64_t>(info[0]).FromMaybe(0));
          __int64 arg1 = Nan::To<int64_t>(info[1]).FromMaybe(0);
          
          unsigned __int64 result;
          result = ::Windows::Devices::Midi2::MidiClock::OffsetTimestampByTicks(arg0, arg1);
          info.GetReturnValue().Set(Nan::New<Number>(static_cast<double>(result)));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void OffsetTimestampByMicroseconds(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 2
        && info[0]->IsNumber()
        && info[1]->IsNumber())
      {
        try
        {
          unsigned __int64 arg0 = static_cast<unsigned __int64>(Nan::To<int64_t>(info[0]).FromMaybe(0));
          __int64 arg1 = Nan::To<int64_t>(info[1]).FromMaybe(0);
          
          unsigned __int64 result;
          result = ::Windows::Devices::Midi2::MidiClock::OffsetTimestampByMicroseconds(arg0, arg1);
          info.GetReturnValue().Set(Nan::New<Number>(static_cast<double>(result)));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void OffsetTimestampByMilliseconds(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 2
        && info[0]->IsNumber()
        && info[1]->IsNumber())
      {
        try
        {
          unsigned __int64 arg0 = static_cast<unsigned __int64>(Nan::To<int64_t>(info[0]).FromMaybe(0));
          __int64 arg1 = Nan::To<int64_t>(info[1]).FromMaybe(0);
          
          unsigned __int64 result;
          result = ::Windows::Devices::Midi2::MidiClock::OffsetTimestampByMilliseconds(arg0, arg1);
          info.GetReturnValue().Set(Nan::New<Number>(static_cast<double>(result)));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }



    static void NowGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      try
      {
        unsigned __int64 result = ::Windows::Devices::Midi2::MidiClock::Now;
        info.GetReturnValue().Set(Nan::New<Number>(static_cast<double>(result)));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      

    static void TimestampFrequencyGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      try
      {
        unsigned __int64 result = ::Windows::Devices::Midi2::MidiClock::TimestampFrequency;
        info.GetReturnValue().Set(Nan::New<Number>(static_cast<double>(result)));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      

    private:
      ::Windows::Devices::Midi2::MidiClock^ _instance;
      static Persistent<FunctionTemplate> s_constructorTemplate;

      friend v8::Local<v8::Value> WrapMidiClock(::Windows::Devices::Midi2::MidiClock^ wintRtInstance);
      friend ::Windows::Devices::Midi2::MidiClock^ UnwrapMidiClock(Local<Value> value);
  };

  Persistent<FunctionTemplate> MidiClock::s_constructorTemplate;

  v8::Local<v8::Value> WrapMidiClock(::Windows::Devices::Midi2::MidiClock^ winRtInstance) {
    EscapableHandleScope scope;

    if (winRtInstance == nullptr) {
      return scope.Escape(Undefined());
    }

    Local<Value> opaqueWrapper = CreateOpaqueWrapper(winRtInstance);
    Local<Value> args[] = {opaqueWrapper};
    Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(MidiClock::s_constructorTemplate);
    return scope.Escape(Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(),_countof(args), args).ToLocalChecked());
  }

  ::Windows::Devices::Midi2::MidiClock^ UnwrapMidiClock(Local<Value> value) {
     return MidiClock::Unwrap<MidiClock>(Nan::To<Object>(value).ToLocalChecked())->_instance;
  }

  void InitMidiClock(Local<Object> exports) {
    MidiClock::Init(exports);
  }

  class MidiEndpointConnection : public WrapperBase {
    public:
      
      static void Init(const Local<Object> exports) {
        HandleScope scope;

        Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(New);
        s_constructorTemplate.Reset(localRef);
        localRef->SetClassName(Nan::New<String>("MidiEndpointConnection").ToLocalChecked());
        localRef->InstanceTemplate()->SetInternalFieldCount(1);


          
            Nan::SetPrototypeMethod(localRef, "open", Open);
            Nan::SetPrototypeMethod(localRef, "sendMessagePacket", SendMessagePacket);
      //      Nan::SetPrototypeMethod(localRef, "sendMessageStruct", SendMessageStruct);
            Nan::SetPrototypeMethod(localRef, "sendMessageWordArray", SendMessageWordArray);
            Nan::SetPrototypeMethod(localRef, "sendMessageWords", SendMessageWords);
            Nan::SetPrototypeMethod(localRef, "sendMessageBuffer", SendMessageBuffer);
            Nan::SetPrototypeMethod(localRef, "close", Close);
          


          
          Nan::SetPrototypeMethod(localRef,"addListener", AddListener);
          Nan::SetPrototypeMethod(localRef,"on", AddListener);
          Nan::SetPrototypeMethod(localRef,"removeListener", RemoveListener);
          Nan::SetPrototypeMethod(localRef, "off", RemoveListener);

          
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("tag").ToLocalChecked(), TagGetter, TagSetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("connectionId").ToLocalChecked(), ConnectionIdGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("endpointDeviceId").ToLocalChecked(), EndpointDeviceIdGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("isOpen").ToLocalChecked(), IsOpenGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("messageProcessingPlugins").ToLocalChecked(), MessageProcessingPluginsGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("settings").ToLocalChecked(), SettingsGetter);

        Local<Object> constructor = Nan::To<Object>(Nan::GetFunction(localRef).ToLocalChecked()).ToLocalChecked();
        Nan::SetMethod(constructor, "castFrom", CastFrom);

        Nan::SetMethod(constructor, "getDeviceSelector", GetDeviceSelector);
        Nan::SetMethod(constructor, "sendMessageSucceeded", SendMessageSucceeded);
        Nan::SetMethod(constructor, "sendMessageFailed", SendMessageFailed);


        Nan::Set(exports, Nan::New<String>("MidiEndpointConnection").ToLocalChecked(), constructor);
      }

      virtual ::Platform::Object^ GetObjectInstance() const override {
        return _instance;
      }

    private:

      MidiEndpointConnection(::Windows::Devices::Midi2::MidiEndpointConnection^ instance) {
        _instance = instance;
      }

      
    static void New(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(s_constructorTemplate);

      // in case the constructor was called without the new operator
      if (!localRef->HasInstance(info.This())) {
        if (info.Length() > 0) {
          std::unique_ptr<Local<Value> []> constructorArgs(new Local<Value>[info.Length()]);

          Local<Value> *argsPtr = constructorArgs.get();
          for (int i = 0; i < info.Length(); i++) {
            argsPtr[i] = info[i];
          }

          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), constructorArgs.get());
          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        } else {
          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), nullptr);

          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        }
      }

      ::Windows::Devices::Midi2::MidiEndpointConnection^ winRtInstance;


      if (info.Length() == 1 && OpaqueWrapper::IsOpaqueWrapper(info[0]) &&
        NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointConnection^>(info[0])) {
        try {
          winRtInstance = (::Windows::Devices::Midi2::MidiEndpointConnection^) NodeRT::Utils::GetObjectInstance(info[0]);
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no suitable constructor found")));
        return;
      }

      NodeRT::Utils::SetHiddenValue(info.This(), Nan::New<String>("__winRtInstance__").ToLocalChecked(), True());

      MidiEndpointConnection *wrapperInstance = new MidiEndpointConnection(winRtInstance);
      wrapperInstance->Wrap(info.This());

      info.GetReturnValue().Set(info.This());
    }


      
    static void CastFrom(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;
      if (info.Length() < 1 || !NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointConnection^>(info[0])) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no object provided, or given object could not be casted to requested type")));
        return;
      }

      ::Windows::Devices::Midi2::MidiEndpointConnection^ winRtInstance;
      try {
        winRtInstance = (::Windows::Devices::Midi2::MidiEndpointConnection^) NodeRT::Utils::GetObjectInstance(info[0]);
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }

      info.GetReturnValue().Set(WrapMidiEndpointConnection(winRtInstance));
    }


    static void Open(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointConnection^>(info.This())) {
        return;
      }

      MidiEndpointConnection *wrapper = MidiEndpointConnection::Unwrap<MidiEndpointConnection>(info.This());

      if (info.Length() == 0)
      {
        try
        {
          bool result;
          result = wrapper->_instance->Open();
          info.GetReturnValue().Set(Nan::New<Boolean>(result));
          return;
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }
    static void SendMessagePacket(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointConnection^>(info.This())) {
        return;
      }

      MidiEndpointConnection *wrapper = MidiEndpointConnection::Unwrap<MidiEndpointConnection>(info.This());

      if (info.Length() == 1
        && NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::IMidiUniversalPacket^>(info[0]))
      {
        try
        {
          ::Windows::Devices::Midi2::IMidiUniversalPacket^ arg0 = UnwrapIMidiUniversalPacket(info[0]);
          
          ::Windows::Devices::Midi2::MidiSendMessageResult result;
          result = wrapper->_instance->SendMessagePacket(arg0);
          info.GetReturnValue().Set(Nan::New<Integer>(static_cast<int>(result)));
          return;
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }
 //   static void SendMessageStruct(Nan::NAN_METHOD_ARGS_TYPE info) {
 //     HandleScope scope;

 //     if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointConnection^>(info.This())) {
 //       return;
 //     }

 //     MidiEndpointConnection *wrapper = MidiEndpointConnection::Unwrap<MidiEndpointConnection>(info.This());

 //     if (info.Length() == 3
 //       && info[0]->IsNumber()
 //       && NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessageStruct&^>(info[1])
 //       && info[2]->IsInt32())
 //     {
 //       try
 //       {
 //         unsigned __int64 arg0 = static_cast<unsigned __int64>(Nan::To<int64_t>(info[0]).FromMaybe(0));
 //         ::Windows::Devices::Midi2::MidiMessageStruct arg1 = MidiMessageStructFromJsObject(info[1]);
 //         unsigned char arg2 = static_cast<unsigned char>(Nan::To<int32_t>(info[2]).FromMaybe(0));
 //         
 //         ::Windows::Devices::Midi2::MidiSendMessageResult result;
 //         result = wrapper->_instance->SendMessageStruct(arg0, arg1, arg2);
 //         info.GetReturnValue().Set(Nan::New<Integer>(static_cast<int>(result)));
 //         return;
 //       } catch (Platform::Exception ^exception) {
 //         NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
 //         return;
 //       }
 //     }
 //else {
 //       Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
 //       return;
 //     }
 //   }
    static void SendMessageWordArray(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointConnection^>(info.This())) {
        return;
      }

      MidiEndpointConnection *wrapper = MidiEndpointConnection::Unwrap<MidiEndpointConnection>(info.This());

      if (info.Length() == 4
        && info[0]->IsNumber()
        && (NodeRT::Utils::IsWinRtWrapperOf<::Platform::Array<unsigned int>^>(info[1]) || info[1]->IsArray())
        && info[2]->IsUint32()
        && info[3]->IsInt32())
      {
        try
        {
          unsigned __int64 arg0 = static_cast<unsigned __int64>(Nan::To<int64_t>(info[0]).FromMaybe(0));
          ::Platform::Array<unsigned int>^ arg1 = 
            [] (v8::Local<v8::Value> value) -> ::Platform::Array<unsigned int>^
            {
              if (value->IsArray())
              {
                return NodeRT::Collections::JsArrayToWinrtArray<unsigned int>(value.As<Array>(), 
                 [](Local<Value> value) -> bool {
                   return value->IsUint32();
                 },
                 [](Local<Value> value) -> unsigned int {
                   return static_cast<unsigned int>(Nan::To<uint32_t>(value).FromMaybe(0));
                 }
                );
              }
              else
              {
                return dynamic_cast<::Platform::Array<unsigned int>^>(NodeRT::Utils::GetObjectInstance(value));
              }
            } (info[1]);
          unsigned int arg2 = static_cast<unsigned int>(Nan::To<uint32_t>(info[2]).FromMaybe(0));
          unsigned char arg3 = static_cast<unsigned char>(Nan::To<int32_t>(info[3]).FromMaybe(0));
          
          ::Windows::Devices::Midi2::MidiSendMessageResult result;
          result = wrapper->_instance->SendMessageWordArray(arg0, arg1, arg2, arg3);
          info.GetReturnValue().Set(Nan::New<Integer>(static_cast<int>(result)));
          return;
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }
    static void SendMessageWords(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointConnection^>(info.This())) {
        return;
      }

      MidiEndpointConnection *wrapper = MidiEndpointConnection::Unwrap<MidiEndpointConnection>(info.This());

      if (info.Length() == 2
        && info[0]->IsNumber()
        && info[1]->IsUint32())
      {
        try
        {
          unsigned __int64 arg0 = static_cast<unsigned __int64>(Nan::To<int64_t>(info[0]).FromMaybe(0));
          unsigned int arg1 = static_cast<unsigned int>(Nan::To<uint32_t>(info[1]).FromMaybe(0));
          
          ::Windows::Devices::Midi2::MidiSendMessageResult result;
          result = wrapper->_instance->SendMessageWords(arg0, arg1);
          info.GetReturnValue().Set(Nan::New<Integer>(static_cast<int>(result)));
          return;
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
      else if (info.Length() == 3
        && info[0]->IsNumber()
        && info[1]->IsUint32()
        && info[2]->IsUint32())
      {
        try
        {
          unsigned __int64 arg0 = static_cast<unsigned __int64>(Nan::To<int64_t>(info[0]).FromMaybe(0));
          unsigned int arg1 = static_cast<unsigned int>(Nan::To<uint32_t>(info[1]).FromMaybe(0));
          unsigned int arg2 = static_cast<unsigned int>(Nan::To<uint32_t>(info[2]).FromMaybe(0));
          
          ::Windows::Devices::Midi2::MidiSendMessageResult result;
          result = wrapper->_instance->SendMessageWords(arg0, arg1, arg2);
          info.GetReturnValue().Set(Nan::New<Integer>(static_cast<int>(result)));
          return;
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
      else if (info.Length() == 4
        && info[0]->IsNumber()
        && info[1]->IsUint32()
        && info[2]->IsUint32()
        && info[3]->IsUint32())
      {
        try
        {
          unsigned __int64 arg0 = static_cast<unsigned __int64>(Nan::To<int64_t>(info[0]).FromMaybe(0));
          unsigned int arg1 = static_cast<unsigned int>(Nan::To<uint32_t>(info[1]).FromMaybe(0));
          unsigned int arg2 = static_cast<unsigned int>(Nan::To<uint32_t>(info[2]).FromMaybe(0));
          unsigned int arg3 = static_cast<unsigned int>(Nan::To<uint32_t>(info[3]).FromMaybe(0));
          
          ::Windows::Devices::Midi2::MidiSendMessageResult result;
          result = wrapper->_instance->SendMessageWords(arg0, arg1, arg2, arg3);
          info.GetReturnValue().Set(Nan::New<Integer>(static_cast<int>(result)));
          return;
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
      else if (info.Length() == 5
        && info[0]->IsNumber()
        && info[1]->IsUint32()
        && info[2]->IsUint32()
        && info[3]->IsUint32()
        && info[4]->IsUint32())
      {
        try
        {
          unsigned __int64 arg0 = static_cast<unsigned __int64>(Nan::To<int64_t>(info[0]).FromMaybe(0));
          unsigned int arg1 = static_cast<unsigned int>(Nan::To<uint32_t>(info[1]).FromMaybe(0));
          unsigned int arg2 = static_cast<unsigned int>(Nan::To<uint32_t>(info[2]).FromMaybe(0));
          unsigned int arg3 = static_cast<unsigned int>(Nan::To<uint32_t>(info[3]).FromMaybe(0));
          unsigned int arg4 = static_cast<unsigned int>(Nan::To<uint32_t>(info[4]).FromMaybe(0));
          
          ::Windows::Devices::Midi2::MidiSendMessageResult result;
          result = wrapper->_instance->SendMessageWords(arg0, arg1, arg2, arg3, arg4);
          info.GetReturnValue().Set(Nan::New<Integer>(static_cast<int>(result)));
          return;
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }
    static void SendMessageBuffer(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointConnection^>(info.This())) {
        return;
      }

      MidiEndpointConnection *wrapper = MidiEndpointConnection::Unwrap<MidiEndpointConnection>(info.This());

      if (info.Length() == 4
        && info[0]->IsNumber()
        && NodeRT::Utils::IsWinRtWrapperOf<::Windows::Foundation::IMemoryBuffer^>(info[1])
        && info[2]->IsUint32()
        && info[3]->IsInt32())
      {
        try
        {
          unsigned __int64 arg0 = static_cast<unsigned __int64>(Nan::To<int64_t>(info[0]).FromMaybe(0));
          ::Windows::Foundation::IMemoryBuffer^ arg1 = dynamic_cast<::Windows::Foundation::IMemoryBuffer^>(NodeRT::Utils::GetObjectInstance(info[1]));
          unsigned int arg2 = static_cast<unsigned int>(Nan::To<uint32_t>(info[2]).FromMaybe(0));
          unsigned char arg3 = static_cast<unsigned char>(Nan::To<int32_t>(info[3]).FromMaybe(0));
          
          ::Windows::Devices::Midi2::MidiSendMessageResult result;
          result = wrapper->_instance->SendMessageBuffer(arg0, arg1, arg2, arg3);
          info.GetReturnValue().Set(Nan::New<Integer>(static_cast<int>(result)));
          return;
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void Close(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointConnection^>(info.This())) {
        return;
      }

      MidiEndpointConnection *wrapper = MidiEndpointConnection::Unwrap<MidiEndpointConnection>(info.This());

      if (info.Length() == 0) {
        try {
          delete wrapper->_instance;
          wrapper->_instance = nullptr;
          return;
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      } else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }



    static void GetDeviceSelector(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 0)
      {
        try
        {
          Platform::String^ result;
          result = ::Windows::Devices::Midi2::MidiEndpointConnection::GetDeviceSelector();
          info.GetReturnValue().Set(NodeRT::Utils::NewString(result->Data()));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void SendMessageSucceeded(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 1
        && info[0]->IsInt32())
      {
        try
        {
          ::Windows::Devices::Midi2::MidiSendMessageResult arg0 = static_cast<::Windows::Devices::Midi2::MidiSendMessageResult>(Nan::To<int32_t>(info[0]).FromMaybe(0));
          
          bool result;
          result = ::Windows::Devices::Midi2::MidiEndpointConnection::SendMessageSucceeded(arg0);
          info.GetReturnValue().Set(Nan::New<Boolean>(result));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void SendMessageFailed(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 1
        && info[0]->IsInt32())
      {
        try
        {
          ::Windows::Devices::Midi2::MidiSendMessageResult arg0 = static_cast<::Windows::Devices::Midi2::MidiSendMessageResult>(Nan::To<int32_t>(info[0]).FromMaybe(0));
          
          bool result;
          result = ::Windows::Devices::Midi2::MidiEndpointConnection::SendMessageFailed(arg0);
          info.GetReturnValue().Set(Nan::New<Boolean>(result));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void TagGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointConnection^>(info.This())) {
        return;
      }

      MidiEndpointConnection *wrapper = MidiEndpointConnection::Unwrap<MidiEndpointConnection>(info.This());

      try  {
        ::Platform::Object^ result = wrapper->_instance->Tag;
        info.GetReturnValue().Set(CreateOpaqueWrapper(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void TagSetter(Local<String> property, Local<Value> value, const Nan::PropertyCallbackInfo<void> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Platform::Object^>(value)) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Value to set is of unexpected type")));
        return;
      }

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointConnection^>(info.This())) {
        return;
      }

      MidiEndpointConnection *wrapper = MidiEndpointConnection::Unwrap<MidiEndpointConnection>(info.This());

      try {

        ::Platform::Object^ winRtValue = dynamic_cast<::Platform::Object^>(NodeRT::Utils::GetObjectInstance(value));

        wrapper->_instance->Tag = winRtValue;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
      }
    }
      
    static void ConnectionIdGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointConnection^>(info.This())) {
        return;
      }

      MidiEndpointConnection *wrapper = MidiEndpointConnection::Unwrap<MidiEndpointConnection>(info.This());

      try  {
        ::Platform::Guid result = wrapper->_instance->ConnectionId;
        info.GetReturnValue().Set(NodeRT::Utils::GuidToJs(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void EndpointDeviceIdGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointConnection^>(info.This())) {
        return;
      }

      MidiEndpointConnection *wrapper = MidiEndpointConnection::Unwrap<MidiEndpointConnection>(info.This());

      try  {
        Platform::String^ result = wrapper->_instance->EndpointDeviceId;
        info.GetReturnValue().Set(NodeRT::Utils::NewString(result->Data()));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void IsOpenGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointConnection^>(info.This())) {
        return;
      }

      MidiEndpointConnection *wrapper = MidiEndpointConnection::Unwrap<MidiEndpointConnection>(info.This());

      try  {
        bool result = wrapper->_instance->IsOpen;
        info.GetReturnValue().Set(Nan::New<Boolean>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void MessageProcessingPluginsGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointConnection^>(info.This())) {
        return;
      }

      MidiEndpointConnection *wrapper = MidiEndpointConnection::Unwrap<MidiEndpointConnection>(info.This());

      try  {
        ::Windows::Foundation::Collections::IVector<::Windows::Devices::Midi2::IMidiEndpointMessageProcessingPlugin^>^ result = wrapper->_instance->MessageProcessingPlugins;
        info.GetReturnValue().Set(NodeRT::Collections::VectorWrapper<::Windows::Devices::Midi2::IMidiEndpointMessageProcessingPlugin^>::CreateVectorWrapper(result, 
            [](::Windows::Devices::Midi2::IMidiEndpointMessageProcessingPlugin^ val) -> Local<Value> {
              return WrapIMidiEndpointMessageProcessingPlugin(val);
            },
            [](Local<Value> value) -> bool {
              return NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::IMidiEndpointMessageProcessingPlugin^>(value);
            },
            [](Local<Value> value) -> ::Windows::Devices::Midi2::IMidiEndpointMessageProcessingPlugin^ {
              return UnwrapIMidiEndpointMessageProcessingPlugin(value);
            }
          ));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void SettingsGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointConnection^>(info.This())) {
        return;
      }

      MidiEndpointConnection *wrapper = MidiEndpointConnection::Unwrap<MidiEndpointConnection>(info.This());

      try  {
        ::Windows::Devices::Midi2::IMidiEndpointDefinedConnectionSettings^ result = wrapper->_instance->Settings;
        info.GetReturnValue().Set(WrapIMidiEndpointDefinedConnectionSettings(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      


    static void AddListener(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() < 2 || !info[0]->IsString() || !info[1]->IsFunction()) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"wrong arguments, expected arguments are eventName(string),callback(function)")));
        return;
      }

      String::Value eventName(v8::Isolate::GetCurrent(), info[0]);
      auto str = *eventName;

      Local<Function> callback = info[1].As<Function>();

      ::Windows::Foundation::EventRegistrationToken registrationToken;
      if (NodeRT::Utils::CaseInsenstiveEquals(L"messageReceived", str))
      {
        if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointConnection^>(info.This()))
        {
          Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"The caller of this method isn't of the expected type or internal WinRt object was disposed")));
      return;
        }
        MidiEndpointConnection *wrapper = MidiEndpointConnection::Unwrap<MidiEndpointConnection>(info.This());
      
        try {
          Persistent<Object>* perstPtr = new Persistent<Object>();
          perstPtr->Reset(NodeRT::Utils::CreateCallbackObjectInDomain(callback));
          std::shared_ptr<Persistent<Object>> callbackObjPtr(perstPtr,
            [] (Persistent<Object> *ptr ) {
              NodeUtils::Async::RunOnMain([ptr]() {
                ptr->Reset();
                delete ptr;
            });
          });

          registrationToken = wrapper->_instance->MessageReceived::add(
            ref new ::Windows::Foundation::TypedEventHandler<::Windows::Devices::Midi2::IMidiMessageReceivedEventSource^, ::Windows::Devices::Midi2::MidiMessageReceivedEventArgs^>(
            [callbackObjPtr](::Windows::Devices::Midi2::IMidiMessageReceivedEventSource^ arg0, ::Windows::Devices::Midi2::MidiMessageReceivedEventArgs^ arg1) {
              NodeUtils::Async::RunOnMain([callbackObjPtr , arg0, arg1]() {
                HandleScope scope;


                Local<Value> wrappedArg0;
                Local<Value> wrappedArg1;

                {
                  TryCatch tryCatch;


                  wrappedArg0 = WrapIMidiMessageReceivedEventSource(arg0);
                  wrappedArg1 = WrapMidiMessageReceivedEventArgs(arg1);


                  if (wrappedArg0.IsEmpty()) wrappedArg0 = Undefined();
                  if (wrappedArg1.IsEmpty()) wrappedArg1 = Undefined();
                }

                Local<Value> args[] = { wrappedArg0, wrappedArg1 };
                Local<Object> callbackObjLocalRef = Nan::New<Object>(*callbackObjPtr);
                NodeRT::Utils::CallCallbackInDomain(callbackObjLocalRef, _countof(args), args);
              });
            })
          );
        }
        catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }

      }
 else  {
        Nan::ThrowError(Nan::Error(String::Concat(v8::Isolate::GetCurrent(), NodeRT::Utils::NewString(L"given event name isn't supported: "), info[0].As<String>())));
        return;
      }

      Local<Value> tokenMapVal = NodeRT::Utils::GetHiddenValue(callback, Nan::New<String>(REGISTRATION_TOKEN_MAP_PROPERTY_NAME).ToLocalChecked());
      Local<Object> tokenMap;

      if (tokenMapVal.IsEmpty() || Nan::Equals(tokenMapVal, Undefined()).FromMaybe(false)) {
        tokenMap = Nan::New<Object>();
        NodeRT::Utils::SetHiddenValueWithObject(callback, Nan::New<String>(REGISTRATION_TOKEN_MAP_PROPERTY_NAME).ToLocalChecked(), tokenMap);
      } else {
        tokenMap = Nan::To<Object>(tokenMapVal).ToLocalChecked();
      }

      Nan::Set(tokenMap, info[0], CreateOpaqueWrapper(::Windows::Foundation::PropertyValue::CreateInt64(registrationToken.Value)));
    }

    static void RemoveListener(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() < 2 || !info[0]->IsString() || !info[1]->IsFunction()) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"wrong arguments, expected a string and a callback")));
        return;
      }

      String::Value eventName(v8::Isolate::GetCurrent(), info[0]);
      auto str = *eventName;

      if ((!NodeRT::Utils::CaseInsenstiveEquals(L"messageReceived", str))) {
        Nan::ThrowError(Nan::Error(String::Concat(v8::Isolate::GetCurrent(), NodeRT::Utils::NewString(L"given event name isn't supported: "), info[0].As<String>())));
        return;
      }

      Local<Function> callback = info[1].As<Function>();
      Local<Value> tokenMap = NodeRT::Utils::GetHiddenValue(callback, Nan::New<String>(REGISTRATION_TOKEN_MAP_PROPERTY_NAME).ToLocalChecked());

      if (tokenMap.IsEmpty() || Nan::Equals(tokenMap, Undefined()).FromMaybe(false)) {
        return;
      }

      Local<Value> opaqueWrapperObj =  Nan::Get(Nan::To<Object>(tokenMap).ToLocalChecked(), info[0]).ToLocalChecked();

      if (opaqueWrapperObj.IsEmpty() || Nan::Equals(opaqueWrapperObj,Undefined()).FromMaybe(false)) {
        return;
      }

      OpaqueWrapper *opaqueWrapper = OpaqueWrapper::Unwrap<OpaqueWrapper>(opaqueWrapperObj.As<Object>());

      long long tokenValue = (long long) opaqueWrapper->GetObjectInstance();
      ::Windows::Foundation::EventRegistrationToken registrationToken;
      registrationToken.Value = tokenValue;

      try  {
        if (NodeRT::Utils::CaseInsenstiveEquals(L"messageReceived", str)) {
          if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointConnection^>(info.This()))
          {
            Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"The caller of this method isn't of the expected type or internal WinRt object was disposed")));
            return;
          }
          MidiEndpointConnection *wrapper = MidiEndpointConnection::Unwrap<MidiEndpointConnection>(info.This());
          wrapper->_instance->MessageReceived::remove(registrationToken);
        }
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
      }

      Nan::Delete(Nan::To<Object>(tokenMap).ToLocalChecked(), Nan::To<String>(info[0]).ToLocalChecked());
    }
    private:
      ::Windows::Devices::Midi2::MidiEndpointConnection^ _instance;
      static Persistent<FunctionTemplate> s_constructorTemplate;

      friend v8::Local<v8::Value> WrapMidiEndpointConnection(::Windows::Devices::Midi2::MidiEndpointConnection^ wintRtInstance);
      friend ::Windows::Devices::Midi2::MidiEndpointConnection^ UnwrapMidiEndpointConnection(Local<Value> value);
  };

  Persistent<FunctionTemplate> MidiEndpointConnection::s_constructorTemplate;

  v8::Local<v8::Value> WrapMidiEndpointConnection(::Windows::Devices::Midi2::MidiEndpointConnection^ winRtInstance) {
    EscapableHandleScope scope;

    if (winRtInstance == nullptr) {
      return scope.Escape(Undefined());
    }

    Local<Value> opaqueWrapper = CreateOpaqueWrapper(winRtInstance);
    Local<Value> args[] = {opaqueWrapper};
    Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(MidiEndpointConnection::s_constructorTemplate);
    return scope.Escape(Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(),_countof(args), args).ToLocalChecked());
  }

  ::Windows::Devices::Midi2::MidiEndpointConnection^ UnwrapMidiEndpointConnection(Local<Value> value) {
     return MidiEndpointConnection::Unwrap<MidiEndpointConnection>(Nan::To<Object>(value).ToLocalChecked())->_instance;
  }

  void InitMidiEndpointConnection(Local<Object> exports) {
    MidiEndpointConnection::Init(exports);
  }

  class MidiEndpointConnectionOptions : public WrapperBase {
    public:
      
      static void Init(const Local<Object> exports) {
        HandleScope scope;

        Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(New);
        s_constructorTemplate.Reset(localRef);
        localRef->SetClassName(Nan::New<String>("MidiEndpointConnectionOptions").ToLocalChecked());
        localRef->InstanceTemplate()->SetInternalFieldCount(1);





          
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("requestedStreamConfiguration").ToLocalChecked(), RequestedStreamConfigurationGetter, RequestedStreamConfigurationSetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("connectionSharingPreference").ToLocalChecked(), ConnectionSharingPreferenceGetter, ConnectionSharingPreferenceSetter);

        Local<Object> constructor = Nan::To<Object>(Nan::GetFunction(localRef).ToLocalChecked()).ToLocalChecked();
        Nan::SetMethod(constructor, "castFrom", CastFrom);



        Nan::Set(exports, Nan::New<String>("MidiEndpointConnectionOptions").ToLocalChecked(), constructor);
      }

      virtual ::Platform::Object^ GetObjectInstance() const override {
        return _instance;
      }

    private:

      MidiEndpointConnectionOptions(::Windows::Devices::Midi2::MidiEndpointConnectionOptions^ instance) {
        _instance = instance;
      }

      
    static void New(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(s_constructorTemplate);

      // in case the constructor was called without the new operator
      if (!localRef->HasInstance(info.This())) {
        if (info.Length() > 0) {
          std::unique_ptr<Local<Value> []> constructorArgs(new Local<Value>[info.Length()]);

          Local<Value> *argsPtr = constructorArgs.get();
          for (int i = 0; i < info.Length(); i++) {
            argsPtr[i] = info[i];
          }

          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), constructorArgs.get());
          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        } else {
          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), nullptr);

          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        }
      }

      ::Windows::Devices::Midi2::MidiEndpointConnectionOptions^ winRtInstance;


      if (info.Length() == 1 && OpaqueWrapper::IsOpaqueWrapper(info[0]) &&
        NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointConnectionOptions^>(info[0])) {
        try {
          winRtInstance = (::Windows::Devices::Midi2::MidiEndpointConnectionOptions^) NodeRT::Utils::GetObjectInstance(info[0]);
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
      else if (info.Length() == 0)
      {
        try {
          winRtInstance = ref new ::Windows::Devices::Midi2::MidiEndpointConnectionOptions();
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no suitable constructor found")));
        return;
      }

      NodeRT::Utils::SetHiddenValue(info.This(), Nan::New<String>("__winRtInstance__").ToLocalChecked(), True());

      MidiEndpointConnectionOptions *wrapperInstance = new MidiEndpointConnectionOptions(winRtInstance);
      wrapperInstance->Wrap(info.This());

      info.GetReturnValue().Set(info.This());
    }


      
    static void CastFrom(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;
      if (info.Length() < 1 || !NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointConnectionOptions^>(info[0])) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no object provided, or given object could not be casted to requested type")));
        return;
      }

      ::Windows::Devices::Midi2::MidiEndpointConnectionOptions^ winRtInstance;
      try {
        winRtInstance = (::Windows::Devices::Midi2::MidiEndpointConnectionOptions^) NodeRT::Utils::GetObjectInstance(info[0]);
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }

      info.GetReturnValue().Set(WrapMidiEndpointConnectionOptions(winRtInstance));
    }





    static void RequestedStreamConfigurationGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointConnectionOptions^>(info.This())) {
        return;
      }

      MidiEndpointConnectionOptions *wrapper = MidiEndpointConnectionOptions::Unwrap<MidiEndpointConnectionOptions>(info.This());

      try  {
        ::Windows::Devices::Midi2::MidiStreamConfigurationRequestedSettings^ result = wrapper->_instance->RequestedStreamConfiguration;
        info.GetReturnValue().Set(WrapMidiStreamConfigurationRequestedSettings(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void RequestedStreamConfigurationSetter(Local<String> property, Local<Value> value, const Nan::PropertyCallbackInfo<void> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiStreamConfigurationRequestedSettings^>(value)) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Value to set is of unexpected type")));
        return;
      }

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointConnectionOptions^>(info.This())) {
        return;
      }

      MidiEndpointConnectionOptions *wrapper = MidiEndpointConnectionOptions::Unwrap<MidiEndpointConnectionOptions>(info.This());

      try {

        ::Windows::Devices::Midi2::MidiStreamConfigurationRequestedSettings^ winRtValue = dynamic_cast<::Windows::Devices::Midi2::MidiStreamConfigurationRequestedSettings^>(NodeRT::Utils::GetObjectInstance(value));

        wrapper->_instance->RequestedStreamConfiguration = winRtValue;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
      }
    }
      
    static void ConnectionSharingPreferenceGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointConnectionOptions^>(info.This())) {
        return;
      }

      MidiEndpointConnectionOptions *wrapper = MidiEndpointConnectionOptions::Unwrap<MidiEndpointConnectionOptions>(info.This());

      try  {
        ::Windows::Devices::Midi2::MidiEndpointConnectionSharingPreference result = wrapper->_instance->ConnectionSharingPreference;
        info.GetReturnValue().Set(Nan::New<Integer>(static_cast<int>(result)));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void ConnectionSharingPreferenceSetter(Local<String> property, Local<Value> value, const Nan::PropertyCallbackInfo<void> &info) {
      HandleScope scope;

      if (!value->IsInt32()) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Value to set is of unexpected type")));
        return;
      }

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointConnectionOptions^>(info.This())) {
        return;
      }

      MidiEndpointConnectionOptions *wrapper = MidiEndpointConnectionOptions::Unwrap<MidiEndpointConnectionOptions>(info.This());

      try {

        ::Windows::Devices::Midi2::MidiEndpointConnectionSharingPreference winRtValue = static_cast<::Windows::Devices::Midi2::MidiEndpointConnectionSharingPreference>(Nan::To<int32_t>(value).FromMaybe(0));

        wrapper->_instance->ConnectionSharingPreference = winRtValue;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
      }
    }
      


    private:
      ::Windows::Devices::Midi2::MidiEndpointConnectionOptions^ _instance;
      static Persistent<FunctionTemplate> s_constructorTemplate;

      friend v8::Local<v8::Value> WrapMidiEndpointConnectionOptions(::Windows::Devices::Midi2::MidiEndpointConnectionOptions^ wintRtInstance);
      friend ::Windows::Devices::Midi2::MidiEndpointConnectionOptions^ UnwrapMidiEndpointConnectionOptions(Local<Value> value);
  };

  Persistent<FunctionTemplate> MidiEndpointConnectionOptions::s_constructorTemplate;

  v8::Local<v8::Value> WrapMidiEndpointConnectionOptions(::Windows::Devices::Midi2::MidiEndpointConnectionOptions^ winRtInstance) {
    EscapableHandleScope scope;

    if (winRtInstance == nullptr) {
      return scope.Escape(Undefined());
    }

    Local<Value> opaqueWrapper = CreateOpaqueWrapper(winRtInstance);
    Local<Value> args[] = {opaqueWrapper};
    Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(MidiEndpointConnectionOptions::s_constructorTemplate);
    return scope.Escape(Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(),_countof(args), args).ToLocalChecked());
  }

  ::Windows::Devices::Midi2::MidiEndpointConnectionOptions^ UnwrapMidiEndpointConnectionOptions(Local<Value> value) {
     return MidiEndpointConnectionOptions::Unwrap<MidiEndpointConnectionOptions>(Nan::To<Object>(value).ToLocalChecked())->_instance;
  }

  void InitMidiEndpointConnectionOptions(Local<Object> exports) {
    MidiEndpointConnectionOptions::Init(exports);
  }

  class MidiEndpointDeviceInformation : public WrapperBase {
    public:
      
      static void Init(const Local<Object> exports) {
        HandleScope scope;

        Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(New);
        s_constructorTemplate.Reset(localRef);
        localRef->SetClassName(Nan::New<String>("MidiEndpointDeviceInformation").ToLocalChecked());
        localRef->InstanceTemplate()->SetInternalFieldCount(1);


          
            Nan::SetPrototypeMethod(localRef, "getParentDeviceInformation", GetParentDeviceInformation);
            Nan::SetPrototypeMethod(localRef, "getContainerInformation", GetContainerInformation);
            Nan::SetPrototypeMethod(localRef, "updateFromDeviceInformation", UpdateFromDeviceInformation);
            Nan::SetPrototypeMethod(localRef, "updateFromDeviceInformationUpdate", UpdateFromDeviceInformationUpdate);
          



          
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("configuredProtocol").ToLocalChecked(), ConfiguredProtocolGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("configuredToReceiveJRTimestamps").ToLocalChecked(), ConfiguredToReceiveJRTimestampsGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("configuredToSendJRTimestamps").ToLocalChecked(), ConfiguredToSendJRTimestampsGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("containerId").ToLocalChecked(), ContainerIdGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("description").ToLocalChecked(), DescriptionGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("deviceIdentityDeviceFamilyLsb").ToLocalChecked(), DeviceIdentityDeviceFamilyLsbGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("deviceIdentityDeviceFamilyModelNumberLsb").ToLocalChecked(), DeviceIdentityDeviceFamilyModelNumberLsbGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("deviceIdentityDeviceFamilyModelNumberMsb").ToLocalChecked(), DeviceIdentityDeviceFamilyModelNumberMsbGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("deviceIdentityDeviceFamilyMsb").ToLocalChecked(), DeviceIdentityDeviceFamilyMsbGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("deviceIdentitySoftwareRevisionLevel").ToLocalChecked(), DeviceIdentitySoftwareRevisionLevelGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("deviceIdentitySystemExclusiveId").ToLocalChecked(), DeviceIdentitySystemExclusiveIdGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("deviceInstanceId").ToLocalChecked(), DeviceInstanceIdGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("endpointPurpose").ToLocalChecked(), EndpointPurposeGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("endpointSuppliedName").ToLocalChecked(), EndpointSuppliedNameGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("functionBlockCount").ToLocalChecked(), FunctionBlockCountGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("functionBlocks").ToLocalChecked(), FunctionBlocksGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("groupTerminalBlocks").ToLocalChecked(), GroupTerminalBlocksGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("hasStaticFunctionBlocks").ToLocalChecked(), HasStaticFunctionBlocksGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("id").ToLocalChecked(), IdGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("largeImagePath").ToLocalChecked(), LargeImagePathGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("manufacturerName").ToLocalChecked(), ManufacturerNameGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("name").ToLocalChecked(), NameGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("nativeDataFormat").ToLocalChecked(), NativeDataFormatGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("productInstanceId").ToLocalChecked(), ProductInstanceIdGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("properties").ToLocalChecked(), PropertiesGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("recommendedCCAutomationIntervalMS").ToLocalChecked(), RecommendedCCAutomationIntervalMSGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("requiresNoteOffTranslation").ToLocalChecked(), RequiresNoteOffTranslationGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("smallImagePath").ToLocalChecked(), SmallImagePathGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("specificationVersionMajor").ToLocalChecked(), SpecificationVersionMajorGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("specificationVersionMinor").ToLocalChecked(), SpecificationVersionMinorGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("supportsMidi10Protocol").ToLocalChecked(), SupportsMidi10ProtocolGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("supportsMidi20Protocol").ToLocalChecked(), SupportsMidi20ProtocolGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("supportsMidiPolyphonicExpression").ToLocalChecked(), SupportsMidiPolyphonicExpressionGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("supportsMultiClient").ToLocalChecked(), SupportsMultiClientGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("supportsReceivingJRTimestamps").ToLocalChecked(), SupportsReceivingJRTimestampsGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("supportsSendingJRTimestamps").ToLocalChecked(), SupportsSendingJRTimestampsGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("transportId").ToLocalChecked(), TransportIdGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("transportMnemonic").ToLocalChecked(), TransportMnemonicGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("transportSuppliedName").ToLocalChecked(), TransportSuppliedNameGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("transportSuppliedSerialNumber").ToLocalChecked(), TransportSuppliedSerialNumberGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("userSuppliedName").ToLocalChecked(), UserSuppliedNameGetter);

        Local<Object> constructor = Nan::To<Object>(Nan::GetFunction(localRef).ToLocalChecked()).ToLocalChecked();
        Nan::SetMethod(constructor, "castFrom", CastFrom);

        Nan::SetMethod(constructor, "getAdditionalPropertiesList", GetAdditionalPropertiesList);
        Nan::SetMethod(constructor, "deviceMatchesFilter", DeviceMatchesFilter);
        Nan::SetMethod(constructor, "createFromId", CreateFromId);
        Nan::SetMethod(constructor, "findAll", FindAll);
        Nan::SetAccessor(constructor, Nan::New<String>("diagnosticsLoopbackAEndpointId").ToLocalChecked(), DiagnosticsLoopbackAEndpointIdGetter);
        Nan::SetAccessor(constructor, Nan::New<String>("diagnosticsLoopbackBEndpointId").ToLocalChecked(), DiagnosticsLoopbackBEndpointIdGetter);
        Nan::SetAccessor(constructor, Nan::New<String>("endpointInterfaceClass").ToLocalChecked(), EndpointInterfaceClassGetter);


        Nan::Set(exports, Nan::New<String>("MidiEndpointDeviceInformation").ToLocalChecked(), constructor);
      }

      virtual ::Platform::Object^ GetObjectInstance() const override {
        return _instance;
      }

    private:

      MidiEndpointDeviceInformation(::Windows::Devices::Midi2::MidiEndpointDeviceInformation^ instance) {
        _instance = instance;
      }

      
    static void New(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(s_constructorTemplate);

      // in case the constructor was called without the new operator
      if (!localRef->HasInstance(info.This())) {
        if (info.Length() > 0) {
          std::unique_ptr<Local<Value> []> constructorArgs(new Local<Value>[info.Length()]);

          Local<Value> *argsPtr = constructorArgs.get();
          for (int i = 0; i < info.Length(); i++) {
            argsPtr[i] = info[i];
          }

          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), constructorArgs.get());
          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        } else {
          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), nullptr);

          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        }
      }

      ::Windows::Devices::Midi2::MidiEndpointDeviceInformation^ winRtInstance;


      if (info.Length() == 1 && OpaqueWrapper::IsOpaqueWrapper(info[0]) &&
        NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceInformation^>(info[0])) {
        try {
          winRtInstance = (::Windows::Devices::Midi2::MidiEndpointDeviceInformation^) NodeRT::Utils::GetObjectInstance(info[0]);
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no suitable constructor found")));
        return;
      }

      NodeRT::Utils::SetHiddenValue(info.This(), Nan::New<String>("__winRtInstance__").ToLocalChecked(), True());

      MidiEndpointDeviceInformation *wrapperInstance = new MidiEndpointDeviceInformation(winRtInstance);
      wrapperInstance->Wrap(info.This());

      info.GetReturnValue().Set(info.This());
    }


      
    static void CastFrom(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;
      if (info.Length() < 1 || !NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceInformation^>(info[0])) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no object provided, or given object could not be casted to requested type")));
        return;
      }

      ::Windows::Devices::Midi2::MidiEndpointDeviceInformation^ winRtInstance;
      try {
        winRtInstance = (::Windows::Devices::Midi2::MidiEndpointDeviceInformation^) NodeRT::Utils::GetObjectInstance(info[0]);
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }

      info.GetReturnValue().Set(WrapMidiEndpointDeviceInformation(winRtInstance));
    }


    static void GetParentDeviceInformation(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceInformation^>(info.This())) {
        return;
      }

      MidiEndpointDeviceInformation *wrapper = MidiEndpointDeviceInformation::Unwrap<MidiEndpointDeviceInformation>(info.This());

      if (info.Length() == 0)
      {
        try
        {
          ::Windows::Devices::Enumeration::DeviceInformation^ result;
          result = wrapper->_instance->GetParentDeviceInformation();
          info.GetReturnValue().Set(NodeRT::Utils::CreateExternalWinRTObject("Windows.Devices.Enumeration", "DeviceInformation", result));
          return;
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }
    static void GetContainerInformation(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceInformation^>(info.This())) {
        return;
      }

      MidiEndpointDeviceInformation *wrapper = MidiEndpointDeviceInformation::Unwrap<MidiEndpointDeviceInformation>(info.This());

      if (info.Length() == 0)
      {
        try
        {
          ::Windows::Devices::Enumeration::DeviceInformation^ result;
          result = wrapper->_instance->GetContainerInformation();
          info.GetReturnValue().Set(NodeRT::Utils::CreateExternalWinRTObject("Windows.Devices.Enumeration", "DeviceInformation", result));
          return;
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }
    static void UpdateFromDeviceInformation(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceInformation^>(info.This())) {
        return;
      }

      MidiEndpointDeviceInformation *wrapper = MidiEndpointDeviceInformation::Unwrap<MidiEndpointDeviceInformation>(info.This());

      if (info.Length() == 1
        && NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Enumeration::DeviceInformation^>(info[0]))
      {
        try
        {
          ::Windows::Devices::Enumeration::DeviceInformation^ arg0 = dynamic_cast<::Windows::Devices::Enumeration::DeviceInformation^>(NodeRT::Utils::GetObjectInstance(info[0]));
          
          bool result;
          result = wrapper->_instance->UpdateFromDeviceInformation(arg0);
          info.GetReturnValue().Set(Nan::New<Boolean>(result));
          return;
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }
    static void UpdateFromDeviceInformationUpdate(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceInformation^>(info.This())) {
        return;
      }

      MidiEndpointDeviceInformation *wrapper = MidiEndpointDeviceInformation::Unwrap<MidiEndpointDeviceInformation>(info.This());

      if (info.Length() == 1
        && NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Enumeration::DeviceInformationUpdate^>(info[0]))
      {
        try
        {
          ::Windows::Devices::Enumeration::DeviceInformationUpdate^ arg0 = dynamic_cast<::Windows::Devices::Enumeration::DeviceInformationUpdate^>(NodeRT::Utils::GetObjectInstance(info[0]));
          
          bool result;
          result = wrapper->_instance->UpdateFromDeviceInformationUpdate(arg0);
          info.GetReturnValue().Set(Nan::New<Boolean>(result));
          return;
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }



    static void GetAdditionalPropertiesList(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 0)
      {
        try
        {
          ::Windows::Foundation::Collections::IVectorView<::Platform::String^>^ result;
          result = ::Windows::Devices::Midi2::MidiEndpointDeviceInformation::GetAdditionalPropertiesList();
          info.GetReturnValue().Set(NodeRT::Collections::VectorViewWrapper<::Platform::String^>::CreateVectorViewWrapper(result, 
            [](::Platform::String^ val) -> Local<Value> {
              return NodeRT::Utils::NewString(val->Data());
            },
            [](Local<Value> value) -> bool {
              return value->IsString();
            },
            [](Local<Value> value) -> ::Platform::String^ {
              return ref new Platform::String(NodeRT::Utils::StringToWchar(v8::String::Value(v8::Isolate::GetCurrent(), value)));
            }
          ));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void DeviceMatchesFilter(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 2
        && NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceInformation^>(info[0])
        && info[1]->IsInt32())
      {
        try
        {
          ::Windows::Devices::Midi2::MidiEndpointDeviceInformation^ arg0 = UnwrapMidiEndpointDeviceInformation(info[0]);
          ::Windows::Devices::Midi2::MidiEndpointDeviceInformationFilter arg1 = static_cast<::Windows::Devices::Midi2::MidiEndpointDeviceInformationFilter>(Nan::To<int32_t>(info[1]).FromMaybe(0));
          
          bool result;
          result = ::Windows::Devices::Midi2::MidiEndpointDeviceInformation::DeviceMatchesFilter(arg0, arg1);
          info.GetReturnValue().Set(Nan::New<Boolean>(result));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void CreateFromId(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 1
        && info[0]->IsString())
      {
        try
        {
          Platform::String^ arg0 = ref new Platform::String(NodeRT::Utils::StringToWchar(v8::String::Value(v8::Isolate::GetCurrent(), info[0])));
          
          ::Windows::Devices::Midi2::MidiEndpointDeviceInformation^ result;
          result = ::Windows::Devices::Midi2::MidiEndpointDeviceInformation::CreateFromId(arg0);
          info.GetReturnValue().Set(WrapMidiEndpointDeviceInformation(result));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void FindAll(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 0)
      {
        try
        {
          ::Windows::Foundation::Collections::IVectorView<::Windows::Devices::Midi2::MidiEndpointDeviceInformation^>^ result;
          result = ::Windows::Devices::Midi2::MidiEndpointDeviceInformation::FindAll();
          info.GetReturnValue().Set(NodeRT::Collections::VectorViewWrapper<::Windows::Devices::Midi2::MidiEndpointDeviceInformation^>::CreateVectorViewWrapper(result, 
            [](::Windows::Devices::Midi2::MidiEndpointDeviceInformation^ val) -> Local<Value> {
              return WrapMidiEndpointDeviceInformation(val);
            },
            [](Local<Value> value) -> bool {
              return NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceInformation^>(value);
            },
            [](Local<Value> value) -> ::Windows::Devices::Midi2::MidiEndpointDeviceInformation^ {
              return UnwrapMidiEndpointDeviceInformation(value);
            }
          ));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
      else if (info.Length() == 1
        && info[0]->IsInt32())
      {
        try
        {
          ::Windows::Devices::Midi2::MidiEndpointDeviceInformationSortOrder arg0 = static_cast<::Windows::Devices::Midi2::MidiEndpointDeviceInformationSortOrder>(Nan::To<int32_t>(info[0]).FromMaybe(0));
          
          ::Windows::Foundation::Collections::IVectorView<::Windows::Devices::Midi2::MidiEndpointDeviceInformation^>^ result;
          result = ::Windows::Devices::Midi2::MidiEndpointDeviceInformation::FindAll(arg0);
          info.GetReturnValue().Set(NodeRT::Collections::VectorViewWrapper<::Windows::Devices::Midi2::MidiEndpointDeviceInformation^>::CreateVectorViewWrapper(result, 
            [](::Windows::Devices::Midi2::MidiEndpointDeviceInformation^ val) -> Local<Value> {
              return WrapMidiEndpointDeviceInformation(val);
            },
            [](Local<Value> value) -> bool {
              return NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceInformation^>(value);
            },
            [](Local<Value> value) -> ::Windows::Devices::Midi2::MidiEndpointDeviceInformation^ {
              return UnwrapMidiEndpointDeviceInformation(value);
            }
          ));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
      else if (info.Length() == 2
        && info[0]->IsInt32()
        && info[1]->IsInt32())
      {
        try
        {
          ::Windows::Devices::Midi2::MidiEndpointDeviceInformationSortOrder arg0 = static_cast<::Windows::Devices::Midi2::MidiEndpointDeviceInformationSortOrder>(Nan::To<int32_t>(info[0]).FromMaybe(0));
          ::Windows::Devices::Midi2::MidiEndpointDeviceInformationFilter arg1 = static_cast<::Windows::Devices::Midi2::MidiEndpointDeviceInformationFilter>(Nan::To<int32_t>(info[1]).FromMaybe(0));
          
          ::Windows::Foundation::Collections::IVectorView<::Windows::Devices::Midi2::MidiEndpointDeviceInformation^>^ result;
          result = ::Windows::Devices::Midi2::MidiEndpointDeviceInformation::FindAll(arg0, arg1);
          info.GetReturnValue().Set(NodeRT::Collections::VectorViewWrapper<::Windows::Devices::Midi2::MidiEndpointDeviceInformation^>::CreateVectorViewWrapper(result, 
            [](::Windows::Devices::Midi2::MidiEndpointDeviceInformation^ val) -> Local<Value> {
              return WrapMidiEndpointDeviceInformation(val);
            },
            [](Local<Value> value) -> bool {
              return NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceInformation^>(value);
            },
            [](Local<Value> value) -> ::Windows::Devices::Midi2::MidiEndpointDeviceInformation^ {
              return UnwrapMidiEndpointDeviceInformation(value);
            }
          ));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void ConfiguredProtocolGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceInformation^>(info.This())) {
        return;
      }

      MidiEndpointDeviceInformation *wrapper = MidiEndpointDeviceInformation::Unwrap<MidiEndpointDeviceInformation>(info.This());

      try  {
        ::Windows::Devices::Midi2::MidiProtocol result = wrapper->_instance->ConfiguredProtocol;
        info.GetReturnValue().Set(Nan::New<Integer>(static_cast<int>(result)));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void ConfiguredToReceiveJRTimestampsGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceInformation^>(info.This())) {
        return;
      }

      MidiEndpointDeviceInformation *wrapper = MidiEndpointDeviceInformation::Unwrap<MidiEndpointDeviceInformation>(info.This());

      try  {
        bool result = wrapper->_instance->ConfiguredToReceiveJRTimestamps;
        info.GetReturnValue().Set(Nan::New<Boolean>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void ConfiguredToSendJRTimestampsGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceInformation^>(info.This())) {
        return;
      }

      MidiEndpointDeviceInformation *wrapper = MidiEndpointDeviceInformation::Unwrap<MidiEndpointDeviceInformation>(info.This());

      try  {
        bool result = wrapper->_instance->ConfiguredToSendJRTimestamps;
        info.GetReturnValue().Set(Nan::New<Boolean>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void ContainerIdGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceInformation^>(info.This())) {
        return;
      }

      MidiEndpointDeviceInformation *wrapper = MidiEndpointDeviceInformation::Unwrap<MidiEndpointDeviceInformation>(info.This());

      try  {
        ::Platform::Guid result = wrapper->_instance->ContainerId;
        info.GetReturnValue().Set(NodeRT::Utils::GuidToJs(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void DescriptionGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceInformation^>(info.This())) {
        return;
      }

      MidiEndpointDeviceInformation *wrapper = MidiEndpointDeviceInformation::Unwrap<MidiEndpointDeviceInformation>(info.This());

      try  {
        Platform::String^ result = wrapper->_instance->Description;
        info.GetReturnValue().Set(NodeRT::Utils::NewString(result->Data()));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void DeviceIdentityDeviceFamilyLsbGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceInformation^>(info.This())) {
        return;
      }

      MidiEndpointDeviceInformation *wrapper = MidiEndpointDeviceInformation::Unwrap<MidiEndpointDeviceInformation>(info.This());

      try  {
        unsigned char result = wrapper->_instance->DeviceIdentityDeviceFamilyLsb;
        info.GetReturnValue().Set(Nan::New<Integer>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void DeviceIdentityDeviceFamilyModelNumberLsbGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceInformation^>(info.This())) {
        return;
      }

      MidiEndpointDeviceInformation *wrapper = MidiEndpointDeviceInformation::Unwrap<MidiEndpointDeviceInformation>(info.This());

      try  {
        unsigned char result = wrapper->_instance->DeviceIdentityDeviceFamilyModelNumberLsb;
        info.GetReturnValue().Set(Nan::New<Integer>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void DeviceIdentityDeviceFamilyModelNumberMsbGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceInformation^>(info.This())) {
        return;
      }

      MidiEndpointDeviceInformation *wrapper = MidiEndpointDeviceInformation::Unwrap<MidiEndpointDeviceInformation>(info.This());

      try  {
        unsigned char result = wrapper->_instance->DeviceIdentityDeviceFamilyModelNumberMsb;
        info.GetReturnValue().Set(Nan::New<Integer>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void DeviceIdentityDeviceFamilyMsbGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceInformation^>(info.This())) {
        return;
      }

      MidiEndpointDeviceInformation *wrapper = MidiEndpointDeviceInformation::Unwrap<MidiEndpointDeviceInformation>(info.This());

      try  {
        unsigned char result = wrapper->_instance->DeviceIdentityDeviceFamilyMsb;
        info.GetReturnValue().Set(Nan::New<Integer>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void DeviceIdentitySoftwareRevisionLevelGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceInformation^>(info.This())) {
        return;
      }

      MidiEndpointDeviceInformation *wrapper = MidiEndpointDeviceInformation::Unwrap<MidiEndpointDeviceInformation>(info.This());

      try  {
        ::Platform::Array<unsigned char>^ result = wrapper->_instance->DeviceIdentitySoftwareRevisionLevel;
        info.GetReturnValue().Set(NodeRT::Collections::ArrayWrapper<unsigned char>::CreateArrayWrapper(result, 
            [](unsigned char val) -> Local<Value> {
              return Nan::New<Integer>(val);
            },
            [](Local<Value> value) -> bool {
              return value->IsInt32();
            },
            [](Local<Value> value) -> unsigned char {
              return static_cast<unsigned char>(Nan::To<int32_t>(value).FromMaybe(0));
            }
          ));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void DeviceIdentitySystemExclusiveIdGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceInformation^>(info.This())) {
        return;
      }

      MidiEndpointDeviceInformation *wrapper = MidiEndpointDeviceInformation::Unwrap<MidiEndpointDeviceInformation>(info.This());

      try  {
        ::Platform::Array<unsigned char>^ result = wrapper->_instance->DeviceIdentitySystemExclusiveId;
        info.GetReturnValue().Set(NodeRT::Collections::ArrayWrapper<unsigned char>::CreateArrayWrapper(result, 
            [](unsigned char val) -> Local<Value> {
              return Nan::New<Integer>(val);
            },
            [](Local<Value> value) -> bool {
              return value->IsInt32();
            },
            [](Local<Value> value) -> unsigned char {
              return static_cast<unsigned char>(Nan::To<int32_t>(value).FromMaybe(0));
            }
          ));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void DeviceInstanceIdGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceInformation^>(info.This())) {
        return;
      }

      MidiEndpointDeviceInformation *wrapper = MidiEndpointDeviceInformation::Unwrap<MidiEndpointDeviceInformation>(info.This());

      try  {
        Platform::String^ result = wrapper->_instance->DeviceInstanceId;
        info.GetReturnValue().Set(NodeRT::Utils::NewString(result->Data()));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void EndpointPurposeGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceInformation^>(info.This())) {
        return;
      }

      MidiEndpointDeviceInformation *wrapper = MidiEndpointDeviceInformation::Unwrap<MidiEndpointDeviceInformation>(info.This());

      try  {
        ::Windows::Devices::Midi2::MidiEndpointDevicePurpose result = wrapper->_instance->EndpointPurpose;
        info.GetReturnValue().Set(Nan::New<Integer>(static_cast<int>(result)));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void EndpointSuppliedNameGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceInformation^>(info.This())) {
        return;
      }

      MidiEndpointDeviceInformation *wrapper = MidiEndpointDeviceInformation::Unwrap<MidiEndpointDeviceInformation>(info.This());

      try  {
        Platform::String^ result = wrapper->_instance->EndpointSuppliedName;
        info.GetReturnValue().Set(NodeRT::Utils::NewString(result->Data()));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void FunctionBlockCountGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceInformation^>(info.This())) {
        return;
      }

      MidiEndpointDeviceInformation *wrapper = MidiEndpointDeviceInformation::Unwrap<MidiEndpointDeviceInformation>(info.This());

      try  {
        unsigned char result = wrapper->_instance->FunctionBlockCount;
        info.GetReturnValue().Set(Nan::New<Integer>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void FunctionBlocksGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceInformation^>(info.This())) {
        return;
      }

      MidiEndpointDeviceInformation *wrapper = MidiEndpointDeviceInformation::Unwrap<MidiEndpointDeviceInformation>(info.This());

      try  {
        ::Windows::Foundation::Collections::IMapView<unsigned char, ::Windows::Devices::Midi2::MidiFunctionBlock^>^ result = wrapper->_instance->FunctionBlocks;
        info.GetReturnValue().Set(NodeRT::Collections::MapViewWrapper<unsigned char,::Windows::Devices::Midi2::MidiFunctionBlock^>::CreateMapViewWrapper(result, 
            [](unsigned char val) -> Local<Value> {
              return Nan::New<Integer>(val);
            },
            [](Local<Value> value) -> bool {
              return value->IsInt32();
            },
            [](Local<Value> value) -> unsigned char {
              return static_cast<unsigned char>(Nan::To<int32_t>(value).FromMaybe(0));
            },
            [](::Windows::Devices::Midi2::MidiFunctionBlock^ val) -> Local<Value> {
              return WrapMidiFunctionBlock(val);
            }
          ));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void GroupTerminalBlocksGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceInformation^>(info.This())) {
        return;
      }

      MidiEndpointDeviceInformation *wrapper = MidiEndpointDeviceInformation::Unwrap<MidiEndpointDeviceInformation>(info.This());

      try  {
        ::Windows::Foundation::Collections::IVectorView<::Windows::Devices::Midi2::MidiGroupTerminalBlock^>^ result = wrapper->_instance->GroupTerminalBlocks;
        info.GetReturnValue().Set(NodeRT::Collections::VectorViewWrapper<::Windows::Devices::Midi2::MidiGroupTerminalBlock^>::CreateVectorViewWrapper(result, 
            [](::Windows::Devices::Midi2::MidiGroupTerminalBlock^ val) -> Local<Value> {
              return WrapMidiGroupTerminalBlock(val);
            },
            [](Local<Value> value) -> bool {
              return NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiGroupTerminalBlock^>(value);
            },
            [](Local<Value> value) -> ::Windows::Devices::Midi2::MidiGroupTerminalBlock^ {
              return UnwrapMidiGroupTerminalBlock(value);
            }
          ));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void HasStaticFunctionBlocksGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceInformation^>(info.This())) {
        return;
      }

      MidiEndpointDeviceInformation *wrapper = MidiEndpointDeviceInformation::Unwrap<MidiEndpointDeviceInformation>(info.This());

      try  {
        bool result = wrapper->_instance->HasStaticFunctionBlocks;
        info.GetReturnValue().Set(Nan::New<Boolean>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void IdGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceInformation^>(info.This())) {
        return;
      }

      MidiEndpointDeviceInformation *wrapper = MidiEndpointDeviceInformation::Unwrap<MidiEndpointDeviceInformation>(info.This());

      try  {
        Platform::String^ result = wrapper->_instance->Id;
        info.GetReturnValue().Set(NodeRT::Utils::NewString(result->Data()));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void LargeImagePathGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceInformation^>(info.This())) {
        return;
      }

      MidiEndpointDeviceInformation *wrapper = MidiEndpointDeviceInformation::Unwrap<MidiEndpointDeviceInformation>(info.This());

      try  {
        Platform::String^ result = wrapper->_instance->LargeImagePath;
        info.GetReturnValue().Set(NodeRT::Utils::NewString(result->Data()));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void ManufacturerNameGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceInformation^>(info.This())) {
        return;
      }

      MidiEndpointDeviceInformation *wrapper = MidiEndpointDeviceInformation::Unwrap<MidiEndpointDeviceInformation>(info.This());

      try  {
        Platform::String^ result = wrapper->_instance->ManufacturerName;
        info.GetReturnValue().Set(NodeRT::Utils::NewString(result->Data()));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void NameGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceInformation^>(info.This())) {
        return;
      }

      MidiEndpointDeviceInformation *wrapper = MidiEndpointDeviceInformation::Unwrap<MidiEndpointDeviceInformation>(info.This());

      try  {
        Platform::String^ result = wrapper->_instance->Name;
        info.GetReturnValue().Set(NodeRT::Utils::NewString(result->Data()));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void NativeDataFormatGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceInformation^>(info.This())) {
        return;
      }

      MidiEndpointDeviceInformation *wrapper = MidiEndpointDeviceInformation::Unwrap<MidiEndpointDeviceInformation>(info.This());

      try  {
        ::Windows::Devices::Midi2::MidiEndpointNativeDataFormat result = wrapper->_instance->NativeDataFormat;
        info.GetReturnValue().Set(Nan::New<Integer>(static_cast<int>(result)));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void ProductInstanceIdGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceInformation^>(info.This())) {
        return;
      }

      MidiEndpointDeviceInformation *wrapper = MidiEndpointDeviceInformation::Unwrap<MidiEndpointDeviceInformation>(info.This());

      try  {
        Platform::String^ result = wrapper->_instance->ProductInstanceId;
        info.GetReturnValue().Set(NodeRT::Utils::NewString(result->Data()));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void PropertiesGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceInformation^>(info.This())) {
        return;
      }

      MidiEndpointDeviceInformation *wrapper = MidiEndpointDeviceInformation::Unwrap<MidiEndpointDeviceInformation>(info.This());

      try  {
        ::Windows::Foundation::Collections::IMapView<::Platform::String^, ::Platform::Object^>^ result = wrapper->_instance->Properties;
        info.GetReturnValue().Set(NodeRT::Collections::MapViewWrapper<::Platform::String^,::Platform::Object^>::CreateMapViewWrapper(result, 
            [](::Platform::String^ val) -> Local<Value> {
              return NodeRT::Utils::NewString(val->Data());
            },
            [](Local<Value> value) -> bool {
              return value->IsString();
            },
            [](Local<Value> value) -> ::Platform::String^ {
              return ref new Platform::String(NodeRT::Utils::StringToWchar(v8::String::Value(v8::Isolate::GetCurrent(), value)));
            },
            [](::Platform::Object^ val) -> Local<Value> {
              return CreateOpaqueWrapper(val);
            }
          ));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void RecommendedCCAutomationIntervalMSGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceInformation^>(info.This())) {
        return;
      }

      MidiEndpointDeviceInformation *wrapper = MidiEndpointDeviceInformation::Unwrap<MidiEndpointDeviceInformation>(info.This());

      try  {
        unsigned short result = wrapper->_instance->RecommendedCCAutomationIntervalMS;
        info.GetReturnValue().Set(Nan::New<Integer>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void RequiresNoteOffTranslationGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceInformation^>(info.This())) {
        return;
      }

      MidiEndpointDeviceInformation *wrapper = MidiEndpointDeviceInformation::Unwrap<MidiEndpointDeviceInformation>(info.This());

      try  {
        bool result = wrapper->_instance->RequiresNoteOffTranslation;
        info.GetReturnValue().Set(Nan::New<Boolean>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void SmallImagePathGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceInformation^>(info.This())) {
        return;
      }

      MidiEndpointDeviceInformation *wrapper = MidiEndpointDeviceInformation::Unwrap<MidiEndpointDeviceInformation>(info.This());

      try  {
        Platform::String^ result = wrapper->_instance->SmallImagePath;
        info.GetReturnValue().Set(NodeRT::Utils::NewString(result->Data()));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void SpecificationVersionMajorGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceInformation^>(info.This())) {
        return;
      }

      MidiEndpointDeviceInformation *wrapper = MidiEndpointDeviceInformation::Unwrap<MidiEndpointDeviceInformation>(info.This());

      try  {
        unsigned char result = wrapper->_instance->SpecificationVersionMajor;
        info.GetReturnValue().Set(Nan::New<Integer>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void SpecificationVersionMinorGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceInformation^>(info.This())) {
        return;
      }

      MidiEndpointDeviceInformation *wrapper = MidiEndpointDeviceInformation::Unwrap<MidiEndpointDeviceInformation>(info.This());

      try  {
        unsigned char result = wrapper->_instance->SpecificationVersionMinor;
        info.GetReturnValue().Set(Nan::New<Integer>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void SupportsMidi10ProtocolGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceInformation^>(info.This())) {
        return;
      }

      MidiEndpointDeviceInformation *wrapper = MidiEndpointDeviceInformation::Unwrap<MidiEndpointDeviceInformation>(info.This());

      try  {
        bool result = wrapper->_instance->SupportsMidi10Protocol;
        info.GetReturnValue().Set(Nan::New<Boolean>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void SupportsMidi20ProtocolGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceInformation^>(info.This())) {
        return;
      }

      MidiEndpointDeviceInformation *wrapper = MidiEndpointDeviceInformation::Unwrap<MidiEndpointDeviceInformation>(info.This());

      try  {
        bool result = wrapper->_instance->SupportsMidi20Protocol;
        info.GetReturnValue().Set(Nan::New<Boolean>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void SupportsMidiPolyphonicExpressionGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceInformation^>(info.This())) {
        return;
      }

      MidiEndpointDeviceInformation *wrapper = MidiEndpointDeviceInformation::Unwrap<MidiEndpointDeviceInformation>(info.This());

      try  {
        bool result = wrapper->_instance->SupportsMidiPolyphonicExpression;
        info.GetReturnValue().Set(Nan::New<Boolean>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void SupportsMultiClientGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceInformation^>(info.This())) {
        return;
      }

      MidiEndpointDeviceInformation *wrapper = MidiEndpointDeviceInformation::Unwrap<MidiEndpointDeviceInformation>(info.This());

      try  {
        bool result = wrapper->_instance->SupportsMultiClient;
        info.GetReturnValue().Set(Nan::New<Boolean>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void SupportsReceivingJRTimestampsGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceInformation^>(info.This())) {
        return;
      }

      MidiEndpointDeviceInformation *wrapper = MidiEndpointDeviceInformation::Unwrap<MidiEndpointDeviceInformation>(info.This());

      try  {
        bool result = wrapper->_instance->SupportsReceivingJRTimestamps;
        info.GetReturnValue().Set(Nan::New<Boolean>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void SupportsSendingJRTimestampsGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceInformation^>(info.This())) {
        return;
      }

      MidiEndpointDeviceInformation *wrapper = MidiEndpointDeviceInformation::Unwrap<MidiEndpointDeviceInformation>(info.This());

      try  {
        bool result = wrapper->_instance->SupportsSendingJRTimestamps;
        info.GetReturnValue().Set(Nan::New<Boolean>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void TransportIdGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceInformation^>(info.This())) {
        return;
      }

      MidiEndpointDeviceInformation *wrapper = MidiEndpointDeviceInformation::Unwrap<MidiEndpointDeviceInformation>(info.This());

      try  {
        Platform::String^ result = wrapper->_instance->TransportId;
        info.GetReturnValue().Set(NodeRT::Utils::NewString(result->Data()));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void TransportMnemonicGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceInformation^>(info.This())) {
        return;
      }

      MidiEndpointDeviceInformation *wrapper = MidiEndpointDeviceInformation::Unwrap<MidiEndpointDeviceInformation>(info.This());

      try  {
        Platform::String^ result = wrapper->_instance->TransportMnemonic;
        info.GetReturnValue().Set(NodeRT::Utils::NewString(result->Data()));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void TransportSuppliedNameGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceInformation^>(info.This())) {
        return;
      }

      MidiEndpointDeviceInformation *wrapper = MidiEndpointDeviceInformation::Unwrap<MidiEndpointDeviceInformation>(info.This());

      try  {
        Platform::String^ result = wrapper->_instance->TransportSuppliedName;
        info.GetReturnValue().Set(NodeRT::Utils::NewString(result->Data()));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void TransportSuppliedSerialNumberGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceInformation^>(info.This())) {
        return;
      }

      MidiEndpointDeviceInformation *wrapper = MidiEndpointDeviceInformation::Unwrap<MidiEndpointDeviceInformation>(info.This());

      try  {
        Platform::String^ result = wrapper->_instance->TransportSuppliedSerialNumber;
        info.GetReturnValue().Set(NodeRT::Utils::NewString(result->Data()));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void UserSuppliedNameGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceInformation^>(info.This())) {
        return;
      }

      MidiEndpointDeviceInformation *wrapper = MidiEndpointDeviceInformation::Unwrap<MidiEndpointDeviceInformation>(info.This());

      try  {
        Platform::String^ result = wrapper->_instance->UserSuppliedName;
        info.GetReturnValue().Set(NodeRT::Utils::NewString(result->Data()));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      


    static void DiagnosticsLoopbackAEndpointIdGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      try
      {
        Platform::String^ result = ::Windows::Devices::Midi2::MidiEndpointDeviceInformation::DiagnosticsLoopbackAEndpointId;
        info.GetReturnValue().Set(NodeRT::Utils::NewString(result->Data()));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      

    static void DiagnosticsLoopbackBEndpointIdGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      try
      {
        Platform::String^ result = ::Windows::Devices::Midi2::MidiEndpointDeviceInformation::DiagnosticsLoopbackBEndpointId;
        info.GetReturnValue().Set(NodeRT::Utils::NewString(result->Data()));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      

    static void EndpointInterfaceClassGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      try
      {
        Platform::String^ result = ::Windows::Devices::Midi2::MidiEndpointDeviceInformation::EndpointInterfaceClass;
        info.GetReturnValue().Set(NodeRT::Utils::NewString(result->Data()));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      

    private:
      ::Windows::Devices::Midi2::MidiEndpointDeviceInformation^ _instance;
      static Persistent<FunctionTemplate> s_constructorTemplate;

      friend v8::Local<v8::Value> WrapMidiEndpointDeviceInformation(::Windows::Devices::Midi2::MidiEndpointDeviceInformation^ wintRtInstance);
      friend ::Windows::Devices::Midi2::MidiEndpointDeviceInformation^ UnwrapMidiEndpointDeviceInformation(Local<Value> value);
  };

  Persistent<FunctionTemplate> MidiEndpointDeviceInformation::s_constructorTemplate;

  v8::Local<v8::Value> WrapMidiEndpointDeviceInformation(::Windows::Devices::Midi2::MidiEndpointDeviceInformation^ winRtInstance) {
    EscapableHandleScope scope;

    if (winRtInstance == nullptr) {
      return scope.Escape(Undefined());
    }

    Local<Value> opaqueWrapper = CreateOpaqueWrapper(winRtInstance);
    Local<Value> args[] = {opaqueWrapper};
    Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(MidiEndpointDeviceInformation::s_constructorTemplate);
    return scope.Escape(Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(),_countof(args), args).ToLocalChecked());
  }

  ::Windows::Devices::Midi2::MidiEndpointDeviceInformation^ UnwrapMidiEndpointDeviceInformation(Local<Value> value) {
     return MidiEndpointDeviceInformation::Unwrap<MidiEndpointDeviceInformation>(Nan::To<Object>(value).ToLocalChecked())->_instance;
  }

  void InitMidiEndpointDeviceInformation(Local<Object> exports) {
    MidiEndpointDeviceInformation::Init(exports);
  }

  class MidiEndpointDeviceInformationUpdateEventArgs : public WrapperBase {
    public:
      
      static void Init(const Local<Object> exports) {
        HandleScope scope;

        Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(New);
        s_constructorTemplate.Reset(localRef);
        localRef->SetClassName(Nan::New<String>("MidiEndpointDeviceInformationUpdateEventArgs").ToLocalChecked());
        localRef->InstanceTemplate()->SetInternalFieldCount(1);





          
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("deviceInformationUpdate").ToLocalChecked(), DeviceInformationUpdateGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("id").ToLocalChecked(), IdGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("updatedAdditionalCapabilities").ToLocalChecked(), UpdatedAdditionalCapabilitiesGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("updatedDeviceIdentity").ToLocalChecked(), UpdatedDeviceIdentityGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("updatedEndpointInformation").ToLocalChecked(), UpdatedEndpointInformationGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("updatedFunctionBlocks").ToLocalChecked(), UpdatedFunctionBlocksGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("updatedName").ToLocalChecked(), UpdatedNameGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("updatedStreamConfiguration").ToLocalChecked(), UpdatedStreamConfigurationGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("updatedUserMetadata").ToLocalChecked(), UpdatedUserMetadataGetter);

        Local<Object> constructor = Nan::To<Object>(Nan::GetFunction(localRef).ToLocalChecked()).ToLocalChecked();
        Nan::SetMethod(constructor, "castFrom", CastFrom);



        Nan::Set(exports, Nan::New<String>("MidiEndpointDeviceInformationUpdateEventArgs").ToLocalChecked(), constructor);
      }

      virtual ::Platform::Object^ GetObjectInstance() const override {
        return _instance;
      }

    private:

      MidiEndpointDeviceInformationUpdateEventArgs(::Windows::Devices::Midi2::MidiEndpointDeviceInformationUpdateEventArgs^ instance) {
        _instance = instance;
      }

      
    static void New(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(s_constructorTemplate);

      // in case the constructor was called without the new operator
      if (!localRef->HasInstance(info.This())) {
        if (info.Length() > 0) {
          std::unique_ptr<Local<Value> []> constructorArgs(new Local<Value>[info.Length()]);

          Local<Value> *argsPtr = constructorArgs.get();
          for (int i = 0; i < info.Length(); i++) {
            argsPtr[i] = info[i];
          }

          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), constructorArgs.get());
          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        } else {
          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), nullptr);

          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        }
      }

      ::Windows::Devices::Midi2::MidiEndpointDeviceInformationUpdateEventArgs^ winRtInstance;


      if (info.Length() == 1 && OpaqueWrapper::IsOpaqueWrapper(info[0]) &&
        NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceInformationUpdateEventArgs^>(info[0])) {
        try {
          winRtInstance = (::Windows::Devices::Midi2::MidiEndpointDeviceInformationUpdateEventArgs^) NodeRT::Utils::GetObjectInstance(info[0]);
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no suitable constructor found")));
        return;
      }

      NodeRT::Utils::SetHiddenValue(info.This(), Nan::New<String>("__winRtInstance__").ToLocalChecked(), True());

      MidiEndpointDeviceInformationUpdateEventArgs *wrapperInstance = new MidiEndpointDeviceInformationUpdateEventArgs(winRtInstance);
      wrapperInstance->Wrap(info.This());

      info.GetReturnValue().Set(info.This());
    }


      
    static void CastFrom(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;
      if (info.Length() < 1 || !NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceInformationUpdateEventArgs^>(info[0])) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no object provided, or given object could not be casted to requested type")));
        return;
      }

      ::Windows::Devices::Midi2::MidiEndpointDeviceInformationUpdateEventArgs^ winRtInstance;
      try {
        winRtInstance = (::Windows::Devices::Midi2::MidiEndpointDeviceInformationUpdateEventArgs^) NodeRT::Utils::GetObjectInstance(info[0]);
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }

      info.GetReturnValue().Set(WrapMidiEndpointDeviceInformationUpdateEventArgs(winRtInstance));
    }





    static void DeviceInformationUpdateGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceInformationUpdateEventArgs^>(info.This())) {
        return;
      }

      MidiEndpointDeviceInformationUpdateEventArgs *wrapper = MidiEndpointDeviceInformationUpdateEventArgs::Unwrap<MidiEndpointDeviceInformationUpdateEventArgs>(info.This());

      try  {
        ::Windows::Devices::Enumeration::DeviceInformationUpdate^ result = wrapper->_instance->DeviceInformationUpdate;
        info.GetReturnValue().Set(NodeRT::Utils::CreateExternalWinRTObject("Windows.Devices.Enumeration", "DeviceInformationUpdate", result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void IdGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceInformationUpdateEventArgs^>(info.This())) {
        return;
      }

      MidiEndpointDeviceInformationUpdateEventArgs *wrapper = MidiEndpointDeviceInformationUpdateEventArgs::Unwrap<MidiEndpointDeviceInformationUpdateEventArgs>(info.This());

      try  {
        Platform::String^ result = wrapper->_instance->Id;
        info.GetReturnValue().Set(NodeRT::Utils::NewString(result->Data()));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void UpdatedAdditionalCapabilitiesGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceInformationUpdateEventArgs^>(info.This())) {
        return;
      }

      MidiEndpointDeviceInformationUpdateEventArgs *wrapper = MidiEndpointDeviceInformationUpdateEventArgs::Unwrap<MidiEndpointDeviceInformationUpdateEventArgs>(info.This());

      try  {
        bool result = wrapper->_instance->UpdatedAdditionalCapabilities;
        info.GetReturnValue().Set(Nan::New<Boolean>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void UpdatedDeviceIdentityGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceInformationUpdateEventArgs^>(info.This())) {
        return;
      }

      MidiEndpointDeviceInformationUpdateEventArgs *wrapper = MidiEndpointDeviceInformationUpdateEventArgs::Unwrap<MidiEndpointDeviceInformationUpdateEventArgs>(info.This());

      try  {
        bool result = wrapper->_instance->UpdatedDeviceIdentity;
        info.GetReturnValue().Set(Nan::New<Boolean>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void UpdatedEndpointInformationGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceInformationUpdateEventArgs^>(info.This())) {
        return;
      }

      MidiEndpointDeviceInformationUpdateEventArgs *wrapper = MidiEndpointDeviceInformationUpdateEventArgs::Unwrap<MidiEndpointDeviceInformationUpdateEventArgs>(info.This());

      try  {
        bool result = wrapper->_instance->UpdatedEndpointInformation;
        info.GetReturnValue().Set(Nan::New<Boolean>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void UpdatedFunctionBlocksGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceInformationUpdateEventArgs^>(info.This())) {
        return;
      }

      MidiEndpointDeviceInformationUpdateEventArgs *wrapper = MidiEndpointDeviceInformationUpdateEventArgs::Unwrap<MidiEndpointDeviceInformationUpdateEventArgs>(info.This());

      try  {
        bool result = wrapper->_instance->UpdatedFunctionBlocks;
        info.GetReturnValue().Set(Nan::New<Boolean>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void UpdatedNameGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceInformationUpdateEventArgs^>(info.This())) {
        return;
      }

      MidiEndpointDeviceInformationUpdateEventArgs *wrapper = MidiEndpointDeviceInformationUpdateEventArgs::Unwrap<MidiEndpointDeviceInformationUpdateEventArgs>(info.This());

      try  {
        bool result = wrapper->_instance->UpdatedName;
        info.GetReturnValue().Set(Nan::New<Boolean>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void UpdatedStreamConfigurationGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceInformationUpdateEventArgs^>(info.This())) {
        return;
      }

      MidiEndpointDeviceInformationUpdateEventArgs *wrapper = MidiEndpointDeviceInformationUpdateEventArgs::Unwrap<MidiEndpointDeviceInformationUpdateEventArgs>(info.This());

      try  {
        bool result = wrapper->_instance->UpdatedStreamConfiguration;
        info.GetReturnValue().Set(Nan::New<Boolean>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void UpdatedUserMetadataGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceInformationUpdateEventArgs^>(info.This())) {
        return;
      }

      MidiEndpointDeviceInformationUpdateEventArgs *wrapper = MidiEndpointDeviceInformationUpdateEventArgs::Unwrap<MidiEndpointDeviceInformationUpdateEventArgs>(info.This());

      try  {
        bool result = wrapper->_instance->UpdatedUserMetadata;
        info.GetReturnValue().Set(Nan::New<Boolean>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      


    private:
      ::Windows::Devices::Midi2::MidiEndpointDeviceInformationUpdateEventArgs^ _instance;
      static Persistent<FunctionTemplate> s_constructorTemplate;

      friend v8::Local<v8::Value> WrapMidiEndpointDeviceInformationUpdateEventArgs(::Windows::Devices::Midi2::MidiEndpointDeviceInformationUpdateEventArgs^ wintRtInstance);
      friend ::Windows::Devices::Midi2::MidiEndpointDeviceInformationUpdateEventArgs^ UnwrapMidiEndpointDeviceInformationUpdateEventArgs(Local<Value> value);
  };

  Persistent<FunctionTemplate> MidiEndpointDeviceInformationUpdateEventArgs::s_constructorTemplate;

  v8::Local<v8::Value> WrapMidiEndpointDeviceInformationUpdateEventArgs(::Windows::Devices::Midi2::MidiEndpointDeviceInformationUpdateEventArgs^ winRtInstance) {
    EscapableHandleScope scope;

    if (winRtInstance == nullptr) {
      return scope.Escape(Undefined());
    }

    Local<Value> opaqueWrapper = CreateOpaqueWrapper(winRtInstance);
    Local<Value> args[] = {opaqueWrapper};
    Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(MidiEndpointDeviceInformationUpdateEventArgs::s_constructorTemplate);
    return scope.Escape(Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(),_countof(args), args).ToLocalChecked());
  }

  ::Windows::Devices::Midi2::MidiEndpointDeviceInformationUpdateEventArgs^ UnwrapMidiEndpointDeviceInformationUpdateEventArgs(Local<Value> value) {
     return MidiEndpointDeviceInformationUpdateEventArgs::Unwrap<MidiEndpointDeviceInformationUpdateEventArgs>(Nan::To<Object>(value).ToLocalChecked())->_instance;
  }

  void InitMidiEndpointDeviceInformationUpdateEventArgs(Local<Object> exports) {
    MidiEndpointDeviceInformationUpdateEventArgs::Init(exports);
  }

  class MidiEndpointDeviceWatcher : public WrapperBase {
    public:
      
      static void Init(const Local<Object> exports) {
        HandleScope scope;

        Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(New);
        s_constructorTemplate.Reset(localRef);
        localRef->SetClassName(Nan::New<String>("MidiEndpointDeviceWatcher").ToLocalChecked());
        localRef->InstanceTemplate()->SetInternalFieldCount(1);


          
            Nan::SetPrototypeMethod(localRef, "start", Start);
            Nan::SetPrototypeMethod(localRef, "stop", Stop);
          


          
          Nan::SetPrototypeMethod(localRef,"addListener", AddListener);
          Nan::SetPrototypeMethod(localRef,"on", AddListener);
          Nan::SetPrototypeMethod(localRef,"removeListener", RemoveListener);
          Nan::SetPrototypeMethod(localRef, "off", RemoveListener);

          
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("enumeratedEndpointDevices").ToLocalChecked(), EnumeratedEndpointDevicesGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("status").ToLocalChecked(), StatusGetter);

        Local<Object> constructor = Nan::To<Object>(Nan::GetFunction(localRef).ToLocalChecked()).ToLocalChecked();
        Nan::SetMethod(constructor, "castFrom", CastFrom);

        Nan::SetMethod(constructor, "createWatcher", CreateWatcher);


        Nan::Set(exports, Nan::New<String>("MidiEndpointDeviceWatcher").ToLocalChecked(), constructor);
      }

      virtual ::Platform::Object^ GetObjectInstance() const override {
        return _instance;
      }

    private:

      MidiEndpointDeviceWatcher(::Windows::Devices::Midi2::MidiEndpointDeviceWatcher^ instance) {
        _instance = instance;
      }

      
    static void New(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(s_constructorTemplate);

      // in case the constructor was called without the new operator
      if (!localRef->HasInstance(info.This())) {
        if (info.Length() > 0) {
          std::unique_ptr<Local<Value> []> constructorArgs(new Local<Value>[info.Length()]);

          Local<Value> *argsPtr = constructorArgs.get();
          for (int i = 0; i < info.Length(); i++) {
            argsPtr[i] = info[i];
          }

          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), constructorArgs.get());
          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        } else {
          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), nullptr);

          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        }
      }

      ::Windows::Devices::Midi2::MidiEndpointDeviceWatcher^ winRtInstance;


      if (info.Length() == 1 && OpaqueWrapper::IsOpaqueWrapper(info[0]) &&
        NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceWatcher^>(info[0])) {
        try {
          winRtInstance = (::Windows::Devices::Midi2::MidiEndpointDeviceWatcher^) NodeRT::Utils::GetObjectInstance(info[0]);
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no suitable constructor found")));
        return;
      }

      NodeRT::Utils::SetHiddenValue(info.This(), Nan::New<String>("__winRtInstance__").ToLocalChecked(), True());

      MidiEndpointDeviceWatcher *wrapperInstance = new MidiEndpointDeviceWatcher(winRtInstance);
      wrapperInstance->Wrap(info.This());

      info.GetReturnValue().Set(info.This());
    }


      
    static void CastFrom(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;
      if (info.Length() < 1 || !NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceWatcher^>(info[0])) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no object provided, or given object could not be casted to requested type")));
        return;
      }

      ::Windows::Devices::Midi2::MidiEndpointDeviceWatcher^ winRtInstance;
      try {
        winRtInstance = (::Windows::Devices::Midi2::MidiEndpointDeviceWatcher^) NodeRT::Utils::GetObjectInstance(info[0]);
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }

      info.GetReturnValue().Set(WrapMidiEndpointDeviceWatcher(winRtInstance));
    }


    static void Start(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceWatcher^>(info.This())) {
        return;
      }

      MidiEndpointDeviceWatcher *wrapper = MidiEndpointDeviceWatcher::Unwrap<MidiEndpointDeviceWatcher>(info.This());

      if (info.Length() == 0)
      {
        try
        {
          wrapper->_instance->Start();
          return;
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }
    static void Stop(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceWatcher^>(info.This())) {
        return;
      }

      MidiEndpointDeviceWatcher *wrapper = MidiEndpointDeviceWatcher::Unwrap<MidiEndpointDeviceWatcher>(info.This());

      if (info.Length() == 0)
      {
        try
        {
          wrapper->_instance->Stop();
          return;
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }



    static void CreateWatcher(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 1
        && info[0]->IsInt32())
      {
        try
        {
          ::Windows::Devices::Midi2::MidiEndpointDeviceInformationFilter arg0 = static_cast<::Windows::Devices::Midi2::MidiEndpointDeviceInformationFilter>(Nan::To<int32_t>(info[0]).FromMaybe(0));
          
          ::Windows::Devices::Midi2::MidiEndpointDeviceWatcher^ result;
          result = ::Windows::Devices::Midi2::MidiEndpointDeviceWatcher::CreateWatcher(arg0);
          info.GetReturnValue().Set(WrapMidiEndpointDeviceWatcher(result));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void EnumeratedEndpointDevicesGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceWatcher^>(info.This())) {
        return;
      }

      MidiEndpointDeviceWatcher *wrapper = MidiEndpointDeviceWatcher::Unwrap<MidiEndpointDeviceWatcher>(info.This());

      try  {
        ::Windows::Foundation::Collections::IMapView<::Platform::String^, ::Windows::Devices::Midi2::MidiEndpointDeviceInformation^>^ result = wrapper->_instance->EnumeratedEndpointDevices;
        info.GetReturnValue().Set(NodeRT::Collections::MapViewWrapper<::Platform::String^,::Windows::Devices::Midi2::MidiEndpointDeviceInformation^>::CreateMapViewWrapper(result, 
            [](::Platform::String^ val) -> Local<Value> {
              return NodeRT::Utils::NewString(val->Data());
            },
            [](Local<Value> value) -> bool {
              return value->IsString();
            },
            [](Local<Value> value) -> ::Platform::String^ {
              return ref new Platform::String(NodeRT::Utils::StringToWchar(v8::String::Value(v8::Isolate::GetCurrent(), value)));
            },
            [](::Windows::Devices::Midi2::MidiEndpointDeviceInformation^ val) -> Local<Value> {
              return WrapMidiEndpointDeviceInformation(val);
            }
          ));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void StatusGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceWatcher^>(info.This())) {
        return;
      }

      MidiEndpointDeviceWatcher *wrapper = MidiEndpointDeviceWatcher::Unwrap<MidiEndpointDeviceWatcher>(info.This());

      try  {
        ::Windows::Devices::Enumeration::DeviceWatcherStatus result = wrapper->_instance->Status;
        info.GetReturnValue().Set(Nan::New<Integer>(static_cast<int>(result)));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      


    static void AddListener(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() < 2 || !info[0]->IsString() || !info[1]->IsFunction()) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"wrong arguments, expected arguments are eventName(string),callback(function)")));
        return;
      }

      String::Value eventName(v8::Isolate::GetCurrent(), info[0]);
      auto str = *eventName;

      Local<Function> callback = info[1].As<Function>();

      ::Windows::Foundation::EventRegistrationToken registrationToken;
      if (NodeRT::Utils::CaseInsenstiveEquals(L"added", str))
      {
        if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceWatcher^>(info.This()))
        {
          Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"The caller of this method isn't of the expected type or internal WinRt object was disposed")));
      return;
        }
        MidiEndpointDeviceWatcher *wrapper = MidiEndpointDeviceWatcher::Unwrap<MidiEndpointDeviceWatcher>(info.This());
      
        try {
          Persistent<Object>* perstPtr = new Persistent<Object>();
          perstPtr->Reset(NodeRT::Utils::CreateCallbackObjectInDomain(callback));
          std::shared_ptr<Persistent<Object>> callbackObjPtr(perstPtr,
            [] (Persistent<Object> *ptr ) {
              NodeUtils::Async::RunOnMain([ptr]() {
                ptr->Reset();
                delete ptr;
            });
          });

          registrationToken = wrapper->_instance->Added::add(
            ref new ::Windows::Foundation::TypedEventHandler<::Windows::Devices::Midi2::MidiEndpointDeviceWatcher^, ::Windows::Devices::Midi2::MidiEndpointDeviceInformation^>(
            [callbackObjPtr](::Windows::Devices::Midi2::MidiEndpointDeviceWatcher^ arg0, ::Windows::Devices::Midi2::MidiEndpointDeviceInformation^ arg1) {
              NodeUtils::Async::RunOnMain([callbackObjPtr , arg0, arg1]() {
                HandleScope scope;


                Local<Value> wrappedArg0;
                Local<Value> wrappedArg1;

                {
                  TryCatch tryCatch;


                  wrappedArg0 = WrapMidiEndpointDeviceWatcher(arg0);
                  wrappedArg1 = WrapMidiEndpointDeviceInformation(arg1);


                  if (wrappedArg0.IsEmpty()) wrappedArg0 = Undefined();
                  if (wrappedArg1.IsEmpty()) wrappedArg1 = Undefined();
                }

                Local<Value> args[] = { wrappedArg0, wrappedArg1 };
                Local<Object> callbackObjLocalRef = Nan::New<Object>(*callbackObjPtr);
                NodeRT::Utils::CallCallbackInDomain(callbackObjLocalRef, _countof(args), args);
              });
            })
          );
        }
        catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }

      }
      else if (NodeRT::Utils::CaseInsenstiveEquals(L"enumerationCompleted", str))
      {
        if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceWatcher^>(info.This()))
        {
          Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"The caller of this method isn't of the expected type or internal WinRt object was disposed")));
      return;
        }
        MidiEndpointDeviceWatcher *wrapper = MidiEndpointDeviceWatcher::Unwrap<MidiEndpointDeviceWatcher>(info.This());
      
        try {
          Persistent<Object>* perstPtr = new Persistent<Object>();
          perstPtr->Reset(NodeRT::Utils::CreateCallbackObjectInDomain(callback));
          std::shared_ptr<Persistent<Object>> callbackObjPtr(perstPtr,
            [] (Persistent<Object> *ptr ) {
              NodeUtils::Async::RunOnMain([ptr]() {
                ptr->Reset();
                delete ptr;
            });
          });

          registrationToken = wrapper->_instance->EnumerationCompleted::add(
            ref new ::Windows::Foundation::TypedEventHandler<::Windows::Devices::Midi2::MidiEndpointDeviceWatcher^, ::Platform::Object^>(
            [callbackObjPtr](::Windows::Devices::Midi2::MidiEndpointDeviceWatcher^ arg0, ::Platform::Object^ arg1) {
              NodeUtils::Async::RunOnMain([callbackObjPtr , arg0, arg1]() {
                HandleScope scope;


                Local<Value> wrappedArg0;
                Local<Value> wrappedArg1;

                {
                  TryCatch tryCatch;


                  wrappedArg0 = WrapMidiEndpointDeviceWatcher(arg0);
                  wrappedArg1 = CreateOpaqueWrapper(arg1);


                  if (wrappedArg0.IsEmpty()) wrappedArg0 = Undefined();
                  if (wrappedArg1.IsEmpty()) wrappedArg1 = Undefined();
                }

                Local<Value> args[] = { wrappedArg0, wrappedArg1 };
                Local<Object> callbackObjLocalRef = Nan::New<Object>(*callbackObjPtr);
                NodeRT::Utils::CallCallbackInDomain(callbackObjLocalRef, _countof(args), args);
              });
            })
          );
        }
        catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }

      }
      else if (NodeRT::Utils::CaseInsenstiveEquals(L"removed", str))
      {
        if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceWatcher^>(info.This()))
        {
          Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"The caller of this method isn't of the expected type or internal WinRt object was disposed")));
      return;
        }
        MidiEndpointDeviceWatcher *wrapper = MidiEndpointDeviceWatcher::Unwrap<MidiEndpointDeviceWatcher>(info.This());
      
        try {
          Persistent<Object>* perstPtr = new Persistent<Object>();
          perstPtr->Reset(NodeRT::Utils::CreateCallbackObjectInDomain(callback));
          std::shared_ptr<Persistent<Object>> callbackObjPtr(perstPtr,
            [] (Persistent<Object> *ptr ) {
              NodeUtils::Async::RunOnMain([ptr]() {
                ptr->Reset();
                delete ptr;
            });
          });

          registrationToken = wrapper->_instance->Removed::add(
            ref new ::Windows::Foundation::TypedEventHandler<::Windows::Devices::Midi2::MidiEndpointDeviceWatcher^, ::Windows::Devices::Enumeration::DeviceInformationUpdate^>(
            [callbackObjPtr](::Windows::Devices::Midi2::MidiEndpointDeviceWatcher^ arg0, ::Windows::Devices::Enumeration::DeviceInformationUpdate^ arg1) {
              NodeUtils::Async::RunOnMain([callbackObjPtr , arg0, arg1]() {
                HandleScope scope;


                Local<Value> wrappedArg0;
                Local<Value> wrappedArg1;

                {
                  TryCatch tryCatch;


                  wrappedArg0 = WrapMidiEndpointDeviceWatcher(arg0);
                  wrappedArg1 = NodeRT::Utils::CreateExternalWinRTObject("Windows.Devices.Enumeration", "DeviceInformationUpdate", arg1);


                  if (wrappedArg0.IsEmpty()) wrappedArg0 = Undefined();
                  if (wrappedArg1.IsEmpty()) wrappedArg1 = Undefined();
                }

                Local<Value> args[] = { wrappedArg0, wrappedArg1 };
                Local<Object> callbackObjLocalRef = Nan::New<Object>(*callbackObjPtr);
                NodeRT::Utils::CallCallbackInDomain(callbackObjLocalRef, _countof(args), args);
              });
            })
          );
        }
        catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }

      }
      else if (NodeRT::Utils::CaseInsenstiveEquals(L"stopped", str))
      {
        if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceWatcher^>(info.This()))
        {
          Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"The caller of this method isn't of the expected type or internal WinRt object was disposed")));
      return;
        }
        MidiEndpointDeviceWatcher *wrapper = MidiEndpointDeviceWatcher::Unwrap<MidiEndpointDeviceWatcher>(info.This());
      
        try {
          Persistent<Object>* perstPtr = new Persistent<Object>();
          perstPtr->Reset(NodeRT::Utils::CreateCallbackObjectInDomain(callback));
          std::shared_ptr<Persistent<Object>> callbackObjPtr(perstPtr,
            [] (Persistent<Object> *ptr ) {
              NodeUtils::Async::RunOnMain([ptr]() {
                ptr->Reset();
                delete ptr;
            });
          });

          registrationToken = wrapper->_instance->Stopped::add(
            ref new ::Windows::Foundation::TypedEventHandler<::Windows::Devices::Midi2::MidiEndpointDeviceWatcher^, ::Platform::Object^>(
            [callbackObjPtr](::Windows::Devices::Midi2::MidiEndpointDeviceWatcher^ arg0, ::Platform::Object^ arg1) {
              NodeUtils::Async::RunOnMain([callbackObjPtr , arg0, arg1]() {
                HandleScope scope;


                Local<Value> wrappedArg0;
                Local<Value> wrappedArg1;

                {
                  TryCatch tryCatch;


                  wrappedArg0 = WrapMidiEndpointDeviceWatcher(arg0);
                  wrappedArg1 = CreateOpaqueWrapper(arg1);


                  if (wrappedArg0.IsEmpty()) wrappedArg0 = Undefined();
                  if (wrappedArg1.IsEmpty()) wrappedArg1 = Undefined();
                }

                Local<Value> args[] = { wrappedArg0, wrappedArg1 };
                Local<Object> callbackObjLocalRef = Nan::New<Object>(*callbackObjPtr);
                NodeRT::Utils::CallCallbackInDomain(callbackObjLocalRef, _countof(args), args);
              });
            })
          );
        }
        catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }

      }
      else if (NodeRT::Utils::CaseInsenstiveEquals(L"updated", str))
      {
        if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceWatcher^>(info.This()))
        {
          Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"The caller of this method isn't of the expected type or internal WinRt object was disposed")));
      return;
        }
        MidiEndpointDeviceWatcher *wrapper = MidiEndpointDeviceWatcher::Unwrap<MidiEndpointDeviceWatcher>(info.This());
      
        try {
          Persistent<Object>* perstPtr = new Persistent<Object>();
          perstPtr->Reset(NodeRT::Utils::CreateCallbackObjectInDomain(callback));
          std::shared_ptr<Persistent<Object>> callbackObjPtr(perstPtr,
            [] (Persistent<Object> *ptr ) {
              NodeUtils::Async::RunOnMain([ptr]() {
                ptr->Reset();
                delete ptr;
            });
          });

          registrationToken = wrapper->_instance->Updated::add(
            ref new ::Windows::Foundation::TypedEventHandler<::Windows::Devices::Midi2::MidiEndpointDeviceWatcher^, ::Windows::Devices::Midi2::MidiEndpointDeviceInformationUpdateEventArgs^>(
            [callbackObjPtr](::Windows::Devices::Midi2::MidiEndpointDeviceWatcher^ arg0, ::Windows::Devices::Midi2::MidiEndpointDeviceInformationUpdateEventArgs^ arg1) {
              NodeUtils::Async::RunOnMain([callbackObjPtr , arg0, arg1]() {
                HandleScope scope;


                Local<Value> wrappedArg0;
                Local<Value> wrappedArg1;

                {
                  TryCatch tryCatch;


                  wrappedArg0 = WrapMidiEndpointDeviceWatcher(arg0);
                  wrappedArg1 = WrapMidiEndpointDeviceInformationUpdateEventArgs(arg1);


                  if (wrappedArg0.IsEmpty()) wrappedArg0 = Undefined();
                  if (wrappedArg1.IsEmpty()) wrappedArg1 = Undefined();
                }

                Local<Value> args[] = { wrappedArg0, wrappedArg1 };
                Local<Object> callbackObjLocalRef = Nan::New<Object>(*callbackObjPtr);
                NodeRT::Utils::CallCallbackInDomain(callbackObjLocalRef, _countof(args), args);
              });
            })
          );
        }
        catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }

      }
 else  {
        Nan::ThrowError(Nan::Error(String::Concat(v8::Isolate::GetCurrent(), NodeRT::Utils::NewString(L"given event name isn't supported: "), info[0].As<String>())));
        return;
      }

      Local<Value> tokenMapVal = NodeRT::Utils::GetHiddenValue(callback, Nan::New<String>(REGISTRATION_TOKEN_MAP_PROPERTY_NAME).ToLocalChecked());
      Local<Object> tokenMap;

      if (tokenMapVal.IsEmpty() || Nan::Equals(tokenMapVal, Undefined()).FromMaybe(false)) {
        tokenMap = Nan::New<Object>();
        NodeRT::Utils::SetHiddenValueWithObject(callback, Nan::New<String>(REGISTRATION_TOKEN_MAP_PROPERTY_NAME).ToLocalChecked(), tokenMap);
      } else {
        tokenMap = Nan::To<Object>(tokenMapVal).ToLocalChecked();
      }

      Nan::Set(tokenMap, info[0], CreateOpaqueWrapper(::Windows::Foundation::PropertyValue::CreateInt64(registrationToken.Value)));
    }

    static void RemoveListener(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() < 2 || !info[0]->IsString() || !info[1]->IsFunction()) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"wrong arguments, expected a string and a callback")));
        return;
      }

      String::Value eventName(v8::Isolate::GetCurrent(), info[0]);
      auto str = *eventName;

      if ((!NodeRT::Utils::CaseInsenstiveEquals(L"added", str)) &&(!NodeRT::Utils::CaseInsenstiveEquals(L"enumerationCompleted", str)) &&(!NodeRT::Utils::CaseInsenstiveEquals(L"removed", str)) &&(!NodeRT::Utils::CaseInsenstiveEquals(L"stopped", str)) &&(!NodeRT::Utils::CaseInsenstiveEquals(L"updated", str))) {
        Nan::ThrowError(Nan::Error(String::Concat(v8::Isolate::GetCurrent(), NodeRT::Utils::NewString(L"given event name isn't supported: "), info[0].As<String>())));
        return;
      }

      Local<Function> callback = info[1].As<Function>();
      Local<Value> tokenMap = NodeRT::Utils::GetHiddenValue(callback, Nan::New<String>(REGISTRATION_TOKEN_MAP_PROPERTY_NAME).ToLocalChecked());

      if (tokenMap.IsEmpty() || Nan::Equals(tokenMap, Undefined()).FromMaybe(false)) {
        return;
      }

      Local<Value> opaqueWrapperObj =  Nan::Get(Nan::To<Object>(tokenMap).ToLocalChecked(), info[0]).ToLocalChecked();

      if (opaqueWrapperObj.IsEmpty() || Nan::Equals(opaqueWrapperObj,Undefined()).FromMaybe(false)) {
        return;
      }

      OpaqueWrapper *opaqueWrapper = OpaqueWrapper::Unwrap<OpaqueWrapper>(opaqueWrapperObj.As<Object>());

      long long tokenValue = (long long) opaqueWrapper->GetObjectInstance();
      ::Windows::Foundation::EventRegistrationToken registrationToken;
      registrationToken.Value = tokenValue;

      try  {
        if (NodeRT::Utils::CaseInsenstiveEquals(L"added", str)) {
          if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceWatcher^>(info.This()))
          {
            Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"The caller of this method isn't of the expected type or internal WinRt object was disposed")));
            return;
          }
          MidiEndpointDeviceWatcher *wrapper = MidiEndpointDeviceWatcher::Unwrap<MidiEndpointDeviceWatcher>(info.This());
          wrapper->_instance->Added::remove(registrationToken);
        }
        else if (NodeRT::Utils::CaseInsenstiveEquals(L"enumerationCompleted", str))
        {
          if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceWatcher^>(info.This()))
          {
            Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"The caller of this method isn't of the expected type or internal WinRt object was disposed")));
            return;
          }
          MidiEndpointDeviceWatcher *wrapper = MidiEndpointDeviceWatcher::Unwrap<MidiEndpointDeviceWatcher>(info.This());
          wrapper->_instance->EnumerationCompleted::remove(registrationToken);
        }
        else if (NodeRT::Utils::CaseInsenstiveEquals(L"removed", str))
        {
          if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceWatcher^>(info.This()))
          {
            Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"The caller of this method isn't of the expected type or internal WinRt object was disposed")));
            return;
          }
          MidiEndpointDeviceWatcher *wrapper = MidiEndpointDeviceWatcher::Unwrap<MidiEndpointDeviceWatcher>(info.This());
          wrapper->_instance->Removed::remove(registrationToken);
        }
        else if (NodeRT::Utils::CaseInsenstiveEquals(L"stopped", str))
        {
          if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceWatcher^>(info.This()))
          {
            Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"The caller of this method isn't of the expected type or internal WinRt object was disposed")));
            return;
          }
          MidiEndpointDeviceWatcher *wrapper = MidiEndpointDeviceWatcher::Unwrap<MidiEndpointDeviceWatcher>(info.This());
          wrapper->_instance->Stopped::remove(registrationToken);
        }
        else if (NodeRT::Utils::CaseInsenstiveEquals(L"updated", str))
        {
          if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointDeviceWatcher^>(info.This()))
          {
            Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"The caller of this method isn't of the expected type or internal WinRt object was disposed")));
            return;
          }
          MidiEndpointDeviceWatcher *wrapper = MidiEndpointDeviceWatcher::Unwrap<MidiEndpointDeviceWatcher>(info.This());
          wrapper->_instance->Updated::remove(registrationToken);
        }
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
      }

      Nan::Delete(Nan::To<Object>(tokenMap).ToLocalChecked(), Nan::To<String>(info[0]).ToLocalChecked());
    }
    private:
      ::Windows::Devices::Midi2::MidiEndpointDeviceWatcher^ _instance;
      static Persistent<FunctionTemplate> s_constructorTemplate;

      friend v8::Local<v8::Value> WrapMidiEndpointDeviceWatcher(::Windows::Devices::Midi2::MidiEndpointDeviceWatcher^ wintRtInstance);
      friend ::Windows::Devices::Midi2::MidiEndpointDeviceWatcher^ UnwrapMidiEndpointDeviceWatcher(Local<Value> value);
  };

  Persistent<FunctionTemplate> MidiEndpointDeviceWatcher::s_constructorTemplate;

  v8::Local<v8::Value> WrapMidiEndpointDeviceWatcher(::Windows::Devices::Midi2::MidiEndpointDeviceWatcher^ winRtInstance) {
    EscapableHandleScope scope;

    if (winRtInstance == nullptr) {
      return scope.Escape(Undefined());
    }

    Local<Value> opaqueWrapper = CreateOpaqueWrapper(winRtInstance);
    Local<Value> args[] = {opaqueWrapper};
    Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(MidiEndpointDeviceWatcher::s_constructorTemplate);
    return scope.Escape(Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(),_countof(args), args).ToLocalChecked());
  }

  ::Windows::Devices::Midi2::MidiEndpointDeviceWatcher^ UnwrapMidiEndpointDeviceWatcher(Local<Value> value) {
     return MidiEndpointDeviceWatcher::Unwrap<MidiEndpointDeviceWatcher>(Nan::To<Object>(value).ToLocalChecked())->_instance;
  }

  void InitMidiEndpointDeviceWatcher(Local<Object> exports) {
    MidiEndpointDeviceWatcher::Init(exports);
  }

  class MidiFunctionBlock : public WrapperBase {
    public:
      
      static void Init(const Local<Object> exports) {
        HandleScope scope;

        Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(New);
        s_constructorTemplate.Reset(localRef);
        localRef->SetClassName(Nan::New<String>("MidiFunctionBlock").ToLocalChecked());
        localRef->InstanceTemplate()->SetInternalFieldCount(1);


          
            Nan::SetPrototypeMethod(localRef, "includesGroup", IncludesGroup);
            Nan::SetPrototypeMethod(localRef, "updateFromJson", UpdateFromJson);
          



          
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("direction").ToLocalChecked(), DirectionGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("firstGroupIndex").ToLocalChecked(), FirstGroupIndexGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("groupCount").ToLocalChecked(), GroupCountGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("isActive").ToLocalChecked(), IsActiveGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("maxSystemExclusive8Streams").ToLocalChecked(), MaxSystemExclusive8StreamsGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("midi10Connection").ToLocalChecked(), Midi10ConnectionGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("midiCIMessageVersionFormat").ToLocalChecked(), MidiCIMessageVersionFormatGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("name").ToLocalChecked(), NameGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("number").ToLocalChecked(), NumberGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("uIHint").ToLocalChecked(), UIHintGetter);

        Local<Object> constructor = Nan::To<Object>(Nan::GetFunction(localRef).ToLocalChecked()).ToLocalChecked();
        Nan::SetMethod(constructor, "castFrom", CastFrom);



        Nan::Set(exports, Nan::New<String>("MidiFunctionBlock").ToLocalChecked(), constructor);
      }

      virtual ::Platform::Object^ GetObjectInstance() const override {
        return _instance;
      }

    private:

      MidiFunctionBlock(::Windows::Devices::Midi2::MidiFunctionBlock^ instance) {
        _instance = instance;
      }

      
    static void New(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(s_constructorTemplate);

      // in case the constructor was called without the new operator
      if (!localRef->HasInstance(info.This())) {
        if (info.Length() > 0) {
          std::unique_ptr<Local<Value> []> constructorArgs(new Local<Value>[info.Length()]);

          Local<Value> *argsPtr = constructorArgs.get();
          for (int i = 0; i < info.Length(); i++) {
            argsPtr[i] = info[i];
          }

          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), constructorArgs.get());
          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        } else {
          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), nullptr);

          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        }
      }

      ::Windows::Devices::Midi2::MidiFunctionBlock^ winRtInstance;


      if (info.Length() == 1 && OpaqueWrapper::IsOpaqueWrapper(info[0]) &&
        NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiFunctionBlock^>(info[0])) {
        try {
          winRtInstance = (::Windows::Devices::Midi2::MidiFunctionBlock^) NodeRT::Utils::GetObjectInstance(info[0]);
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
      else if (info.Length() == 0)
      {
        try {
          winRtInstance = ref new ::Windows::Devices::Midi2::MidiFunctionBlock();
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no suitable constructor found")));
        return;
      }

      NodeRT::Utils::SetHiddenValue(info.This(), Nan::New<String>("__winRtInstance__").ToLocalChecked(), True());

      MidiFunctionBlock *wrapperInstance = new MidiFunctionBlock(winRtInstance);
      wrapperInstance->Wrap(info.This());

      info.GetReturnValue().Set(info.This());
    }


      
    static void CastFrom(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;
      if (info.Length() < 1 || !NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiFunctionBlock^>(info[0])) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no object provided, or given object could not be casted to requested type")));
        return;
      }

      ::Windows::Devices::Midi2::MidiFunctionBlock^ winRtInstance;
      try {
        winRtInstance = (::Windows::Devices::Midi2::MidiFunctionBlock^) NodeRT::Utils::GetObjectInstance(info[0]);
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }

      info.GetReturnValue().Set(WrapMidiFunctionBlock(winRtInstance));
    }


    static void IncludesGroup(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiFunctionBlock^>(info.This())) {
        return;
      }

      MidiFunctionBlock *wrapper = MidiFunctionBlock::Unwrap<MidiFunctionBlock>(info.This());

      if (info.Length() == 1
        && NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiGroup^>(info[0]))
      {
        try
        {
          ::Windows::Devices::Midi2::MidiGroup^ arg0 = UnwrapMidiGroup(info[0]);
          
          bool result;
          result = wrapper->_instance->IncludesGroup(arg0);
          info.GetReturnValue().Set(Nan::New<Boolean>(result));
          return;
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }
    static void UpdateFromJson(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiFunctionBlock^>(info.This())) {
        return;
      }

      MidiFunctionBlock *wrapper = MidiFunctionBlock::Unwrap<MidiFunctionBlock>(info.This());

      if (info.Length() == 1
        && NodeRT::Utils::IsWinRtWrapperOf<::Windows::Data::Json::JsonObject^>(info[0]))
      {
        try
        {
          ::Windows::Data::Json::JsonObject^ arg0 = dynamic_cast<::Windows::Data::Json::JsonObject^>(NodeRT::Utils::GetObjectInstance(info[0]));
          
          bool result;
          result = wrapper->_instance->UpdateFromJson(arg0);
          info.GetReturnValue().Set(Nan::New<Boolean>(result));
          return;
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }



    static void DirectionGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiFunctionBlock^>(info.This())) {
        return;
      }

      MidiFunctionBlock *wrapper = MidiFunctionBlock::Unwrap<MidiFunctionBlock>(info.This());

      try  {
        ::Windows::Devices::Midi2::MidiFunctionBlockDirection result = wrapper->_instance->Direction;
        info.GetReturnValue().Set(Nan::New<Integer>(static_cast<int>(result)));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void FirstGroupIndexGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiFunctionBlock^>(info.This())) {
        return;
      }

      MidiFunctionBlock *wrapper = MidiFunctionBlock::Unwrap<MidiFunctionBlock>(info.This());

      try  {
        unsigned char result = wrapper->_instance->FirstGroupIndex;
        info.GetReturnValue().Set(Nan::New<Integer>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void GroupCountGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiFunctionBlock^>(info.This())) {
        return;
      }

      MidiFunctionBlock *wrapper = MidiFunctionBlock::Unwrap<MidiFunctionBlock>(info.This());

      try  {
        unsigned char result = wrapper->_instance->GroupCount;
        info.GetReturnValue().Set(Nan::New<Integer>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void IsActiveGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiFunctionBlock^>(info.This())) {
        return;
      }

      MidiFunctionBlock *wrapper = MidiFunctionBlock::Unwrap<MidiFunctionBlock>(info.This());

      try  {
        bool result = wrapper->_instance->IsActive;
        info.GetReturnValue().Set(Nan::New<Boolean>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void MaxSystemExclusive8StreamsGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiFunctionBlock^>(info.This())) {
        return;
      }

      MidiFunctionBlock *wrapper = MidiFunctionBlock::Unwrap<MidiFunctionBlock>(info.This());

      try  {
        unsigned char result = wrapper->_instance->MaxSystemExclusive8Streams;
        info.GetReturnValue().Set(Nan::New<Integer>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void Midi10ConnectionGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiFunctionBlock^>(info.This())) {
        return;
      }

      MidiFunctionBlock *wrapper = MidiFunctionBlock::Unwrap<MidiFunctionBlock>(info.This());

      try  {
        ::Windows::Devices::Midi2::MidiFunctionBlockMidi10 result = wrapper->_instance->Midi10Connection;
        info.GetReturnValue().Set(Nan::New<Integer>(static_cast<int>(result)));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void MidiCIMessageVersionFormatGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiFunctionBlock^>(info.This())) {
        return;
      }

      MidiFunctionBlock *wrapper = MidiFunctionBlock::Unwrap<MidiFunctionBlock>(info.This());

      try  {
        unsigned char result = wrapper->_instance->MidiCIMessageVersionFormat;
        info.GetReturnValue().Set(Nan::New<Integer>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void NameGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiFunctionBlock^>(info.This())) {
        return;
      }

      MidiFunctionBlock *wrapper = MidiFunctionBlock::Unwrap<MidiFunctionBlock>(info.This());

      try  {
        Platform::String^ result = wrapper->_instance->Name;
        info.GetReturnValue().Set(NodeRT::Utils::NewString(result->Data()));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void NumberGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiFunctionBlock^>(info.This())) {
        return;
      }

      MidiFunctionBlock *wrapper = MidiFunctionBlock::Unwrap<MidiFunctionBlock>(info.This());

      try  {
        unsigned char result = wrapper->_instance->Number;
        info.GetReturnValue().Set(Nan::New<Integer>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void UIHintGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiFunctionBlock^>(info.This())) {
        return;
      }

      MidiFunctionBlock *wrapper = MidiFunctionBlock::Unwrap<MidiFunctionBlock>(info.This());

      try  {
        ::Windows::Devices::Midi2::MidiFunctionBlockUIHint result = wrapper->_instance->UIHint;
        info.GetReturnValue().Set(Nan::New<Integer>(static_cast<int>(result)));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      


    private:
      ::Windows::Devices::Midi2::MidiFunctionBlock^ _instance;
      static Persistent<FunctionTemplate> s_constructorTemplate;

      friend v8::Local<v8::Value> WrapMidiFunctionBlock(::Windows::Devices::Midi2::MidiFunctionBlock^ wintRtInstance);
      friend ::Windows::Devices::Midi2::MidiFunctionBlock^ UnwrapMidiFunctionBlock(Local<Value> value);
  };

  Persistent<FunctionTemplate> MidiFunctionBlock::s_constructorTemplate;

  v8::Local<v8::Value> WrapMidiFunctionBlock(::Windows::Devices::Midi2::MidiFunctionBlock^ winRtInstance) {
    EscapableHandleScope scope;

    if (winRtInstance == nullptr) {
      return scope.Escape(Undefined());
    }

    Local<Value> opaqueWrapper = CreateOpaqueWrapper(winRtInstance);
    Local<Value> args[] = {opaqueWrapper};
    Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(MidiFunctionBlock::s_constructorTemplate);
    return scope.Escape(Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(),_countof(args), args).ToLocalChecked());
  }

  ::Windows::Devices::Midi2::MidiFunctionBlock^ UnwrapMidiFunctionBlock(Local<Value> value) {
     return MidiFunctionBlock::Unwrap<MidiFunctionBlock>(Nan::To<Object>(value).ToLocalChecked())->_instance;
  }

  void InitMidiFunctionBlock(Local<Object> exports) {
    MidiFunctionBlock::Init(exports);
  }

  class MidiGroup : public WrapperBase {
    public:
      
      static void Init(const Local<Object> exports) {
        HandleScope scope;

        Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(New);
        s_constructorTemplate.Reset(localRef);
        localRef->SetClassName(Nan::New<String>("MidiGroup").ToLocalChecked());
        localRef->InstanceTemplate()->SetInternalFieldCount(1);





          
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("index").ToLocalChecked(), IndexGetter, IndexSetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("numberForDisplay").ToLocalChecked(), NumberForDisplayGetter);

        Local<Object> constructor = Nan::To<Object>(Nan::GetFunction(localRef).ToLocalChecked()).ToLocalChecked();
        Nan::SetMethod(constructor, "castFrom", CastFrom);

        Nan::SetMethod(constructor, "isValidGroupIndex", IsValidGroupIndex);
        Nan::SetAccessor(constructor, Nan::New<String>("labelFull").ToLocalChecked(), LabelFullGetter);
        Nan::SetAccessor(constructor, Nan::New<String>("labelShort").ToLocalChecked(), LabelShortGetter);


        Nan::Set(exports, Nan::New<String>("MidiGroup").ToLocalChecked(), constructor);
      }

      virtual ::Platform::Object^ GetObjectInstance() const override {
        return _instance;
      }

    private:

      MidiGroup(::Windows::Devices::Midi2::MidiGroup^ instance) {
        _instance = instance;
      }

      
    static void New(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(s_constructorTemplate);

      // in case the constructor was called without the new operator
      if (!localRef->HasInstance(info.This())) {
        if (info.Length() > 0) {
          std::unique_ptr<Local<Value> []> constructorArgs(new Local<Value>[info.Length()]);

          Local<Value> *argsPtr = constructorArgs.get();
          for (int i = 0; i < info.Length(); i++) {
            argsPtr[i] = info[i];
          }

          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), constructorArgs.get());
          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        } else {
          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), nullptr);

          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        }
      }

      ::Windows::Devices::Midi2::MidiGroup^ winRtInstance;


      if (info.Length() == 1 && OpaqueWrapper::IsOpaqueWrapper(info[0]) &&
        NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiGroup^>(info[0])) {
        try {
          winRtInstance = (::Windows::Devices::Midi2::MidiGroup^) NodeRT::Utils::GetObjectInstance(info[0]);
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
      else if (info.Length() == 1
        && info[0]->IsInt32())
      {
        try {
          unsigned char arg0 = static_cast<unsigned char>(Nan::To<int32_t>(info[0]).FromMaybe(0));
          
          winRtInstance = ref new ::Windows::Devices::Midi2::MidiGroup(arg0);
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
      else if (info.Length() == 0)
      {
        try {
          winRtInstance = ref new ::Windows::Devices::Midi2::MidiGroup();
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no suitable constructor found")));
        return;
      }

      NodeRT::Utils::SetHiddenValue(info.This(), Nan::New<String>("__winRtInstance__").ToLocalChecked(), True());

      MidiGroup *wrapperInstance = new MidiGroup(winRtInstance);
      wrapperInstance->Wrap(info.This());

      info.GetReturnValue().Set(info.This());
    }


      
    static void CastFrom(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;
      if (info.Length() < 1 || !NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiGroup^>(info[0])) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no object provided, or given object could not be casted to requested type")));
        return;
      }

      ::Windows::Devices::Midi2::MidiGroup^ winRtInstance;
      try {
        winRtInstance = (::Windows::Devices::Midi2::MidiGroup^) NodeRT::Utils::GetObjectInstance(info[0]);
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }

      info.GetReturnValue().Set(WrapMidiGroup(winRtInstance));
    }





    static void IsValidGroupIndex(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 1
        && info[0]->IsInt32())
      {
        try
        {
          unsigned char arg0 = static_cast<unsigned char>(Nan::To<int32_t>(info[0]).FromMaybe(0));
          
          bool result;
          result = ::Windows::Devices::Midi2::MidiGroup::IsValidGroupIndex(arg0);
          info.GetReturnValue().Set(Nan::New<Boolean>(result));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void IndexGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiGroup^>(info.This())) {
        return;
      }

      MidiGroup *wrapper = MidiGroup::Unwrap<MidiGroup>(info.This());

      try  {
        unsigned char result = wrapper->_instance->Index;
        info.GetReturnValue().Set(Nan::New<Integer>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void IndexSetter(Local<String> property, Local<Value> value, const Nan::PropertyCallbackInfo<void> &info) {
      HandleScope scope;

      if (!value->IsInt32()) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Value to set is of unexpected type")));
        return;
      }

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiGroup^>(info.This())) {
        return;
      }

      MidiGroup *wrapper = MidiGroup::Unwrap<MidiGroup>(info.This());

      try {

        unsigned char winRtValue = static_cast<unsigned char>(Nan::To<int32_t>(value).FromMaybe(0));

        wrapper->_instance->Index = winRtValue;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
      }
    }
      
    static void NumberForDisplayGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiGroup^>(info.This())) {
        return;
      }

      MidiGroup *wrapper = MidiGroup::Unwrap<MidiGroup>(info.This());

      try  {
        unsigned char result = wrapper->_instance->NumberForDisplay;
        info.GetReturnValue().Set(Nan::New<Integer>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      


    static void LabelFullGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      try
      {
        Platform::String^ result = ::Windows::Devices::Midi2::MidiGroup::LabelFull;
        info.GetReturnValue().Set(NodeRT::Utils::NewString(result->Data()));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      

    static void LabelShortGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      try
      {
        Platform::String^ result = ::Windows::Devices::Midi2::MidiGroup::LabelShort;
        info.GetReturnValue().Set(NodeRT::Utils::NewString(result->Data()));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      

    private:
      ::Windows::Devices::Midi2::MidiGroup^ _instance;
      static Persistent<FunctionTemplate> s_constructorTemplate;

      friend v8::Local<v8::Value> WrapMidiGroup(::Windows::Devices::Midi2::MidiGroup^ wintRtInstance);
      friend ::Windows::Devices::Midi2::MidiGroup^ UnwrapMidiGroup(Local<Value> value);
  };

  Persistent<FunctionTemplate> MidiGroup::s_constructorTemplate;

  v8::Local<v8::Value> WrapMidiGroup(::Windows::Devices::Midi2::MidiGroup^ winRtInstance) {
    EscapableHandleScope scope;

    if (winRtInstance == nullptr) {
      return scope.Escape(Undefined());
    }

    Local<Value> opaqueWrapper = CreateOpaqueWrapper(winRtInstance);
    Local<Value> args[] = {opaqueWrapper};
    Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(MidiGroup::s_constructorTemplate);
    return scope.Escape(Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(),_countof(args), args).ToLocalChecked());
  }

  ::Windows::Devices::Midi2::MidiGroup^ UnwrapMidiGroup(Local<Value> value) {
     return MidiGroup::Unwrap<MidiGroup>(Nan::To<Object>(value).ToLocalChecked())->_instance;
  }

  void InitMidiGroup(Local<Object> exports) {
    MidiGroup::Init(exports);
  }

  class MidiGroupEndpointListener : public WrapperBase {
    public:
      
      static void Init(const Local<Object> exports) {
        HandleScope scope;

        Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(New);
        s_constructorTemplate.Reset(localRef);
        localRef->SetClassName(Nan::New<String>("MidiGroupEndpointListener").ToLocalChecked());
        localRef->InstanceTemplate()->SetInternalFieldCount(1);


          
            Nan::SetPrototypeMethod(localRef, "initialize", Initialize);
            Nan::SetPrototypeMethod(localRef, "onEndpointConnectionOpened", OnEndpointConnectionOpened);
            Nan::SetPrototypeMethod(localRef, "processIncomingMessage", ProcessIncomingMessage);
            Nan::SetPrototypeMethod(localRef, "cleanup", Cleanup);
          


          
          Nan::SetPrototypeMethod(localRef,"addListener", AddListener);
          Nan::SetPrototypeMethod(localRef,"on", AddListener);
          Nan::SetPrototypeMethod(localRef,"removeListener", RemoveListener);
          Nan::SetPrototypeMethod(localRef, "off", RemoveListener);

          
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("tag").ToLocalChecked(), TagGetter, TagSetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("name").ToLocalChecked(), NameGetter, NameSetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("isEnabled").ToLocalChecked(), IsEnabledGetter, IsEnabledSetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("id").ToLocalChecked(), IdGetter, IdSetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("includeGroups").ToLocalChecked(), IncludeGroupsGetter);

        Local<Object> constructor = Nan::To<Object>(Nan::GetFunction(localRef).ToLocalChecked()).ToLocalChecked();
        Nan::SetMethod(constructor, "castFrom", CastFrom);



        Nan::Set(exports, Nan::New<String>("MidiGroupEndpointListener").ToLocalChecked(), constructor);
      }

      virtual ::Platform::Object^ GetObjectInstance() const override {
        return _instance;
      }

    private:

      MidiGroupEndpointListener(::Windows::Devices::Midi2::MidiGroupEndpointListener^ instance) {
        _instance = instance;
      }

      
    static void New(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(s_constructorTemplate);

      // in case the constructor was called without the new operator
      if (!localRef->HasInstance(info.This())) {
        if (info.Length() > 0) {
          std::unique_ptr<Local<Value> []> constructorArgs(new Local<Value>[info.Length()]);

          Local<Value> *argsPtr = constructorArgs.get();
          for (int i = 0; i < info.Length(); i++) {
            argsPtr[i] = info[i];
          }

          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), constructorArgs.get());
          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        } else {
          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), nullptr);

          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        }
      }

      ::Windows::Devices::Midi2::MidiGroupEndpointListener^ winRtInstance;


      if (info.Length() == 1 && OpaqueWrapper::IsOpaqueWrapper(info[0]) &&
        NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiGroupEndpointListener^>(info[0])) {
        try {
          winRtInstance = (::Windows::Devices::Midi2::MidiGroupEndpointListener^) NodeRT::Utils::GetObjectInstance(info[0]);
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
      else if (info.Length() == 0)
      {
        try {
          winRtInstance = ref new ::Windows::Devices::Midi2::MidiGroupEndpointListener();
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no suitable constructor found")));
        return;
      }

      NodeRT::Utils::SetHiddenValue(info.This(), Nan::New<String>("__winRtInstance__").ToLocalChecked(), True());

      MidiGroupEndpointListener *wrapperInstance = new MidiGroupEndpointListener(winRtInstance);
      wrapperInstance->Wrap(info.This());

      info.GetReturnValue().Set(info.This());
    }


      
    static void CastFrom(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;
      if (info.Length() < 1 || !NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiGroupEndpointListener^>(info[0])) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no object provided, or given object could not be casted to requested type")));
        return;
      }

      ::Windows::Devices::Midi2::MidiGroupEndpointListener^ winRtInstance;
      try {
        winRtInstance = (::Windows::Devices::Midi2::MidiGroupEndpointListener^) NodeRT::Utils::GetObjectInstance(info[0]);
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }

      info.GetReturnValue().Set(WrapMidiGroupEndpointListener(winRtInstance));
    }


    static void Initialize(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiGroupEndpointListener^>(info.This())) {
        return;
      }

      MidiGroupEndpointListener *wrapper = MidiGroupEndpointListener::Unwrap<MidiGroupEndpointListener>(info.This());

      if (info.Length() == 1
        && NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::IMidiEndpointConnectionSource^>(info[0]))
      {
        try
        {
          ::Windows::Devices::Midi2::IMidiEndpointConnectionSource^ arg0 = UnwrapIMidiEndpointConnectionSource(info[0]);
          
          wrapper->_instance->Initialize(arg0);
          return;
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }
    static void OnEndpointConnectionOpened(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiGroupEndpointListener^>(info.This())) {
        return;
      }

      MidiGroupEndpointListener *wrapper = MidiGroupEndpointListener::Unwrap<MidiGroupEndpointListener>(info.This());

      if (info.Length() == 0)
      {
        try
        {
          wrapper->_instance->OnEndpointConnectionOpened();
          return;
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }
    static void ProcessIncomingMessage(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiGroupEndpointListener^>(info.This())) {
        return;
      }

      MidiGroupEndpointListener *wrapper = MidiGroupEndpointListener::Unwrap<MidiGroupEndpointListener>(info.This());

      if (info.Length() == 1
        && NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessageReceivedEventArgs^>(info[0]))
      {
        try
        {
          ::Windows::Devices::Midi2::MidiMessageReceivedEventArgs^ arg0 = UnwrapMidiMessageReceivedEventArgs(info[0]);
          bool arg1;
          bool arg2;
          
          wrapper->_instance->ProcessIncomingMessage(arg0, &arg1, &arg2);
          Local<Object> resObj = Nan::New<Object>();
          Nan::Set(resObj, Nan::New<String>("skipFurtherListeners").ToLocalChecked(), Nan::New<Boolean>(arg1));
          Nan::Set(resObj, Nan::New<String>("skipMainMessageReceivedEvent").ToLocalChecked(), Nan::New<Boolean>(arg2));
          info.GetReturnValue().Set(resObj);
          return;
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }
    static void Cleanup(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiGroupEndpointListener^>(info.This())) {
        return;
      }

      MidiGroupEndpointListener *wrapper = MidiGroupEndpointListener::Unwrap<MidiGroupEndpointListener>(info.This());

      if (info.Length() == 0)
      {
        try
        {
          wrapper->_instance->Cleanup();
          return;
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }



    static void TagGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiGroupEndpointListener^>(info.This())) {
        return;
      }

      MidiGroupEndpointListener *wrapper = MidiGroupEndpointListener::Unwrap<MidiGroupEndpointListener>(info.This());

      try  {
        ::Platform::Object^ result = wrapper->_instance->Tag;
        info.GetReturnValue().Set(CreateOpaqueWrapper(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void TagSetter(Local<String> property, Local<Value> value, const Nan::PropertyCallbackInfo<void> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Platform::Object^>(value)) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Value to set is of unexpected type")));
        return;
      }

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiGroupEndpointListener^>(info.This())) {
        return;
      }

      MidiGroupEndpointListener *wrapper = MidiGroupEndpointListener::Unwrap<MidiGroupEndpointListener>(info.This());

      try {

        ::Platform::Object^ winRtValue = dynamic_cast<::Platform::Object^>(NodeRT::Utils::GetObjectInstance(value));

        wrapper->_instance->Tag = winRtValue;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
      }
    }
      
    static void NameGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiGroupEndpointListener^>(info.This())) {
        return;
      }

      MidiGroupEndpointListener *wrapper = MidiGroupEndpointListener::Unwrap<MidiGroupEndpointListener>(info.This());

      try  {
        Platform::String^ result = wrapper->_instance->Name;
        info.GetReturnValue().Set(NodeRT::Utils::NewString(result->Data()));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void NameSetter(Local<String> property, Local<Value> value, const Nan::PropertyCallbackInfo<void> &info) {
      HandleScope scope;

      if (!value->IsString()) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Value to set is of unexpected type")));
        return;
      }

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiGroupEndpointListener^>(info.This())) {
        return;
      }

      MidiGroupEndpointListener *wrapper = MidiGroupEndpointListener::Unwrap<MidiGroupEndpointListener>(info.This());

      try {

        Platform::String^ winRtValue = ref new Platform::String(NodeRT::Utils::StringToWchar(v8::String::Value(v8::Isolate::GetCurrent(), value)));

        wrapper->_instance->Name = winRtValue;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
      }
    }
      
    static void IsEnabledGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiGroupEndpointListener^>(info.This())) {
        return;
      }

      MidiGroupEndpointListener *wrapper = MidiGroupEndpointListener::Unwrap<MidiGroupEndpointListener>(info.This());

      try  {
        bool result = wrapper->_instance->IsEnabled;
        info.GetReturnValue().Set(Nan::New<Boolean>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void IsEnabledSetter(Local<String> property, Local<Value> value, const Nan::PropertyCallbackInfo<void> &info) {
      HandleScope scope;

      if (!value->IsBoolean()) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Value to set is of unexpected type")));
        return;
      }

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiGroupEndpointListener^>(info.This())) {
        return;
      }

      MidiGroupEndpointListener *wrapper = MidiGroupEndpointListener::Unwrap<MidiGroupEndpointListener>(info.This());

      try {

        bool winRtValue = Nan::To<bool>(value).FromMaybe(false);

        wrapper->_instance->IsEnabled = winRtValue;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
      }
    }
      
    static void IdGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiGroupEndpointListener^>(info.This())) {
        return;
      }

      MidiGroupEndpointListener *wrapper = MidiGroupEndpointListener::Unwrap<MidiGroupEndpointListener>(info.This());

      try  {
        Platform::String^ result = wrapper->_instance->Id;
        info.GetReturnValue().Set(NodeRT::Utils::NewString(result->Data()));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void IdSetter(Local<String> property, Local<Value> value, const Nan::PropertyCallbackInfo<void> &info) {
      HandleScope scope;

      if (!value->IsString()) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Value to set is of unexpected type")));
        return;
      }

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiGroupEndpointListener^>(info.This())) {
        return;
      }

      MidiGroupEndpointListener *wrapper = MidiGroupEndpointListener::Unwrap<MidiGroupEndpointListener>(info.This());

      try {

        Platform::String^ winRtValue = ref new Platform::String(NodeRT::Utils::StringToWchar(v8::String::Value(v8::Isolate::GetCurrent(), value)));

        wrapper->_instance->Id = winRtValue;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
      }
    }
      
    static void IncludeGroupsGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiGroupEndpointListener^>(info.This())) {
        return;
      }

      MidiGroupEndpointListener *wrapper = MidiGroupEndpointListener::Unwrap<MidiGroupEndpointListener>(info.This());

      try  {
        ::Windows::Foundation::Collections::IVector<::Windows::Devices::Midi2::MidiGroup^>^ result = wrapper->_instance->IncludeGroups;
        info.GetReturnValue().Set(NodeRT::Collections::VectorWrapper<::Windows::Devices::Midi2::MidiGroup^>::CreateVectorWrapper(result, 
            [](::Windows::Devices::Midi2::MidiGroup^ val) -> Local<Value> {
              return WrapMidiGroup(val);
            },
            [](Local<Value> value) -> bool {
              return NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiGroup^>(value);
            },
            [](Local<Value> value) -> ::Windows::Devices::Midi2::MidiGroup^ {
              return UnwrapMidiGroup(value);
            }
          ));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      


    static void AddListener(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() < 2 || !info[0]->IsString() || !info[1]->IsFunction()) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"wrong arguments, expected arguments are eventName(string),callback(function)")));
        return;
      }

      String::Value eventName(v8::Isolate::GetCurrent(), info[0]);
      auto str = *eventName;

      Local<Function> callback = info[1].As<Function>();

      ::Windows::Foundation::EventRegistrationToken registrationToken;
      if (NodeRT::Utils::CaseInsenstiveEquals(L"messageReceived", str))
      {
        if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiGroupEndpointListener^>(info.This()))
        {
          Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"The caller of this method isn't of the expected type or internal WinRt object was disposed")));
      return;
        }
        MidiGroupEndpointListener *wrapper = MidiGroupEndpointListener::Unwrap<MidiGroupEndpointListener>(info.This());
      
        try {
          Persistent<Object>* perstPtr = new Persistent<Object>();
          perstPtr->Reset(NodeRT::Utils::CreateCallbackObjectInDomain(callback));
          std::shared_ptr<Persistent<Object>> callbackObjPtr(perstPtr,
            [] (Persistent<Object> *ptr ) {
              NodeUtils::Async::RunOnMain([ptr]() {
                ptr->Reset();
                delete ptr;
            });
          });

          registrationToken = wrapper->_instance->MessageReceived::add(
            ref new ::Windows::Foundation::TypedEventHandler<::Windows::Devices::Midi2::IMidiMessageReceivedEventSource^, ::Windows::Devices::Midi2::MidiMessageReceivedEventArgs^>(
            [callbackObjPtr](::Windows::Devices::Midi2::IMidiMessageReceivedEventSource^ arg0, ::Windows::Devices::Midi2::MidiMessageReceivedEventArgs^ arg1) {
              NodeUtils::Async::RunOnMain([callbackObjPtr , arg0, arg1]() {
                HandleScope scope;


                Local<Value> wrappedArg0;
                Local<Value> wrappedArg1;

                {
                  TryCatch tryCatch;


                  wrappedArg0 = WrapIMidiMessageReceivedEventSource(arg0);
                  wrappedArg1 = WrapMidiMessageReceivedEventArgs(arg1);


                  if (wrappedArg0.IsEmpty()) wrappedArg0 = Undefined();
                  if (wrappedArg1.IsEmpty()) wrappedArg1 = Undefined();
                }

                Local<Value> args[] = { wrappedArg0, wrappedArg1 };
                Local<Object> callbackObjLocalRef = Nan::New<Object>(*callbackObjPtr);
                NodeRT::Utils::CallCallbackInDomain(callbackObjLocalRef, _countof(args), args);
              });
            })
          );
        }
        catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }

      }
 else  {
        Nan::ThrowError(Nan::Error(String::Concat(v8::Isolate::GetCurrent(), NodeRT::Utils::NewString(L"given event name isn't supported: "), info[0].As<String>())));
        return;
      }

      Local<Value> tokenMapVal = NodeRT::Utils::GetHiddenValue(callback, Nan::New<String>(REGISTRATION_TOKEN_MAP_PROPERTY_NAME).ToLocalChecked());
      Local<Object> tokenMap;

      if (tokenMapVal.IsEmpty() || Nan::Equals(tokenMapVal, Undefined()).FromMaybe(false)) {
        tokenMap = Nan::New<Object>();
        NodeRT::Utils::SetHiddenValueWithObject(callback, Nan::New<String>(REGISTRATION_TOKEN_MAP_PROPERTY_NAME).ToLocalChecked(), tokenMap);
      } else {
        tokenMap = Nan::To<Object>(tokenMapVal).ToLocalChecked();
      }

      Nan::Set(tokenMap, info[0], CreateOpaqueWrapper(::Windows::Foundation::PropertyValue::CreateInt64(registrationToken.Value)));
    }

    static void RemoveListener(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() < 2 || !info[0]->IsString() || !info[1]->IsFunction()) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"wrong arguments, expected a string and a callback")));
        return;
      }

      String::Value eventName(v8::Isolate::GetCurrent(), info[0]);
      auto str = *eventName;

      if ((!NodeRT::Utils::CaseInsenstiveEquals(L"messageReceived", str))) {
        Nan::ThrowError(Nan::Error(String::Concat(v8::Isolate::GetCurrent(), NodeRT::Utils::NewString(L"given event name isn't supported: "), info[0].As<String>())));
        return;
      }

      Local<Function> callback = info[1].As<Function>();
      Local<Value> tokenMap = NodeRT::Utils::GetHiddenValue(callback, Nan::New<String>(REGISTRATION_TOKEN_MAP_PROPERTY_NAME).ToLocalChecked());

      if (tokenMap.IsEmpty() || Nan::Equals(tokenMap, Undefined()).FromMaybe(false)) {
        return;
      }

      Local<Value> opaqueWrapperObj =  Nan::Get(Nan::To<Object>(tokenMap).ToLocalChecked(), info[0]).ToLocalChecked();

      if (opaqueWrapperObj.IsEmpty() || Nan::Equals(opaqueWrapperObj,Undefined()).FromMaybe(false)) {
        return;
      }

      OpaqueWrapper *opaqueWrapper = OpaqueWrapper::Unwrap<OpaqueWrapper>(opaqueWrapperObj.As<Object>());

      long long tokenValue = (long long) opaqueWrapper->GetObjectInstance();
      ::Windows::Foundation::EventRegistrationToken registrationToken;
      registrationToken.Value = tokenValue;

      try  {
        if (NodeRT::Utils::CaseInsenstiveEquals(L"messageReceived", str)) {
          if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiGroupEndpointListener^>(info.This()))
          {
            Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"The caller of this method isn't of the expected type or internal WinRt object was disposed")));
            return;
          }
          MidiGroupEndpointListener *wrapper = MidiGroupEndpointListener::Unwrap<MidiGroupEndpointListener>(info.This());
          wrapper->_instance->MessageReceived::remove(registrationToken);
        }
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
      }

      Nan::Delete(Nan::To<Object>(tokenMap).ToLocalChecked(), Nan::To<String>(info[0]).ToLocalChecked());
    }
    private:
      ::Windows::Devices::Midi2::MidiGroupEndpointListener^ _instance;
      static Persistent<FunctionTemplate> s_constructorTemplate;

      friend v8::Local<v8::Value> WrapMidiGroupEndpointListener(::Windows::Devices::Midi2::MidiGroupEndpointListener^ wintRtInstance);
      friend ::Windows::Devices::Midi2::MidiGroupEndpointListener^ UnwrapMidiGroupEndpointListener(Local<Value> value);
  };

  Persistent<FunctionTemplate> MidiGroupEndpointListener::s_constructorTemplate;

  v8::Local<v8::Value> WrapMidiGroupEndpointListener(::Windows::Devices::Midi2::MidiGroupEndpointListener^ winRtInstance) {
    EscapableHandleScope scope;

    if (winRtInstance == nullptr) {
      return scope.Escape(Undefined());
    }

    Local<Value> opaqueWrapper = CreateOpaqueWrapper(winRtInstance);
    Local<Value> args[] = {opaqueWrapper};
    Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(MidiGroupEndpointListener::s_constructorTemplate);
    return scope.Escape(Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(),_countof(args), args).ToLocalChecked());
  }

  ::Windows::Devices::Midi2::MidiGroupEndpointListener^ UnwrapMidiGroupEndpointListener(Local<Value> value) {
     return MidiGroupEndpointListener::Unwrap<MidiGroupEndpointListener>(Nan::To<Object>(value).ToLocalChecked())->_instance;
  }

  void InitMidiGroupEndpointListener(Local<Object> exports) {
    MidiGroupEndpointListener::Init(exports);
  }

  class MidiGroupTerminalBlock : public WrapperBase {
    public:
      
      static void Init(const Local<Object> exports) {
        HandleScope scope;

        Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(New);
        s_constructorTemplate.Reset(localRef);
        localRef->SetClassName(Nan::New<String>("MidiGroupTerminalBlock").ToLocalChecked());
        localRef->InstanceTemplate()->SetInternalFieldCount(1);


          
            Nan::SetPrototypeMethod(localRef, "includesGroup", IncludesGroup);
          



          
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("calculatedMaxDeviceInputBandwidthBitsPerSecond").ToLocalChecked(), CalculatedMaxDeviceInputBandwidthBitsPerSecondGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("calculatedMaxDeviceOutputBandwidthBitsPerSecond").ToLocalChecked(), CalculatedMaxDeviceOutputBandwidthBitsPerSecondGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("direction").ToLocalChecked(), DirectionGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("firstGroupIndex").ToLocalChecked(), FirstGroupIndexGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("groupCount").ToLocalChecked(), GroupCountGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("maxDeviceInputBandwidthIn4KBitsPerSecondUnits").ToLocalChecked(), MaxDeviceInputBandwidthIn4KBitsPerSecondUnitsGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("maxDeviceOutputBandwidthIn4KBitsPerSecondUnits").ToLocalChecked(), MaxDeviceOutputBandwidthIn4KBitsPerSecondUnitsGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("name").ToLocalChecked(), NameGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("number").ToLocalChecked(), NumberGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("protocol").ToLocalChecked(), ProtocolGetter);

        Local<Object> constructor = Nan::To<Object>(Nan::GetFunction(localRef).ToLocalChecked()).ToLocalChecked();
        Nan::SetMethod(constructor, "castFrom", CastFrom);



        Nan::Set(exports, Nan::New<String>("MidiGroupTerminalBlock").ToLocalChecked(), constructor);
      }

      virtual ::Platform::Object^ GetObjectInstance() const override {
        return _instance;
      }

    private:

      MidiGroupTerminalBlock(::Windows::Devices::Midi2::MidiGroupTerminalBlock^ instance) {
        _instance = instance;
      }

      
    static void New(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(s_constructorTemplate);

      // in case the constructor was called without the new operator
      if (!localRef->HasInstance(info.This())) {
        if (info.Length() > 0) {
          std::unique_ptr<Local<Value> []> constructorArgs(new Local<Value>[info.Length()]);

          Local<Value> *argsPtr = constructorArgs.get();
          for (int i = 0; i < info.Length(); i++) {
            argsPtr[i] = info[i];
          }

          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), constructorArgs.get());
          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        } else {
          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), nullptr);

          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        }
      }

      ::Windows::Devices::Midi2::MidiGroupTerminalBlock^ winRtInstance;


      if (info.Length() == 1 && OpaqueWrapper::IsOpaqueWrapper(info[0]) &&
        NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiGroupTerminalBlock^>(info[0])) {
        try {
          winRtInstance = (::Windows::Devices::Midi2::MidiGroupTerminalBlock^) NodeRT::Utils::GetObjectInstance(info[0]);
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no suitable constructor found")));
        return;
      }

      NodeRT::Utils::SetHiddenValue(info.This(), Nan::New<String>("__winRtInstance__").ToLocalChecked(), True());

      MidiGroupTerminalBlock *wrapperInstance = new MidiGroupTerminalBlock(winRtInstance);
      wrapperInstance->Wrap(info.This());

      info.GetReturnValue().Set(info.This());
    }


      
    static void CastFrom(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;
      if (info.Length() < 1 || !NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiGroupTerminalBlock^>(info[0])) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no object provided, or given object could not be casted to requested type")));
        return;
      }

      ::Windows::Devices::Midi2::MidiGroupTerminalBlock^ winRtInstance;
      try {
        winRtInstance = (::Windows::Devices::Midi2::MidiGroupTerminalBlock^) NodeRT::Utils::GetObjectInstance(info[0]);
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }

      info.GetReturnValue().Set(WrapMidiGroupTerminalBlock(winRtInstance));
    }


    static void IncludesGroup(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiGroupTerminalBlock^>(info.This())) {
        return;
      }

      MidiGroupTerminalBlock *wrapper = MidiGroupTerminalBlock::Unwrap<MidiGroupTerminalBlock>(info.This());

      if (info.Length() == 1
        && NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiGroup^>(info[0]))
      {
        try
        {
          ::Windows::Devices::Midi2::MidiGroup^ arg0 = UnwrapMidiGroup(info[0]);
          
          bool result;
          result = wrapper->_instance->IncludesGroup(arg0);
          info.GetReturnValue().Set(Nan::New<Boolean>(result));
          return;
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }



    static void CalculatedMaxDeviceInputBandwidthBitsPerSecondGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiGroupTerminalBlock^>(info.This())) {
        return;
      }

      MidiGroupTerminalBlock *wrapper = MidiGroupTerminalBlock::Unwrap<MidiGroupTerminalBlock>(info.This());

      try  {
        unsigned int result = wrapper->_instance->CalculatedMaxDeviceInputBandwidthBitsPerSecond;
        info.GetReturnValue().Set(Nan::New<Integer>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void CalculatedMaxDeviceOutputBandwidthBitsPerSecondGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiGroupTerminalBlock^>(info.This())) {
        return;
      }

      MidiGroupTerminalBlock *wrapper = MidiGroupTerminalBlock::Unwrap<MidiGroupTerminalBlock>(info.This());

      try  {
        unsigned int result = wrapper->_instance->CalculatedMaxDeviceOutputBandwidthBitsPerSecond;
        info.GetReturnValue().Set(Nan::New<Integer>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void DirectionGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiGroupTerminalBlock^>(info.This())) {
        return;
      }

      MidiGroupTerminalBlock *wrapper = MidiGroupTerminalBlock::Unwrap<MidiGroupTerminalBlock>(info.This());

      try  {
        ::Windows::Devices::Midi2::MidiGroupTerminalBlockDirection result = wrapper->_instance->Direction;
        info.GetReturnValue().Set(Nan::New<Integer>(static_cast<int>(result)));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void FirstGroupIndexGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiGroupTerminalBlock^>(info.This())) {
        return;
      }

      MidiGroupTerminalBlock *wrapper = MidiGroupTerminalBlock::Unwrap<MidiGroupTerminalBlock>(info.This());

      try  {
        unsigned char result = wrapper->_instance->FirstGroupIndex;
        info.GetReturnValue().Set(Nan::New<Integer>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void GroupCountGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiGroupTerminalBlock^>(info.This())) {
        return;
      }

      MidiGroupTerminalBlock *wrapper = MidiGroupTerminalBlock::Unwrap<MidiGroupTerminalBlock>(info.This());

      try  {
        unsigned char result = wrapper->_instance->GroupCount;
        info.GetReturnValue().Set(Nan::New<Integer>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void MaxDeviceInputBandwidthIn4KBitsPerSecondUnitsGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiGroupTerminalBlock^>(info.This())) {
        return;
      }

      MidiGroupTerminalBlock *wrapper = MidiGroupTerminalBlock::Unwrap<MidiGroupTerminalBlock>(info.This());

      try  {
        unsigned short result = wrapper->_instance->MaxDeviceInputBandwidthIn4KBitsPerSecondUnits;
        info.GetReturnValue().Set(Nan::New<Integer>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void MaxDeviceOutputBandwidthIn4KBitsPerSecondUnitsGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiGroupTerminalBlock^>(info.This())) {
        return;
      }

      MidiGroupTerminalBlock *wrapper = MidiGroupTerminalBlock::Unwrap<MidiGroupTerminalBlock>(info.This());

      try  {
        unsigned short result = wrapper->_instance->MaxDeviceOutputBandwidthIn4KBitsPerSecondUnits;
        info.GetReturnValue().Set(Nan::New<Integer>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void NameGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiGroupTerminalBlock^>(info.This())) {
        return;
      }

      MidiGroupTerminalBlock *wrapper = MidiGroupTerminalBlock::Unwrap<MidiGroupTerminalBlock>(info.This());

      try  {
        Platform::String^ result = wrapper->_instance->Name;
        info.GetReturnValue().Set(NodeRT::Utils::NewString(result->Data()));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void NumberGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiGroupTerminalBlock^>(info.This())) {
        return;
      }

      MidiGroupTerminalBlock *wrapper = MidiGroupTerminalBlock::Unwrap<MidiGroupTerminalBlock>(info.This());

      try  {
        unsigned char result = wrapper->_instance->Number;
        info.GetReturnValue().Set(Nan::New<Integer>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void ProtocolGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiGroupTerminalBlock^>(info.This())) {
        return;
      }

      MidiGroupTerminalBlock *wrapper = MidiGroupTerminalBlock::Unwrap<MidiGroupTerminalBlock>(info.This());

      try  {
        ::Windows::Devices::Midi2::MidiGroupTerminalBlockProtocol result = wrapper->_instance->Protocol;
        info.GetReturnValue().Set(Nan::New<Integer>(static_cast<int>(result)));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      


    private:
      ::Windows::Devices::Midi2::MidiGroupTerminalBlock^ _instance;
      static Persistent<FunctionTemplate> s_constructorTemplate;

      friend v8::Local<v8::Value> WrapMidiGroupTerminalBlock(::Windows::Devices::Midi2::MidiGroupTerminalBlock^ wintRtInstance);
      friend ::Windows::Devices::Midi2::MidiGroupTerminalBlock^ UnwrapMidiGroupTerminalBlock(Local<Value> value);
  };

  Persistent<FunctionTemplate> MidiGroupTerminalBlock::s_constructorTemplate;

  v8::Local<v8::Value> WrapMidiGroupTerminalBlock(::Windows::Devices::Midi2::MidiGroupTerminalBlock^ winRtInstance) {
    EscapableHandleScope scope;

    if (winRtInstance == nullptr) {
      return scope.Escape(Undefined());
    }

    Local<Value> opaqueWrapper = CreateOpaqueWrapper(winRtInstance);
    Local<Value> args[] = {opaqueWrapper};
    Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(MidiGroupTerminalBlock::s_constructorTemplate);
    return scope.Escape(Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(),_countof(args), args).ToLocalChecked());
  }

  ::Windows::Devices::Midi2::MidiGroupTerminalBlock^ UnwrapMidiGroupTerminalBlock(Local<Value> value) {
     return MidiGroupTerminalBlock::Unwrap<MidiGroupTerminalBlock>(Nan::To<Object>(value).ToLocalChecked())->_instance;
  }

  void InitMidiGroupTerminalBlock(Local<Object> exports) {
    MidiGroupTerminalBlock::Init(exports);
  }

  class MidiMessage128 : public WrapperBase {
    public:
      
      static void Init(const Local<Object> exports) {
        HandleScope scope;

        Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(New);
        s_constructorTemplate.Reset(localRef);
        localRef->SetClassName(Nan::New<String>("MidiMessage128").ToLocalChecked());
        localRef->InstanceTemplate()->SetInternalFieldCount(1);


          
            Nan::SetPrototypeMethod(localRef, "peekFirstWord", PeekFirstWord);
            Nan::SetPrototypeMethod(localRef, "toString", ToString);
          



          
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("word3").ToLocalChecked(), Word3Getter, Word3Setter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("word2").ToLocalChecked(), Word2Getter, Word2Setter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("word1").ToLocalChecked(), Word1Getter, Word1Setter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("word0").ToLocalChecked(), Word0Getter, Word0Setter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("timestamp").ToLocalChecked(), TimestampGetter, TimestampSetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("messageType").ToLocalChecked(), MessageTypeGetter, MessageTypeSetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("packetType").ToLocalChecked(), PacketTypeGetter);

        Local<Object> constructor = Nan::To<Object>(Nan::GetFunction(localRef).ToLocalChecked()).ToLocalChecked();
        Nan::SetMethod(constructor, "castFrom", CastFrom);



        Nan::Set(exports, Nan::New<String>("MidiMessage128").ToLocalChecked(), constructor);
      }

      virtual ::Platform::Object^ GetObjectInstance() const override {
        return _instance;
      }

    private:

      MidiMessage128(::Windows::Devices::Midi2::MidiMessage128^ instance) {
        _instance = instance;
      }

      
    static void New(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(s_constructorTemplate);

      // in case the constructor was called without the new operator
      if (!localRef->HasInstance(info.This())) {
        if (info.Length() > 0) {
          std::unique_ptr<Local<Value> []> constructorArgs(new Local<Value>[info.Length()]);

          Local<Value> *argsPtr = constructorArgs.get();
          for (int i = 0; i < info.Length(); i++) {
            argsPtr[i] = info[i];
          }

          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), constructorArgs.get());
          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        } else {
          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), nullptr);

          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        }
      }

      ::Windows::Devices::Midi2::MidiMessage128^ winRtInstance;


      if (info.Length() == 1 && OpaqueWrapper::IsOpaqueWrapper(info[0]) &&
        NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessage128^>(info[0])) {
        try {
          winRtInstance = (::Windows::Devices::Midi2::MidiMessage128^) NodeRT::Utils::GetObjectInstance(info[0]);
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
      else if (info.Length() == 0)
      {
        try {
          winRtInstance = ref new ::Windows::Devices::Midi2::MidiMessage128();
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
      else if (info.Length() == 5
        && info[0]->IsNumber()
        && info[1]->IsUint32()
        && info[2]->IsUint32()
        && info[3]->IsUint32()
        && info[4]->IsUint32())
      {
        try {
          unsigned __int64 arg0 = static_cast<unsigned __int64>(Nan::To<int64_t>(info[0]).FromMaybe(0));
          unsigned int arg1 = static_cast<unsigned int>(Nan::To<uint32_t>(info[1]).FromMaybe(0));
          unsigned int arg2 = static_cast<unsigned int>(Nan::To<uint32_t>(info[2]).FromMaybe(0));
          unsigned int arg3 = static_cast<unsigned int>(Nan::To<uint32_t>(info[3]).FromMaybe(0));
          unsigned int arg4 = static_cast<unsigned int>(Nan::To<uint32_t>(info[4]).FromMaybe(0));
          
          winRtInstance = ref new ::Windows::Devices::Midi2::MidiMessage128(arg0,arg1,arg2,arg3,arg4);
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
      else if (info.Length() == 2
        && info[0]->IsNumber()
        && (NodeRT::Utils::IsWinRtWrapperOf<::Platform::Array<unsigned int>^>(info[1]) || info[1]->IsArray()))
      {
        try {
          unsigned __int64 arg0 = static_cast<unsigned __int64>(Nan::To<int64_t>(info[0]).FromMaybe(0));
          ::Platform::Array<unsigned int>^ arg1 = 
            [] (v8::Local<v8::Value> value) -> ::Platform::Array<unsigned int>^
            {
              if (value->IsArray())
              {
                return NodeRT::Collections::JsArrayToWinrtArray<unsigned int>(value.As<Array>(), 
                 [](Local<Value> value) -> bool {
                   return value->IsUint32();
                 },
                 [](Local<Value> value) -> unsigned int {
                   return static_cast<unsigned int>(Nan::To<uint32_t>(value).FromMaybe(0));
                 }
                );
              }
              else
              {
                return dynamic_cast<::Platform::Array<unsigned int>^>(NodeRT::Utils::GetObjectInstance(value));
              }
            } (info[1]);
          
          winRtInstance = ref new ::Windows::Devices::Midi2::MidiMessage128(arg0,arg1);
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no suitable constructor found")));
        return;
      }

      NodeRT::Utils::SetHiddenValue(info.This(), Nan::New<String>("__winRtInstance__").ToLocalChecked(), True());

      MidiMessage128 *wrapperInstance = new MidiMessage128(winRtInstance);
      wrapperInstance->Wrap(info.This());

      info.GetReturnValue().Set(info.This());
    }


      
    static void CastFrom(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;
      if (info.Length() < 1 || !NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessage128^>(info[0])) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no object provided, or given object could not be casted to requested type")));
        return;
      }

      ::Windows::Devices::Midi2::MidiMessage128^ winRtInstance;
      try {
        winRtInstance = (::Windows::Devices::Midi2::MidiMessage128^) NodeRT::Utils::GetObjectInstance(info[0]);
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }

      info.GetReturnValue().Set(WrapMidiMessage128(winRtInstance));
    }


    static void PeekFirstWord(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessage128^>(info.This())) {
        return;
      }

      MidiMessage128 *wrapper = MidiMessage128::Unwrap<MidiMessage128>(info.This());

      if (info.Length() == 0)
      {
        try
        {
          unsigned int result;
          result = wrapper->_instance->PeekFirstWord();
          info.GetReturnValue().Set(Nan::New<Integer>(result));
          return;
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }
    static void ToString(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessage128^>(info.This())) {
        return;
      }

      MidiMessage128 *wrapper = MidiMessage128::Unwrap<MidiMessage128>(info.This());

      if (info.Length() == 0)
      {
        try
        {
          Platform::String^ result;
          result = wrapper->_instance->ToString();
          info.GetReturnValue().Set(NodeRT::Utils::NewString(result->Data()));
          return;
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }



    static void Word3Getter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessage128^>(info.This())) {
        return;
      }

      MidiMessage128 *wrapper = MidiMessage128::Unwrap<MidiMessage128>(info.This());

      try  {
        unsigned int result = wrapper->_instance->Word3;
        info.GetReturnValue().Set(Nan::New<Integer>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void Word3Setter(Local<String> property, Local<Value> value, const Nan::PropertyCallbackInfo<void> &info) {
      HandleScope scope;

      if (!value->IsUint32()) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Value to set is of unexpected type")));
        return;
      }

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessage128^>(info.This())) {
        return;
      }

      MidiMessage128 *wrapper = MidiMessage128::Unwrap<MidiMessage128>(info.This());

      try {

        unsigned int winRtValue = static_cast<unsigned int>(Nan::To<uint32_t>(value).FromMaybe(0));

        wrapper->_instance->Word3 = winRtValue;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
      }
    }
      
    static void Word2Getter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessage128^>(info.This())) {
        return;
      }

      MidiMessage128 *wrapper = MidiMessage128::Unwrap<MidiMessage128>(info.This());

      try  {
        unsigned int result = wrapper->_instance->Word2;
        info.GetReturnValue().Set(Nan::New<Integer>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void Word2Setter(Local<String> property, Local<Value> value, const Nan::PropertyCallbackInfo<void> &info) {
      HandleScope scope;

      if (!value->IsUint32()) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Value to set is of unexpected type")));
        return;
      }

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessage128^>(info.This())) {
        return;
      }

      MidiMessage128 *wrapper = MidiMessage128::Unwrap<MidiMessage128>(info.This());

      try {

        unsigned int winRtValue = static_cast<unsigned int>(Nan::To<uint32_t>(value).FromMaybe(0));

        wrapper->_instance->Word2 = winRtValue;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
      }
    }
      
    static void Word1Getter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessage128^>(info.This())) {
        return;
      }

      MidiMessage128 *wrapper = MidiMessage128::Unwrap<MidiMessage128>(info.This());

      try  {
        unsigned int result = wrapper->_instance->Word1;
        info.GetReturnValue().Set(Nan::New<Integer>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void Word1Setter(Local<String> property, Local<Value> value, const Nan::PropertyCallbackInfo<void> &info) {
      HandleScope scope;

      if (!value->IsUint32()) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Value to set is of unexpected type")));
        return;
      }

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessage128^>(info.This())) {
        return;
      }

      MidiMessage128 *wrapper = MidiMessage128::Unwrap<MidiMessage128>(info.This());

      try {

        unsigned int winRtValue = static_cast<unsigned int>(Nan::To<uint32_t>(value).FromMaybe(0));

        wrapper->_instance->Word1 = winRtValue;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
      }
    }
      
    static void Word0Getter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessage128^>(info.This())) {
        return;
      }

      MidiMessage128 *wrapper = MidiMessage128::Unwrap<MidiMessage128>(info.This());

      try  {
        unsigned int result = wrapper->_instance->Word0;
        info.GetReturnValue().Set(Nan::New<Integer>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void Word0Setter(Local<String> property, Local<Value> value, const Nan::PropertyCallbackInfo<void> &info) {
      HandleScope scope;

      if (!value->IsUint32()) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Value to set is of unexpected type")));
        return;
      }

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessage128^>(info.This())) {
        return;
      }

      MidiMessage128 *wrapper = MidiMessage128::Unwrap<MidiMessage128>(info.This());

      try {

        unsigned int winRtValue = static_cast<unsigned int>(Nan::To<uint32_t>(value).FromMaybe(0));

        wrapper->_instance->Word0 = winRtValue;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
      }
    }
      
    static void TimestampGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessage128^>(info.This())) {
        return;
      }

      MidiMessage128 *wrapper = MidiMessage128::Unwrap<MidiMessage128>(info.This());

      try  {
        unsigned __int64 result = wrapper->_instance->Timestamp;
        info.GetReturnValue().Set(Nan::New<Number>(static_cast<double>(result)));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void TimestampSetter(Local<String> property, Local<Value> value, const Nan::PropertyCallbackInfo<void> &info) {
      HandleScope scope;

      if (!value->IsNumber()) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Value to set is of unexpected type")));
        return;
      }

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessage128^>(info.This())) {
        return;
      }

      MidiMessage128 *wrapper = MidiMessage128::Unwrap<MidiMessage128>(info.This());

      try {

        unsigned __int64 winRtValue = static_cast<unsigned __int64>(Nan::To<int64_t>(value).FromMaybe(0));

        wrapper->_instance->Timestamp = winRtValue;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
      }
    }
      
    static void MessageTypeGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessage128^>(info.This())) {
        return;
      }

      MidiMessage128 *wrapper = MidiMessage128::Unwrap<MidiMessage128>(info.This());

      try  {
        ::Windows::Devices::Midi2::MidiMessageType result = wrapper->_instance->MessageType;
        info.GetReturnValue().Set(Nan::New<Integer>(static_cast<int>(result)));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void MessageTypeSetter(Local<String> property, Local<Value> value, const Nan::PropertyCallbackInfo<void> &info) {
      HandleScope scope;

      if (!value->IsInt32()) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Value to set is of unexpected type")));
        return;
      }

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessage128^>(info.This())) {
        return;
      }

      MidiMessage128 *wrapper = MidiMessage128::Unwrap<MidiMessage128>(info.This());

      try {

        ::Windows::Devices::Midi2::MidiMessageType winRtValue = static_cast<::Windows::Devices::Midi2::MidiMessageType>(Nan::To<int32_t>(value).FromMaybe(0));

        wrapper->_instance->MessageType = winRtValue;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
      }
    }
      
    static void PacketTypeGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessage128^>(info.This())) {
        return;
      }

      MidiMessage128 *wrapper = MidiMessage128::Unwrap<MidiMessage128>(info.This());

      try  {
        ::Windows::Devices::Midi2::MidiPacketType result = wrapper->_instance->PacketType;
        info.GetReturnValue().Set(Nan::New<Integer>(static_cast<int>(result)));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      


    private:
      ::Windows::Devices::Midi2::MidiMessage128^ _instance;
      static Persistent<FunctionTemplate> s_constructorTemplate;

      friend v8::Local<v8::Value> WrapMidiMessage128(::Windows::Devices::Midi2::MidiMessage128^ wintRtInstance);
      friend ::Windows::Devices::Midi2::MidiMessage128^ UnwrapMidiMessage128(Local<Value> value);
  };

  Persistent<FunctionTemplate> MidiMessage128::s_constructorTemplate;

  v8::Local<v8::Value> WrapMidiMessage128(::Windows::Devices::Midi2::MidiMessage128^ winRtInstance) {
    EscapableHandleScope scope;

    if (winRtInstance == nullptr) {
      return scope.Escape(Undefined());
    }

    Local<Value> opaqueWrapper = CreateOpaqueWrapper(winRtInstance);
    Local<Value> args[] = {opaqueWrapper};
    Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(MidiMessage128::s_constructorTemplate);
    return scope.Escape(Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(),_countof(args), args).ToLocalChecked());
  }

  ::Windows::Devices::Midi2::MidiMessage128^ UnwrapMidiMessage128(Local<Value> value) {
     return MidiMessage128::Unwrap<MidiMessage128>(Nan::To<Object>(value).ToLocalChecked())->_instance;
  }

  void InitMidiMessage128(Local<Object> exports) {
    MidiMessage128::Init(exports);
  }

  class MidiMessage32 : public WrapperBase {
    public:
      
      static void Init(const Local<Object> exports) {
        HandleScope scope;

        Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(New);
        s_constructorTemplate.Reset(localRef);
        localRef->SetClassName(Nan::New<String>("MidiMessage32").ToLocalChecked());
        localRef->InstanceTemplate()->SetInternalFieldCount(1);


          
            Nan::SetPrototypeMethod(localRef, "peekFirstWord", PeekFirstWord);
            Nan::SetPrototypeMethod(localRef, "toString", ToString);
          



          
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("word0").ToLocalChecked(), Word0Getter, Word0Setter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("timestamp").ToLocalChecked(), TimestampGetter, TimestampSetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("messageType").ToLocalChecked(), MessageTypeGetter, MessageTypeSetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("packetType").ToLocalChecked(), PacketTypeGetter);

        Local<Object> constructor = Nan::To<Object>(Nan::GetFunction(localRef).ToLocalChecked()).ToLocalChecked();
        Nan::SetMethod(constructor, "castFrom", CastFrom);



        Nan::Set(exports, Nan::New<String>("MidiMessage32").ToLocalChecked(), constructor);
      }

      virtual ::Platform::Object^ GetObjectInstance() const override {
        return _instance;
      }

    private:

      MidiMessage32(::Windows::Devices::Midi2::MidiMessage32^ instance) {
        _instance = instance;
      }

      
    static void New(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(s_constructorTemplate);

      // in case the constructor was called without the new operator
      if (!localRef->HasInstance(info.This())) {
        if (info.Length() > 0) {
          std::unique_ptr<Local<Value> []> constructorArgs(new Local<Value>[info.Length()]);

          Local<Value> *argsPtr = constructorArgs.get();
          for (int i = 0; i < info.Length(); i++) {
            argsPtr[i] = info[i];
          }

          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), constructorArgs.get());
          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        } else {
          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), nullptr);

          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        }
      }

      ::Windows::Devices::Midi2::MidiMessage32^ winRtInstance;


      if (info.Length() == 1 && OpaqueWrapper::IsOpaqueWrapper(info[0]) &&
        NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessage32^>(info[0])) {
        try {
          winRtInstance = (::Windows::Devices::Midi2::MidiMessage32^) NodeRT::Utils::GetObjectInstance(info[0]);
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
      else if (info.Length() == 0)
      {
        try {
          winRtInstance = ref new ::Windows::Devices::Midi2::MidiMessage32();
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
      else if (info.Length() == 2
        && info[0]->IsNumber()
        && info[1]->IsUint32())
      {
        try {
          unsigned __int64 arg0 = static_cast<unsigned __int64>(Nan::To<int64_t>(info[0]).FromMaybe(0));
          unsigned int arg1 = static_cast<unsigned int>(Nan::To<uint32_t>(info[1]).FromMaybe(0));
          
          winRtInstance = ref new ::Windows::Devices::Midi2::MidiMessage32(arg0,arg1);
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no suitable constructor found")));
        return;
      }

      NodeRT::Utils::SetHiddenValue(info.This(), Nan::New<String>("__winRtInstance__").ToLocalChecked(), True());

      MidiMessage32 *wrapperInstance = new MidiMessage32(winRtInstance);
      wrapperInstance->Wrap(info.This());

      info.GetReturnValue().Set(info.This());
    }


      
    static void CastFrom(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;
      if (info.Length() < 1 || !NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessage32^>(info[0])) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no object provided, or given object could not be casted to requested type")));
        return;
      }

      ::Windows::Devices::Midi2::MidiMessage32^ winRtInstance;
      try {
        winRtInstance = (::Windows::Devices::Midi2::MidiMessage32^) NodeRT::Utils::GetObjectInstance(info[0]);
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }

      info.GetReturnValue().Set(WrapMidiMessage32(winRtInstance));
    }


    static void PeekFirstWord(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessage32^>(info.This())) {
        return;
      }

      MidiMessage32 *wrapper = MidiMessage32::Unwrap<MidiMessage32>(info.This());

      if (info.Length() == 0)
      {
        try
        {
          unsigned int result;
          result = wrapper->_instance->PeekFirstWord();
          info.GetReturnValue().Set(Nan::New<Integer>(result));
          return;
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }
    static void ToString(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessage32^>(info.This())) {
        return;
      }

      MidiMessage32 *wrapper = MidiMessage32::Unwrap<MidiMessage32>(info.This());

      if (info.Length() == 0)
      {
        try
        {
          Platform::String^ result;
          result = wrapper->_instance->ToString();
          info.GetReturnValue().Set(NodeRT::Utils::NewString(result->Data()));
          return;
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }



    static void Word0Getter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessage32^>(info.This())) {
        return;
      }

      MidiMessage32 *wrapper = MidiMessage32::Unwrap<MidiMessage32>(info.This());

      try  {
        unsigned int result = wrapper->_instance->Word0;
        info.GetReturnValue().Set(Nan::New<Integer>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void Word0Setter(Local<String> property, Local<Value> value, const Nan::PropertyCallbackInfo<void> &info) {
      HandleScope scope;

      if (!value->IsUint32()) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Value to set is of unexpected type")));
        return;
      }

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessage32^>(info.This())) {
        return;
      }

      MidiMessage32 *wrapper = MidiMessage32::Unwrap<MidiMessage32>(info.This());

      try {

        unsigned int winRtValue = static_cast<unsigned int>(Nan::To<uint32_t>(value).FromMaybe(0));

        wrapper->_instance->Word0 = winRtValue;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
      }
    }
      
    static void TimestampGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessage32^>(info.This())) {
        return;
      }

      MidiMessage32 *wrapper = MidiMessage32::Unwrap<MidiMessage32>(info.This());

      try  {
        unsigned __int64 result = wrapper->_instance->Timestamp;
        info.GetReturnValue().Set(Nan::New<Number>(static_cast<double>(result)));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void TimestampSetter(Local<String> property, Local<Value> value, const Nan::PropertyCallbackInfo<void> &info) {
      HandleScope scope;

      if (!value->IsNumber()) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Value to set is of unexpected type")));
        return;
      }

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessage32^>(info.This())) {
        return;
      }

      MidiMessage32 *wrapper = MidiMessage32::Unwrap<MidiMessage32>(info.This());

      try {

        unsigned __int64 winRtValue = static_cast<unsigned __int64>(Nan::To<int64_t>(value).FromMaybe(0));

        wrapper->_instance->Timestamp = winRtValue;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
      }
    }
      
    static void MessageTypeGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessage32^>(info.This())) {
        return;
      }

      MidiMessage32 *wrapper = MidiMessage32::Unwrap<MidiMessage32>(info.This());

      try  {
        ::Windows::Devices::Midi2::MidiMessageType result = wrapper->_instance->MessageType;
        info.GetReturnValue().Set(Nan::New<Integer>(static_cast<int>(result)));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void MessageTypeSetter(Local<String> property, Local<Value> value, const Nan::PropertyCallbackInfo<void> &info) {
      HandleScope scope;

      if (!value->IsInt32()) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Value to set is of unexpected type")));
        return;
      }

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessage32^>(info.This())) {
        return;
      }

      MidiMessage32 *wrapper = MidiMessage32::Unwrap<MidiMessage32>(info.This());

      try {

        ::Windows::Devices::Midi2::MidiMessageType winRtValue = static_cast<::Windows::Devices::Midi2::MidiMessageType>(Nan::To<int32_t>(value).FromMaybe(0));

        wrapper->_instance->MessageType = winRtValue;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
      }
    }
      
    static void PacketTypeGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessage32^>(info.This())) {
        return;
      }

      MidiMessage32 *wrapper = MidiMessage32::Unwrap<MidiMessage32>(info.This());

      try  {
        ::Windows::Devices::Midi2::MidiPacketType result = wrapper->_instance->PacketType;
        info.GetReturnValue().Set(Nan::New<Integer>(static_cast<int>(result)));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      


    private:
      ::Windows::Devices::Midi2::MidiMessage32^ _instance;
      static Persistent<FunctionTemplate> s_constructorTemplate;

      friend v8::Local<v8::Value> WrapMidiMessage32(::Windows::Devices::Midi2::MidiMessage32^ wintRtInstance);
      friend ::Windows::Devices::Midi2::MidiMessage32^ UnwrapMidiMessage32(Local<Value> value);
  };

  Persistent<FunctionTemplate> MidiMessage32::s_constructorTemplate;

  v8::Local<v8::Value> WrapMidiMessage32(::Windows::Devices::Midi2::MidiMessage32^ winRtInstance) {
    EscapableHandleScope scope;

    if (winRtInstance == nullptr) {
      return scope.Escape(Undefined());
    }

    Local<Value> opaqueWrapper = CreateOpaqueWrapper(winRtInstance);
    Local<Value> args[] = {opaqueWrapper};
    Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(MidiMessage32::s_constructorTemplate);
    return scope.Escape(Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(),_countof(args), args).ToLocalChecked());
  }

  ::Windows::Devices::Midi2::MidiMessage32^ UnwrapMidiMessage32(Local<Value> value) {
     return MidiMessage32::Unwrap<MidiMessage32>(Nan::To<Object>(value).ToLocalChecked())->_instance;
  }

  void InitMidiMessage32(Local<Object> exports) {
    MidiMessage32::Init(exports);
  }

  class MidiMessage64 : public WrapperBase {
    public:
      
      static void Init(const Local<Object> exports) {
        HandleScope scope;

        Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(New);
        s_constructorTemplate.Reset(localRef);
        localRef->SetClassName(Nan::New<String>("MidiMessage64").ToLocalChecked());
        localRef->InstanceTemplate()->SetInternalFieldCount(1);


          
            Nan::SetPrototypeMethod(localRef, "peekFirstWord", PeekFirstWord);
            Nan::SetPrototypeMethod(localRef, "toString", ToString);
          



          
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("word1").ToLocalChecked(), Word1Getter, Word1Setter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("word0").ToLocalChecked(), Word0Getter, Word0Setter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("timestamp").ToLocalChecked(), TimestampGetter, TimestampSetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("messageType").ToLocalChecked(), MessageTypeGetter, MessageTypeSetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("packetType").ToLocalChecked(), PacketTypeGetter);

        Local<Object> constructor = Nan::To<Object>(Nan::GetFunction(localRef).ToLocalChecked()).ToLocalChecked();
        Nan::SetMethod(constructor, "castFrom", CastFrom);



        Nan::Set(exports, Nan::New<String>("MidiMessage64").ToLocalChecked(), constructor);
      }

      virtual ::Platform::Object^ GetObjectInstance() const override {
        return _instance;
      }

    private:

      MidiMessage64(::Windows::Devices::Midi2::MidiMessage64^ instance) {
        _instance = instance;
      }

      
    static void New(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(s_constructorTemplate);

      // in case the constructor was called without the new operator
      if (!localRef->HasInstance(info.This())) {
        if (info.Length() > 0) {
          std::unique_ptr<Local<Value> []> constructorArgs(new Local<Value>[info.Length()]);

          Local<Value> *argsPtr = constructorArgs.get();
          for (int i = 0; i < info.Length(); i++) {
            argsPtr[i] = info[i];
          }

          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), constructorArgs.get());
          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        } else {
          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), nullptr);

          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        }
      }

      ::Windows::Devices::Midi2::MidiMessage64^ winRtInstance;


      if (info.Length() == 1 && OpaqueWrapper::IsOpaqueWrapper(info[0]) &&
        NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessage64^>(info[0])) {
        try {
          winRtInstance = (::Windows::Devices::Midi2::MidiMessage64^) NodeRT::Utils::GetObjectInstance(info[0]);
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
      else if (info.Length() == 0)
      {
        try {
          winRtInstance = ref new ::Windows::Devices::Midi2::MidiMessage64();
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
      else if (info.Length() == 3
        && info[0]->IsNumber()
        && info[1]->IsUint32()
        && info[2]->IsUint32())
      {
        try {
          unsigned __int64 arg0 = static_cast<unsigned __int64>(Nan::To<int64_t>(info[0]).FromMaybe(0));
          unsigned int arg1 = static_cast<unsigned int>(Nan::To<uint32_t>(info[1]).FromMaybe(0));
          unsigned int arg2 = static_cast<unsigned int>(Nan::To<uint32_t>(info[2]).FromMaybe(0));
          
          winRtInstance = ref new ::Windows::Devices::Midi2::MidiMessage64(arg0,arg1,arg2);
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
      else if (info.Length() == 2
        && info[0]->IsNumber()
        && (NodeRT::Utils::IsWinRtWrapperOf<::Platform::Array<unsigned int>^>(info[1]) || info[1]->IsArray()))
      {
        try {
          unsigned __int64 arg0 = static_cast<unsigned __int64>(Nan::To<int64_t>(info[0]).FromMaybe(0));
          ::Platform::Array<unsigned int>^ arg1 = 
            [] (v8::Local<v8::Value> value) -> ::Platform::Array<unsigned int>^
            {
              if (value->IsArray())
              {
                return NodeRT::Collections::JsArrayToWinrtArray<unsigned int>(value.As<Array>(), 
                 [](Local<Value> value) -> bool {
                   return value->IsUint32();
                 },
                 [](Local<Value> value) -> unsigned int {
                   return static_cast<unsigned int>(Nan::To<uint32_t>(value).FromMaybe(0));
                 }
                );
              }
              else
              {
                return dynamic_cast<::Platform::Array<unsigned int>^>(NodeRT::Utils::GetObjectInstance(value));
              }
            } (info[1]);
          
          winRtInstance = ref new ::Windows::Devices::Midi2::MidiMessage64(arg0,arg1);
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no suitable constructor found")));
        return;
      }

      NodeRT::Utils::SetHiddenValue(info.This(), Nan::New<String>("__winRtInstance__").ToLocalChecked(), True());

      MidiMessage64 *wrapperInstance = new MidiMessage64(winRtInstance);
      wrapperInstance->Wrap(info.This());

      info.GetReturnValue().Set(info.This());
    }


      
    static void CastFrom(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;
      if (info.Length() < 1 || !NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessage64^>(info[0])) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no object provided, or given object could not be casted to requested type")));
        return;
      }

      ::Windows::Devices::Midi2::MidiMessage64^ winRtInstance;
      try {
        winRtInstance = (::Windows::Devices::Midi2::MidiMessage64^) NodeRT::Utils::GetObjectInstance(info[0]);
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }

      info.GetReturnValue().Set(WrapMidiMessage64(winRtInstance));
    }


    static void PeekFirstWord(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessage64^>(info.This())) {
        return;
      }

      MidiMessage64 *wrapper = MidiMessage64::Unwrap<MidiMessage64>(info.This());

      if (info.Length() == 0)
      {
        try
        {
          unsigned int result;
          result = wrapper->_instance->PeekFirstWord();
          info.GetReturnValue().Set(Nan::New<Integer>(result));
          return;
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }
    static void ToString(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessage64^>(info.This())) {
        return;
      }

      MidiMessage64 *wrapper = MidiMessage64::Unwrap<MidiMessage64>(info.This());

      if (info.Length() == 0)
      {
        try
        {
          Platform::String^ result;
          result = wrapper->_instance->ToString();
          info.GetReturnValue().Set(NodeRT::Utils::NewString(result->Data()));
          return;
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }



    static void Word1Getter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessage64^>(info.This())) {
        return;
      }

      MidiMessage64 *wrapper = MidiMessage64::Unwrap<MidiMessage64>(info.This());

      try  {
        unsigned int result = wrapper->_instance->Word1;
        info.GetReturnValue().Set(Nan::New<Integer>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void Word1Setter(Local<String> property, Local<Value> value, const Nan::PropertyCallbackInfo<void> &info) {
      HandleScope scope;

      if (!value->IsUint32()) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Value to set is of unexpected type")));
        return;
      }

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessage64^>(info.This())) {
        return;
      }

      MidiMessage64 *wrapper = MidiMessage64::Unwrap<MidiMessage64>(info.This());

      try {

        unsigned int winRtValue = static_cast<unsigned int>(Nan::To<uint32_t>(value).FromMaybe(0));

        wrapper->_instance->Word1 = winRtValue;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
      }
    }
      
    static void Word0Getter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessage64^>(info.This())) {
        return;
      }

      MidiMessage64 *wrapper = MidiMessage64::Unwrap<MidiMessage64>(info.This());

      try  {
        unsigned int result = wrapper->_instance->Word0;
        info.GetReturnValue().Set(Nan::New<Integer>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void Word0Setter(Local<String> property, Local<Value> value, const Nan::PropertyCallbackInfo<void> &info) {
      HandleScope scope;

      if (!value->IsUint32()) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Value to set is of unexpected type")));
        return;
      }

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessage64^>(info.This())) {
        return;
      }

      MidiMessage64 *wrapper = MidiMessage64::Unwrap<MidiMessage64>(info.This());

      try {

        unsigned int winRtValue = static_cast<unsigned int>(Nan::To<uint32_t>(value).FromMaybe(0));

        wrapper->_instance->Word0 = winRtValue;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
      }
    }
      
    static void TimestampGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessage64^>(info.This())) {
        return;
      }

      MidiMessage64 *wrapper = MidiMessage64::Unwrap<MidiMessage64>(info.This());

      try  {
        unsigned __int64 result = wrapper->_instance->Timestamp;
        info.GetReturnValue().Set(Nan::New<Number>(static_cast<double>(result)));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void TimestampSetter(Local<String> property, Local<Value> value, const Nan::PropertyCallbackInfo<void> &info) {
      HandleScope scope;

      if (!value->IsNumber()) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Value to set is of unexpected type")));
        return;
      }

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessage64^>(info.This())) {
        return;
      }

      MidiMessage64 *wrapper = MidiMessage64::Unwrap<MidiMessage64>(info.This());

      try {

        unsigned __int64 winRtValue = static_cast<unsigned __int64>(Nan::To<int64_t>(value).FromMaybe(0));

        wrapper->_instance->Timestamp = winRtValue;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
      }
    }
      
    static void MessageTypeGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessage64^>(info.This())) {
        return;
      }

      MidiMessage64 *wrapper = MidiMessage64::Unwrap<MidiMessage64>(info.This());

      try  {
        ::Windows::Devices::Midi2::MidiMessageType result = wrapper->_instance->MessageType;
        info.GetReturnValue().Set(Nan::New<Integer>(static_cast<int>(result)));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void MessageTypeSetter(Local<String> property, Local<Value> value, const Nan::PropertyCallbackInfo<void> &info) {
      HandleScope scope;

      if (!value->IsInt32()) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Value to set is of unexpected type")));
        return;
      }

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessage64^>(info.This())) {
        return;
      }

      MidiMessage64 *wrapper = MidiMessage64::Unwrap<MidiMessage64>(info.This());

      try {

        ::Windows::Devices::Midi2::MidiMessageType winRtValue = static_cast<::Windows::Devices::Midi2::MidiMessageType>(Nan::To<int32_t>(value).FromMaybe(0));

        wrapper->_instance->MessageType = winRtValue;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
      }
    }
      
    static void PacketTypeGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessage64^>(info.This())) {
        return;
      }

      MidiMessage64 *wrapper = MidiMessage64::Unwrap<MidiMessage64>(info.This());

      try  {
        ::Windows::Devices::Midi2::MidiPacketType result = wrapper->_instance->PacketType;
        info.GetReturnValue().Set(Nan::New<Integer>(static_cast<int>(result)));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      


    private:
      ::Windows::Devices::Midi2::MidiMessage64^ _instance;
      static Persistent<FunctionTemplate> s_constructorTemplate;

      friend v8::Local<v8::Value> WrapMidiMessage64(::Windows::Devices::Midi2::MidiMessage64^ wintRtInstance);
      friend ::Windows::Devices::Midi2::MidiMessage64^ UnwrapMidiMessage64(Local<Value> value);
  };

  Persistent<FunctionTemplate> MidiMessage64::s_constructorTemplate;

  v8::Local<v8::Value> WrapMidiMessage64(::Windows::Devices::Midi2::MidiMessage64^ winRtInstance) {
    EscapableHandleScope scope;

    if (winRtInstance == nullptr) {
      return scope.Escape(Undefined());
    }

    Local<Value> opaqueWrapper = CreateOpaqueWrapper(winRtInstance);
    Local<Value> args[] = {opaqueWrapper};
    Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(MidiMessage64::s_constructorTemplate);
    return scope.Escape(Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(),_countof(args), args).ToLocalChecked());
  }

  ::Windows::Devices::Midi2::MidiMessage64^ UnwrapMidiMessage64(Local<Value> value) {
     return MidiMessage64::Unwrap<MidiMessage64>(Nan::To<Object>(value).ToLocalChecked())->_instance;
  }

  void InitMidiMessage64(Local<Object> exports) {
    MidiMessage64::Init(exports);
  }

  class MidiMessage96 : public WrapperBase {
    public:
      
      static void Init(const Local<Object> exports) {
        HandleScope scope;

        Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(New);
        s_constructorTemplate.Reset(localRef);
        localRef->SetClassName(Nan::New<String>("MidiMessage96").ToLocalChecked());
        localRef->InstanceTemplate()->SetInternalFieldCount(1);


          
            Nan::SetPrototypeMethod(localRef, "peekFirstWord", PeekFirstWord);
            Nan::SetPrototypeMethod(localRef, "toString", ToString);
          



          
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("word2").ToLocalChecked(), Word2Getter, Word2Setter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("word1").ToLocalChecked(), Word1Getter, Word1Setter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("word0").ToLocalChecked(), Word0Getter, Word0Setter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("timestamp").ToLocalChecked(), TimestampGetter, TimestampSetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("messageType").ToLocalChecked(), MessageTypeGetter, MessageTypeSetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("packetType").ToLocalChecked(), PacketTypeGetter);

        Local<Object> constructor = Nan::To<Object>(Nan::GetFunction(localRef).ToLocalChecked()).ToLocalChecked();
        Nan::SetMethod(constructor, "castFrom", CastFrom);



        Nan::Set(exports, Nan::New<String>("MidiMessage96").ToLocalChecked(), constructor);
      }

      virtual ::Platform::Object^ GetObjectInstance() const override {
        return _instance;
      }

    private:

      MidiMessage96(::Windows::Devices::Midi2::MidiMessage96^ instance) {
        _instance = instance;
      }

      
    static void New(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(s_constructorTemplate);

      // in case the constructor was called without the new operator
      if (!localRef->HasInstance(info.This())) {
        if (info.Length() > 0) {
          std::unique_ptr<Local<Value> []> constructorArgs(new Local<Value>[info.Length()]);

          Local<Value> *argsPtr = constructorArgs.get();
          for (int i = 0; i < info.Length(); i++) {
            argsPtr[i] = info[i];
          }

          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), constructorArgs.get());
          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        } else {
          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), nullptr);

          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        }
      }

      ::Windows::Devices::Midi2::MidiMessage96^ winRtInstance;


      if (info.Length() == 1 && OpaqueWrapper::IsOpaqueWrapper(info[0]) &&
        NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessage96^>(info[0])) {
        try {
          winRtInstance = (::Windows::Devices::Midi2::MidiMessage96^) NodeRT::Utils::GetObjectInstance(info[0]);
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
      else if (info.Length() == 0)
      {
        try {
          winRtInstance = ref new ::Windows::Devices::Midi2::MidiMessage96();
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
      else if (info.Length() == 4
        && info[0]->IsNumber()
        && info[1]->IsUint32()
        && info[2]->IsUint32()
        && info[3]->IsUint32())
      {
        try {
          unsigned __int64 arg0 = static_cast<unsigned __int64>(Nan::To<int64_t>(info[0]).FromMaybe(0));
          unsigned int arg1 = static_cast<unsigned int>(Nan::To<uint32_t>(info[1]).FromMaybe(0));
          unsigned int arg2 = static_cast<unsigned int>(Nan::To<uint32_t>(info[2]).FromMaybe(0));
          unsigned int arg3 = static_cast<unsigned int>(Nan::To<uint32_t>(info[3]).FromMaybe(0));
          
          winRtInstance = ref new ::Windows::Devices::Midi2::MidiMessage96(arg0,arg1,arg2,arg3);
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
      else if (info.Length() == 2
        && info[0]->IsNumber()
        && (NodeRT::Utils::IsWinRtWrapperOf<::Platform::Array<unsigned int>^>(info[1]) || info[1]->IsArray()))
      {
        try {
          unsigned __int64 arg0 = static_cast<unsigned __int64>(Nan::To<int64_t>(info[0]).FromMaybe(0));
          ::Platform::Array<unsigned int>^ arg1 = 
            [] (v8::Local<v8::Value> value) -> ::Platform::Array<unsigned int>^
            {
              if (value->IsArray())
              {
                return NodeRT::Collections::JsArrayToWinrtArray<unsigned int>(value.As<Array>(), 
                 [](Local<Value> value) -> bool {
                   return value->IsUint32();
                 },
                 [](Local<Value> value) -> unsigned int {
                   return static_cast<unsigned int>(Nan::To<uint32_t>(value).FromMaybe(0));
                 }
                );
              }
              else
              {
                return dynamic_cast<::Platform::Array<unsigned int>^>(NodeRT::Utils::GetObjectInstance(value));
              }
            } (info[1]);
          
          winRtInstance = ref new ::Windows::Devices::Midi2::MidiMessage96(arg0,arg1);
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no suitable constructor found")));
        return;
      }

      NodeRT::Utils::SetHiddenValue(info.This(), Nan::New<String>("__winRtInstance__").ToLocalChecked(), True());

      MidiMessage96 *wrapperInstance = new MidiMessage96(winRtInstance);
      wrapperInstance->Wrap(info.This());

      info.GetReturnValue().Set(info.This());
    }


      
    static void CastFrom(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;
      if (info.Length() < 1 || !NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessage96^>(info[0])) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no object provided, or given object could not be casted to requested type")));
        return;
      }

      ::Windows::Devices::Midi2::MidiMessage96^ winRtInstance;
      try {
        winRtInstance = (::Windows::Devices::Midi2::MidiMessage96^) NodeRT::Utils::GetObjectInstance(info[0]);
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }

      info.GetReturnValue().Set(WrapMidiMessage96(winRtInstance));
    }


    static void PeekFirstWord(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessage96^>(info.This())) {
        return;
      }

      MidiMessage96 *wrapper = MidiMessage96::Unwrap<MidiMessage96>(info.This());

      if (info.Length() == 0)
      {
        try
        {
          unsigned int result;
          result = wrapper->_instance->PeekFirstWord();
          info.GetReturnValue().Set(Nan::New<Integer>(result));
          return;
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }
    static void ToString(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessage96^>(info.This())) {
        return;
      }

      MidiMessage96 *wrapper = MidiMessage96::Unwrap<MidiMessage96>(info.This());

      if (info.Length() == 0)
      {
        try
        {
          Platform::String^ result;
          result = wrapper->_instance->ToString();
          info.GetReturnValue().Set(NodeRT::Utils::NewString(result->Data()));
          return;
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }



    static void Word2Getter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessage96^>(info.This())) {
        return;
      }

      MidiMessage96 *wrapper = MidiMessage96::Unwrap<MidiMessage96>(info.This());

      try  {
        unsigned int result = wrapper->_instance->Word2;
        info.GetReturnValue().Set(Nan::New<Integer>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void Word2Setter(Local<String> property, Local<Value> value, const Nan::PropertyCallbackInfo<void> &info) {
      HandleScope scope;

      if (!value->IsUint32()) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Value to set is of unexpected type")));
        return;
      }

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessage96^>(info.This())) {
        return;
      }

      MidiMessage96 *wrapper = MidiMessage96::Unwrap<MidiMessage96>(info.This());

      try {

        unsigned int winRtValue = static_cast<unsigned int>(Nan::To<uint32_t>(value).FromMaybe(0));

        wrapper->_instance->Word2 = winRtValue;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
      }
    }
      
    static void Word1Getter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessage96^>(info.This())) {
        return;
      }

      MidiMessage96 *wrapper = MidiMessage96::Unwrap<MidiMessage96>(info.This());

      try  {
        unsigned int result = wrapper->_instance->Word1;
        info.GetReturnValue().Set(Nan::New<Integer>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void Word1Setter(Local<String> property, Local<Value> value, const Nan::PropertyCallbackInfo<void> &info) {
      HandleScope scope;

      if (!value->IsUint32()) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Value to set is of unexpected type")));
        return;
      }

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessage96^>(info.This())) {
        return;
      }

      MidiMessage96 *wrapper = MidiMessage96::Unwrap<MidiMessage96>(info.This());

      try {

        unsigned int winRtValue = static_cast<unsigned int>(Nan::To<uint32_t>(value).FromMaybe(0));

        wrapper->_instance->Word1 = winRtValue;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
      }
    }
      
    static void Word0Getter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessage96^>(info.This())) {
        return;
      }

      MidiMessage96 *wrapper = MidiMessage96::Unwrap<MidiMessage96>(info.This());

      try  {
        unsigned int result = wrapper->_instance->Word0;
        info.GetReturnValue().Set(Nan::New<Integer>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void Word0Setter(Local<String> property, Local<Value> value, const Nan::PropertyCallbackInfo<void> &info) {
      HandleScope scope;

      if (!value->IsUint32()) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Value to set is of unexpected type")));
        return;
      }

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessage96^>(info.This())) {
        return;
      }

      MidiMessage96 *wrapper = MidiMessage96::Unwrap<MidiMessage96>(info.This());

      try {

        unsigned int winRtValue = static_cast<unsigned int>(Nan::To<uint32_t>(value).FromMaybe(0));

        wrapper->_instance->Word0 = winRtValue;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
      }
    }
      
    static void TimestampGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessage96^>(info.This())) {
        return;
      }

      MidiMessage96 *wrapper = MidiMessage96::Unwrap<MidiMessage96>(info.This());

      try  {
        unsigned __int64 result = wrapper->_instance->Timestamp;
        info.GetReturnValue().Set(Nan::New<Number>(static_cast<double>(result)));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void TimestampSetter(Local<String> property, Local<Value> value, const Nan::PropertyCallbackInfo<void> &info) {
      HandleScope scope;

      if (!value->IsNumber()) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Value to set is of unexpected type")));
        return;
      }

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessage96^>(info.This())) {
        return;
      }

      MidiMessage96 *wrapper = MidiMessage96::Unwrap<MidiMessage96>(info.This());

      try {

        unsigned __int64 winRtValue = static_cast<unsigned __int64>(Nan::To<int64_t>(value).FromMaybe(0));

        wrapper->_instance->Timestamp = winRtValue;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
      }
    }
      
    static void MessageTypeGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessage96^>(info.This())) {
        return;
      }

      MidiMessage96 *wrapper = MidiMessage96::Unwrap<MidiMessage96>(info.This());

      try  {
        ::Windows::Devices::Midi2::MidiMessageType result = wrapper->_instance->MessageType;
        info.GetReturnValue().Set(Nan::New<Integer>(static_cast<int>(result)));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void MessageTypeSetter(Local<String> property, Local<Value> value, const Nan::PropertyCallbackInfo<void> &info) {
      HandleScope scope;

      if (!value->IsInt32()) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Value to set is of unexpected type")));
        return;
      }

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessage96^>(info.This())) {
        return;
      }

      MidiMessage96 *wrapper = MidiMessage96::Unwrap<MidiMessage96>(info.This());

      try {

        ::Windows::Devices::Midi2::MidiMessageType winRtValue = static_cast<::Windows::Devices::Midi2::MidiMessageType>(Nan::To<int32_t>(value).FromMaybe(0));

        wrapper->_instance->MessageType = winRtValue;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
      }
    }
      
    static void PacketTypeGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessage96^>(info.This())) {
        return;
      }

      MidiMessage96 *wrapper = MidiMessage96::Unwrap<MidiMessage96>(info.This());

      try  {
        ::Windows::Devices::Midi2::MidiPacketType result = wrapper->_instance->PacketType;
        info.GetReturnValue().Set(Nan::New<Integer>(static_cast<int>(result)));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      


    private:
      ::Windows::Devices::Midi2::MidiMessage96^ _instance;
      static Persistent<FunctionTemplate> s_constructorTemplate;

      friend v8::Local<v8::Value> WrapMidiMessage96(::Windows::Devices::Midi2::MidiMessage96^ wintRtInstance);
      friend ::Windows::Devices::Midi2::MidiMessage96^ UnwrapMidiMessage96(Local<Value> value);
  };

  Persistent<FunctionTemplate> MidiMessage96::s_constructorTemplate;

  v8::Local<v8::Value> WrapMidiMessage96(::Windows::Devices::Midi2::MidiMessage96^ winRtInstance) {
    EscapableHandleScope scope;

    if (winRtInstance == nullptr) {
      return scope.Escape(Undefined());
    }

    Local<Value> opaqueWrapper = CreateOpaqueWrapper(winRtInstance);
    Local<Value> args[] = {opaqueWrapper};
    Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(MidiMessage96::s_constructorTemplate);
    return scope.Escape(Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(),_countof(args), args).ToLocalChecked());
  }

  ::Windows::Devices::Midi2::MidiMessage96^ UnwrapMidiMessage96(Local<Value> value) {
     return MidiMessage96::Unwrap<MidiMessage96>(Nan::To<Object>(value).ToLocalChecked())->_instance;
  }

  void InitMidiMessage96(Local<Object> exports) {
    MidiMessage96::Init(exports);
  }

  class MidiMessageBuilder : public WrapperBase {
    public:
      
      static void Init(const Local<Object> exports) {
        HandleScope scope;

        Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(New);
        s_constructorTemplate.Reset(localRef);
        localRef->SetClassName(Nan::New<String>("MidiMessageBuilder").ToLocalChecked());
        localRef->InstanceTemplate()->SetInternalFieldCount(1);






        Local<Object> constructor = Nan::To<Object>(Nan::GetFunction(localRef).ToLocalChecked()).ToLocalChecked();
        Nan::SetMethod(constructor, "castFrom", CastFrom);

        Nan::SetMethod(constructor, "buildUtilityMessage", BuildUtilityMessage);
        Nan::SetMethod(constructor, "buildSystemMessage", BuildSystemMessage);
        Nan::SetMethod(constructor, "buildMidi1ChannelVoiceMessage", BuildMidi1ChannelVoiceMessage);
        Nan::SetMethod(constructor, "buildSystemExclusive7Message", BuildSystemExclusive7Message);
        Nan::SetMethod(constructor, "buildMidi2ChannelVoiceMessage", BuildMidi2ChannelVoiceMessage);
        Nan::SetMethod(constructor, "buildSystemExclusive8Message", BuildSystemExclusive8Message);
        Nan::SetMethod(constructor, "buildMixedDataSetChunkHeaderMessage", BuildMixedDataSetChunkHeaderMessage);
        Nan::SetMethod(constructor, "buildMixedDataSetChunkDataMessage", BuildMixedDataSetChunkDataMessage);
        Nan::SetMethod(constructor, "buildFlexDataMessage", BuildFlexDataMessage);
        Nan::SetMethod(constructor, "buildStreamMessage", BuildStreamMessage);


        Nan::Set(exports, Nan::New<String>("MidiMessageBuilder").ToLocalChecked(), constructor);
      }

      virtual ::Platform::Object^ GetObjectInstance() const override {
        return _instance;
      }

    private:

      MidiMessageBuilder(::Windows::Devices::Midi2::MidiMessageBuilder^ instance) {
        _instance = instance;
      }

      
    static void New(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(s_constructorTemplate);

      // in case the constructor was called without the new operator
      if (!localRef->HasInstance(info.This())) {
        if (info.Length() > 0) {
          std::unique_ptr<Local<Value> []> constructorArgs(new Local<Value>[info.Length()]);

          Local<Value> *argsPtr = constructorArgs.get();
          for (int i = 0; i < info.Length(); i++) {
            argsPtr[i] = info[i];
          }

          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), constructorArgs.get());
          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        } else {
          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), nullptr);

          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        }
      }

      ::Windows::Devices::Midi2::MidiMessageBuilder^ winRtInstance;


      if (info.Length() == 1 && OpaqueWrapper::IsOpaqueWrapper(info[0]) &&
        NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessageBuilder^>(info[0])) {
        try {
          winRtInstance = (::Windows::Devices::Midi2::MidiMessageBuilder^) NodeRT::Utils::GetObjectInstance(info[0]);
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no suitable constructor found")));
        return;
      }

      NodeRT::Utils::SetHiddenValue(info.This(), Nan::New<String>("__winRtInstance__").ToLocalChecked(), True());

      MidiMessageBuilder *wrapperInstance = new MidiMessageBuilder(winRtInstance);
      wrapperInstance->Wrap(info.This());

      info.GetReturnValue().Set(info.This());
    }


      
    static void CastFrom(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;
      if (info.Length() < 1 || !NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessageBuilder^>(info[0])) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no object provided, or given object could not be casted to requested type")));
        return;
      }

      ::Windows::Devices::Midi2::MidiMessageBuilder^ winRtInstance;
      try {
        winRtInstance = (::Windows::Devices::Midi2::MidiMessageBuilder^) NodeRT::Utils::GetObjectInstance(info[0]);
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }

      info.GetReturnValue().Set(WrapMidiMessageBuilder(winRtInstance));
    }





    static void BuildUtilityMessage(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 3
        && info[0]->IsNumber()
        && info[1]->IsInt32()
        && info[2]->IsUint32())
      {
        try
        {
          unsigned __int64 arg0 = static_cast<unsigned __int64>(Nan::To<int64_t>(info[0]).FromMaybe(0));
          unsigned char arg1 = static_cast<unsigned char>(Nan::To<int32_t>(info[1]).FromMaybe(0));
          unsigned int arg2 = static_cast<unsigned int>(Nan::To<uint32_t>(info[2]).FromMaybe(0));
          
          ::Windows::Devices::Midi2::MidiMessage32^ result;
          result = ::Windows::Devices::Midi2::MidiMessageBuilder::BuildUtilityMessage(arg0, arg1, arg2);
          info.GetReturnValue().Set(WrapMidiMessage32(result));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void BuildSystemMessage(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 5
        && info[0]->IsNumber()
        && info[1]->IsInt32()
        && info[2]->IsInt32()
        && info[3]->IsInt32()
        && info[4]->IsInt32())
      {
        try
        {
          unsigned __int64 arg0 = static_cast<unsigned __int64>(Nan::To<int64_t>(info[0]).FromMaybe(0));
          unsigned char arg1 = static_cast<unsigned char>(Nan::To<int32_t>(info[1]).FromMaybe(0));
          unsigned char arg2 = static_cast<unsigned char>(Nan::To<int32_t>(info[2]).FromMaybe(0));
          unsigned char arg3 = static_cast<unsigned char>(Nan::To<int32_t>(info[3]).FromMaybe(0));
          unsigned char arg4 = static_cast<unsigned char>(Nan::To<int32_t>(info[4]).FromMaybe(0));
          
          ::Windows::Devices::Midi2::MidiMessage32^ result;
          result = ::Windows::Devices::Midi2::MidiMessageBuilder::BuildSystemMessage(arg0, arg1, arg2, arg3, arg4);
          info.GetReturnValue().Set(WrapMidiMessage32(result));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void BuildMidi1ChannelVoiceMessage(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 6
        && info[0]->IsNumber()
        && info[1]->IsInt32()
        && info[2]->IsInt32()
        && info[3]->IsInt32()
        && info[4]->IsInt32()
        && info[5]->IsInt32())
      {
        try
        {
          unsigned __int64 arg0 = static_cast<unsigned __int64>(Nan::To<int64_t>(info[0]).FromMaybe(0));
          unsigned char arg1 = static_cast<unsigned char>(Nan::To<int32_t>(info[1]).FromMaybe(0));
          ::Windows::Devices::Midi2::Midi1ChannelVoiceMessageStatus arg2 = static_cast<::Windows::Devices::Midi2::Midi1ChannelVoiceMessageStatus>(Nan::To<int32_t>(info[2]).FromMaybe(0));
          unsigned char arg3 = static_cast<unsigned char>(Nan::To<int32_t>(info[3]).FromMaybe(0));
          unsigned char arg4 = static_cast<unsigned char>(Nan::To<int32_t>(info[4]).FromMaybe(0));
          unsigned char arg5 = static_cast<unsigned char>(Nan::To<int32_t>(info[5]).FromMaybe(0));
          
          ::Windows::Devices::Midi2::MidiMessage32^ result;
          result = ::Windows::Devices::Midi2::MidiMessageBuilder::BuildMidi1ChannelVoiceMessage(arg0, arg1, arg2, arg3, arg4, arg5);
          info.GetReturnValue().Set(WrapMidiMessage32(result));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void BuildSystemExclusive7Message(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 10
        && info[0]->IsNumber()
        && info[1]->IsInt32()
        && info[2]->IsInt32()
        && info[3]->IsInt32()
        && info[4]->IsInt32()
        && info[5]->IsInt32()
        && info[6]->IsInt32()
        && info[7]->IsInt32()
        && info[8]->IsInt32()
        && info[9]->IsInt32())
      {
        try
        {
          unsigned __int64 arg0 = static_cast<unsigned __int64>(Nan::To<int64_t>(info[0]).FromMaybe(0));
          unsigned char arg1 = static_cast<unsigned char>(Nan::To<int32_t>(info[1]).FromMaybe(0));
          unsigned char arg2 = static_cast<unsigned char>(Nan::To<int32_t>(info[2]).FromMaybe(0));
          unsigned char arg3 = static_cast<unsigned char>(Nan::To<int32_t>(info[3]).FromMaybe(0));
          unsigned char arg4 = static_cast<unsigned char>(Nan::To<int32_t>(info[4]).FromMaybe(0));
          unsigned char arg5 = static_cast<unsigned char>(Nan::To<int32_t>(info[5]).FromMaybe(0));
          unsigned char arg6 = static_cast<unsigned char>(Nan::To<int32_t>(info[6]).FromMaybe(0));
          unsigned char arg7 = static_cast<unsigned char>(Nan::To<int32_t>(info[7]).FromMaybe(0));
          unsigned char arg8 = static_cast<unsigned char>(Nan::To<int32_t>(info[8]).FromMaybe(0));
          unsigned char arg9 = static_cast<unsigned char>(Nan::To<int32_t>(info[9]).FromMaybe(0));
          
          ::Windows::Devices::Midi2::MidiMessage64^ result;
          result = ::Windows::Devices::Midi2::MidiMessageBuilder::BuildSystemExclusive7Message(arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9);
          info.GetReturnValue().Set(WrapMidiMessage64(result));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void BuildMidi2ChannelVoiceMessage(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 6
        && info[0]->IsNumber()
        && info[1]->IsInt32()
        && info[2]->IsInt32()
        && info[3]->IsInt32()
        && info[4]->IsInt32()
        && info[5]->IsUint32())
      {
        try
        {
          unsigned __int64 arg0 = static_cast<unsigned __int64>(Nan::To<int64_t>(info[0]).FromMaybe(0));
          unsigned char arg1 = static_cast<unsigned char>(Nan::To<int32_t>(info[1]).FromMaybe(0));
          ::Windows::Devices::Midi2::Midi2ChannelVoiceMessageStatus arg2 = static_cast<::Windows::Devices::Midi2::Midi2ChannelVoiceMessageStatus>(Nan::To<int32_t>(info[2]).FromMaybe(0));
          unsigned char arg3 = static_cast<unsigned char>(Nan::To<int32_t>(info[3]).FromMaybe(0));
          unsigned short arg4 = static_cast<unsigned short>(Nan::To<int32_t>(info[4]).FromMaybe(0));
          unsigned int arg5 = static_cast<unsigned int>(Nan::To<uint32_t>(info[5]).FromMaybe(0));
          
          ::Windows::Devices::Midi2::MidiMessage64^ result;
          result = ::Windows::Devices::Midi2::MidiMessageBuilder::BuildMidi2ChannelVoiceMessage(arg0, arg1, arg2, arg3, arg4, arg5);
          info.GetReturnValue().Set(WrapMidiMessage64(result));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void BuildSystemExclusive8Message(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 18
        && info[0]->IsNumber()
        && info[1]->IsInt32()
        && info[2]->IsInt32()
        && info[3]->IsInt32()
        && info[4]->IsInt32()
        && info[5]->IsInt32()
        && info[6]->IsInt32()
        && info[7]->IsInt32()
        && info[8]->IsInt32()
        && info[9]->IsInt32()
        && info[10]->IsInt32()
        && info[11]->IsInt32()
        && info[12]->IsInt32()
        && info[13]->IsInt32()
        && info[14]->IsInt32()
        && info[15]->IsInt32()
        && info[16]->IsInt32()
        && info[17]->IsInt32())
      {
        try
        {
          unsigned __int64 arg0 = static_cast<unsigned __int64>(Nan::To<int64_t>(info[0]).FromMaybe(0));
          unsigned char arg1 = static_cast<unsigned char>(Nan::To<int32_t>(info[1]).FromMaybe(0));
          ::Windows::Devices::Midi2::MidiSystemExclusive8Status arg2 = static_cast<::Windows::Devices::Midi2::MidiSystemExclusive8Status>(Nan::To<int32_t>(info[2]).FromMaybe(0));
          unsigned char arg3 = static_cast<unsigned char>(Nan::To<int32_t>(info[3]).FromMaybe(0));
          unsigned char arg4 = static_cast<unsigned char>(Nan::To<int32_t>(info[4]).FromMaybe(0));
          unsigned char arg5 = static_cast<unsigned char>(Nan::To<int32_t>(info[5]).FromMaybe(0));
          unsigned char arg6 = static_cast<unsigned char>(Nan::To<int32_t>(info[6]).FromMaybe(0));
          unsigned char arg7 = static_cast<unsigned char>(Nan::To<int32_t>(info[7]).FromMaybe(0));
          unsigned char arg8 = static_cast<unsigned char>(Nan::To<int32_t>(info[8]).FromMaybe(0));
          unsigned char arg9 = static_cast<unsigned char>(Nan::To<int32_t>(info[9]).FromMaybe(0));
          unsigned char arg10 = static_cast<unsigned char>(Nan::To<int32_t>(info[10]).FromMaybe(0));
          unsigned char arg11 = static_cast<unsigned char>(Nan::To<int32_t>(info[11]).FromMaybe(0));
          unsigned char arg12 = static_cast<unsigned char>(Nan::To<int32_t>(info[12]).FromMaybe(0));
          unsigned char arg13 = static_cast<unsigned char>(Nan::To<int32_t>(info[13]).FromMaybe(0));
          unsigned char arg14 = static_cast<unsigned char>(Nan::To<int32_t>(info[14]).FromMaybe(0));
          unsigned char arg15 = static_cast<unsigned char>(Nan::To<int32_t>(info[15]).FromMaybe(0));
          unsigned char arg16 = static_cast<unsigned char>(Nan::To<int32_t>(info[16]).FromMaybe(0));
          unsigned char arg17 = static_cast<unsigned char>(Nan::To<int32_t>(info[17]).FromMaybe(0));
          
          ::Windows::Devices::Midi2::MidiMessage128^ result;
          result = ::Windows::Devices::Midi2::MidiMessageBuilder::BuildSystemExclusive8Message(arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11, arg12, arg13, arg14, arg15, arg16, arg17);
          info.GetReturnValue().Set(WrapMidiMessage128(result));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void BuildMixedDataSetChunkHeaderMessage(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 10
        && info[0]->IsNumber()
        && info[1]->IsInt32()
        && info[2]->IsInt32()
        && info[3]->IsInt32()
        && info[4]->IsInt32()
        && info[5]->IsInt32()
        && info[6]->IsInt32()
        && info[7]->IsInt32()
        && info[8]->IsInt32()
        && info[9]->IsInt32())
      {
        try
        {
          unsigned __int64 arg0 = static_cast<unsigned __int64>(Nan::To<int64_t>(info[0]).FromMaybe(0));
          unsigned char arg1 = static_cast<unsigned char>(Nan::To<int32_t>(info[1]).FromMaybe(0));
          unsigned char arg2 = static_cast<unsigned char>(Nan::To<int32_t>(info[2]).FromMaybe(0));
          unsigned short arg3 = static_cast<unsigned short>(Nan::To<int32_t>(info[3]).FromMaybe(0));
          unsigned short arg4 = static_cast<unsigned short>(Nan::To<int32_t>(info[4]).FromMaybe(0));
          unsigned short arg5 = static_cast<unsigned short>(Nan::To<int32_t>(info[5]).FromMaybe(0));
          unsigned short arg6 = static_cast<unsigned short>(Nan::To<int32_t>(info[6]).FromMaybe(0));
          unsigned short arg7 = static_cast<unsigned short>(Nan::To<int32_t>(info[7]).FromMaybe(0));
          unsigned short arg8 = static_cast<unsigned short>(Nan::To<int32_t>(info[8]).FromMaybe(0));
          unsigned short arg9 = static_cast<unsigned short>(Nan::To<int32_t>(info[9]).FromMaybe(0));
          
          ::Windows::Devices::Midi2::MidiMessage128^ result;
          result = ::Windows::Devices::Midi2::MidiMessageBuilder::BuildMixedDataSetChunkHeaderMessage(arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9);
          info.GetReturnValue().Set(WrapMidiMessage128(result));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void BuildMixedDataSetChunkDataMessage(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 17
        && info[0]->IsNumber()
        && info[1]->IsInt32()
        && info[2]->IsInt32()
        && info[3]->IsInt32()
        && info[4]->IsInt32()
        && info[5]->IsInt32()
        && info[6]->IsInt32()
        && info[7]->IsInt32()
        && info[8]->IsInt32()
        && info[9]->IsInt32()
        && info[10]->IsInt32()
        && info[11]->IsInt32()
        && info[12]->IsInt32()
        && info[13]->IsInt32()
        && info[14]->IsInt32()
        && info[15]->IsInt32()
        && info[16]->IsInt32())
      {
        try
        {
          unsigned __int64 arg0 = static_cast<unsigned __int64>(Nan::To<int64_t>(info[0]).FromMaybe(0));
          unsigned char arg1 = static_cast<unsigned char>(Nan::To<int32_t>(info[1]).FromMaybe(0));
          unsigned char arg2 = static_cast<unsigned char>(Nan::To<int32_t>(info[2]).FromMaybe(0));
          unsigned char arg3 = static_cast<unsigned char>(Nan::To<int32_t>(info[3]).FromMaybe(0));
          unsigned char arg4 = static_cast<unsigned char>(Nan::To<int32_t>(info[4]).FromMaybe(0));
          unsigned char arg5 = static_cast<unsigned char>(Nan::To<int32_t>(info[5]).FromMaybe(0));
          unsigned char arg6 = static_cast<unsigned char>(Nan::To<int32_t>(info[6]).FromMaybe(0));
          unsigned char arg7 = static_cast<unsigned char>(Nan::To<int32_t>(info[7]).FromMaybe(0));
          unsigned char arg8 = static_cast<unsigned char>(Nan::To<int32_t>(info[8]).FromMaybe(0));
          unsigned char arg9 = static_cast<unsigned char>(Nan::To<int32_t>(info[9]).FromMaybe(0));
          unsigned char arg10 = static_cast<unsigned char>(Nan::To<int32_t>(info[10]).FromMaybe(0));
          unsigned char arg11 = static_cast<unsigned char>(Nan::To<int32_t>(info[11]).FromMaybe(0));
          unsigned char arg12 = static_cast<unsigned char>(Nan::To<int32_t>(info[12]).FromMaybe(0));
          unsigned char arg13 = static_cast<unsigned char>(Nan::To<int32_t>(info[13]).FromMaybe(0));
          unsigned char arg14 = static_cast<unsigned char>(Nan::To<int32_t>(info[14]).FromMaybe(0));
          unsigned char arg15 = static_cast<unsigned char>(Nan::To<int32_t>(info[15]).FromMaybe(0));
          unsigned char arg16 = static_cast<unsigned char>(Nan::To<int32_t>(info[16]).FromMaybe(0));
          
          ::Windows::Devices::Midi2::MidiMessage128^ result;
          result = ::Windows::Devices::Midi2::MidiMessageBuilder::BuildMixedDataSetChunkDataMessage(arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11, arg12, arg13, arg14, arg15, arg16);
          info.GetReturnValue().Set(WrapMidiMessage128(result));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void BuildFlexDataMessage(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 10
        && info[0]->IsNumber()
        && info[1]->IsInt32()
        && info[2]->IsInt32()
        && info[3]->IsInt32()
        && info[4]->IsInt32()
        && info[5]->IsInt32()
        && info[6]->IsInt32()
        && info[7]->IsUint32()
        && info[8]->IsUint32()
        && info[9]->IsUint32())
      {
        try
        {
          unsigned __int64 arg0 = static_cast<unsigned __int64>(Nan::To<int64_t>(info[0]).FromMaybe(0));
          unsigned char arg1 = static_cast<unsigned char>(Nan::To<int32_t>(info[1]).FromMaybe(0));
          unsigned char arg2 = static_cast<unsigned char>(Nan::To<int32_t>(info[2]).FromMaybe(0));
          unsigned char arg3 = static_cast<unsigned char>(Nan::To<int32_t>(info[3]).FromMaybe(0));
          unsigned char arg4 = static_cast<unsigned char>(Nan::To<int32_t>(info[4]).FromMaybe(0));
          unsigned char arg5 = static_cast<unsigned char>(Nan::To<int32_t>(info[5]).FromMaybe(0));
          unsigned char arg6 = static_cast<unsigned char>(Nan::To<int32_t>(info[6]).FromMaybe(0));
          unsigned int arg7 = static_cast<unsigned int>(Nan::To<uint32_t>(info[7]).FromMaybe(0));
          unsigned int arg8 = static_cast<unsigned int>(Nan::To<uint32_t>(info[8]).FromMaybe(0));
          unsigned int arg9 = static_cast<unsigned int>(Nan::To<uint32_t>(info[9]).FromMaybe(0));
          
          ::Windows::Devices::Midi2::MidiMessage128^ result;
          result = ::Windows::Devices::Midi2::MidiMessageBuilder::BuildFlexDataMessage(arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9);
          info.GetReturnValue().Set(WrapMidiMessage128(result));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void BuildStreamMessage(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 7
        && info[0]->IsNumber()
        && info[1]->IsInt32()
        && info[2]->IsInt32()
        && info[3]->IsInt32()
        && info[4]->IsUint32()
        && info[5]->IsUint32()
        && info[6]->IsUint32())
      {
        try
        {
          unsigned __int64 arg0 = static_cast<unsigned __int64>(Nan::To<int64_t>(info[0]).FromMaybe(0));
          unsigned char arg1 = static_cast<unsigned char>(Nan::To<int32_t>(info[1]).FromMaybe(0));
          unsigned short arg2 = static_cast<unsigned short>(Nan::To<int32_t>(info[2]).FromMaybe(0));
          unsigned short arg3 = static_cast<unsigned short>(Nan::To<int32_t>(info[3]).FromMaybe(0));
          unsigned int arg4 = static_cast<unsigned int>(Nan::To<uint32_t>(info[4]).FromMaybe(0));
          unsigned int arg5 = static_cast<unsigned int>(Nan::To<uint32_t>(info[5]).FromMaybe(0));
          unsigned int arg6 = static_cast<unsigned int>(Nan::To<uint32_t>(info[6]).FromMaybe(0));
          
          ::Windows::Devices::Midi2::MidiMessage128^ result;
          result = ::Windows::Devices::Midi2::MidiMessageBuilder::BuildStreamMessage(arg0, arg1, arg2, arg3, arg4, arg5, arg6);
          info.GetReturnValue().Set(WrapMidiMessage128(result));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }



    private:
      ::Windows::Devices::Midi2::MidiMessageBuilder^ _instance;
      static Persistent<FunctionTemplate> s_constructorTemplate;

      friend v8::Local<v8::Value> WrapMidiMessageBuilder(::Windows::Devices::Midi2::MidiMessageBuilder^ wintRtInstance);
      friend ::Windows::Devices::Midi2::MidiMessageBuilder^ UnwrapMidiMessageBuilder(Local<Value> value);
  };

  Persistent<FunctionTemplate> MidiMessageBuilder::s_constructorTemplate;

  v8::Local<v8::Value> WrapMidiMessageBuilder(::Windows::Devices::Midi2::MidiMessageBuilder^ winRtInstance) {
    EscapableHandleScope scope;

    if (winRtInstance == nullptr) {
      return scope.Escape(Undefined());
    }

    Local<Value> opaqueWrapper = CreateOpaqueWrapper(winRtInstance);
    Local<Value> args[] = {opaqueWrapper};
    Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(MidiMessageBuilder::s_constructorTemplate);
    return scope.Escape(Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(),_countof(args), args).ToLocalChecked());
  }

  ::Windows::Devices::Midi2::MidiMessageBuilder^ UnwrapMidiMessageBuilder(Local<Value> value) {
     return MidiMessageBuilder::Unwrap<MidiMessageBuilder>(Nan::To<Object>(value).ToLocalChecked())->_instance;
  }

  void InitMidiMessageBuilder(Local<Object> exports) {
    MidiMessageBuilder::Init(exports);
  }

  class MidiMessageConverter : public WrapperBase {
    public:
      
      static void Init(const Local<Object> exports) {
        HandleScope scope;

        Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(New);
        s_constructorTemplate.Reset(localRef);
        localRef->SetClassName(Nan::New<String>("MidiMessageConverter").ToLocalChecked());
        localRef->InstanceTemplate()->SetInternalFieldCount(1);






        Local<Object> constructor = Nan::To<Object>(Nan::GetFunction(localRef).ToLocalChecked()).ToLocalChecked();
        Nan::SetMethod(constructor, "castFrom", CastFrom);

        Nan::SetMethod(constructor, "convertMidi1Message", ConvertMidi1Message);
        Nan::SetMethod(constructor, "convertMidi1ChannelPressureMessage", ConvertMidi1ChannelPressureMessage);
        Nan::SetMethod(constructor, "convertMidi1ContinueMessage", ConvertMidi1ContinueMessage);
        Nan::SetMethod(constructor, "convertMidi1NoteOffMessage", ConvertMidi1NoteOffMessage);
        Nan::SetMethod(constructor, "convertMidi1NoteOnMessage", ConvertMidi1NoteOnMessage);
        Nan::SetMethod(constructor, "convertMidi1PitchBendChangeMessage", ConvertMidi1PitchBendChangeMessage);
        Nan::SetMethod(constructor, "convertMidi1PolyphonicKeyPressureMessage", ConvertMidi1PolyphonicKeyPressureMessage);
        Nan::SetMethod(constructor, "convertMidi1ProgramChangeMessage", ConvertMidi1ProgramChangeMessage);
        Nan::SetMethod(constructor, "convertMidi1SongPositionPointerMessage", ConvertMidi1SongPositionPointerMessage);
        Nan::SetMethod(constructor, "convertMidi1SongSelectMessage", ConvertMidi1SongSelectMessage);
        Nan::SetMethod(constructor, "convertMidi1StartMessage", ConvertMidi1StartMessage);
        Nan::SetMethod(constructor, "convertMidi1StopMessage", ConvertMidi1StopMessage);
        Nan::SetMethod(constructor, "convertMidi1SystemResetMessage", ConvertMidi1SystemResetMessage);
        Nan::SetMethod(constructor, "convertMidi1TimeCodeMessage", ConvertMidi1TimeCodeMessage);
        Nan::SetMethod(constructor, "convertMidi1TimingClockMessage", ConvertMidi1TimingClockMessage);
        Nan::SetMethod(constructor, "convertMidi1TuneRequestMessage", ConvertMidi1TuneRequestMessage);


        Nan::Set(exports, Nan::New<String>("MidiMessageConverter").ToLocalChecked(), constructor);
      }

      virtual ::Platform::Object^ GetObjectInstance() const override {
        return _instance;
      }

    private:

      MidiMessageConverter(::Windows::Devices::Midi2::MidiMessageConverter^ instance) {
        _instance = instance;
      }

      
    static void New(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(s_constructorTemplate);

      // in case the constructor was called without the new operator
      if (!localRef->HasInstance(info.This())) {
        if (info.Length() > 0) {
          std::unique_ptr<Local<Value> []> constructorArgs(new Local<Value>[info.Length()]);

          Local<Value> *argsPtr = constructorArgs.get();
          for (int i = 0; i < info.Length(); i++) {
            argsPtr[i] = info[i];
          }

          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), constructorArgs.get());
          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        } else {
          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), nullptr);

          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        }
      }

      ::Windows::Devices::Midi2::MidiMessageConverter^ winRtInstance;


      if (info.Length() == 1 && OpaqueWrapper::IsOpaqueWrapper(info[0]) &&
        NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessageConverter^>(info[0])) {
        try {
          winRtInstance = (::Windows::Devices::Midi2::MidiMessageConverter^) NodeRT::Utils::GetObjectInstance(info[0]);
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no suitable constructor found")));
        return;
      }

      NodeRT::Utils::SetHiddenValue(info.This(), Nan::New<String>("__winRtInstance__").ToLocalChecked(), True());

      MidiMessageConverter *wrapperInstance = new MidiMessageConverter(winRtInstance);
      wrapperInstance->Wrap(info.This());

      info.GetReturnValue().Set(info.This());
    }


      
    static void CastFrom(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;
      if (info.Length() < 1 || !NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessageConverter^>(info[0])) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no object provided, or given object could not be casted to requested type")));
        return;
      }

      ::Windows::Devices::Midi2::MidiMessageConverter^ winRtInstance;
      try {
        winRtInstance = (::Windows::Devices::Midi2::MidiMessageConverter^) NodeRT::Utils::GetObjectInstance(info[0]);
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }

      info.GetReturnValue().Set(WrapMidiMessageConverter(winRtInstance));
    }





    static void ConvertMidi1Message(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 3
        && info[0]->IsNumber()
        && info[1]->IsInt32()
        && info[2]->IsInt32())
      {
        try
        {
          unsigned __int64 arg0 = static_cast<unsigned __int64>(Nan::To<int64_t>(info[0]).FromMaybe(0));
          unsigned char arg1 = static_cast<unsigned char>(Nan::To<int32_t>(info[1]).FromMaybe(0));
          unsigned char arg2 = static_cast<unsigned char>(Nan::To<int32_t>(info[2]).FromMaybe(0));
          
          ::Windows::Devices::Midi2::MidiMessage32^ result;
          result = ::Windows::Devices::Midi2::MidiMessageConverter::ConvertMidi1Message(arg0, arg1, arg2);
          info.GetReturnValue().Set(WrapMidiMessage32(result));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
      else if (info.Length() == 4
        && info[0]->IsNumber()
        && info[1]->IsInt32()
        && info[2]->IsInt32()
        && info[3]->IsInt32())
      {
        try
        {
          unsigned __int64 arg0 = static_cast<unsigned __int64>(Nan::To<int64_t>(info[0]).FromMaybe(0));
          unsigned char arg1 = static_cast<unsigned char>(Nan::To<int32_t>(info[1]).FromMaybe(0));
          unsigned char arg2 = static_cast<unsigned char>(Nan::To<int32_t>(info[2]).FromMaybe(0));
          unsigned char arg3 = static_cast<unsigned char>(Nan::To<int32_t>(info[3]).FromMaybe(0));
          
          ::Windows::Devices::Midi2::MidiMessage32^ result;
          result = ::Windows::Devices::Midi2::MidiMessageConverter::ConvertMidi1Message(arg0, arg1, arg2, arg3);
          info.GetReturnValue().Set(WrapMidiMessage32(result));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
      else if (info.Length() == 5
        && info[0]->IsNumber()
        && info[1]->IsInt32()
        && info[2]->IsInt32()
        && info[3]->IsInt32()
        && info[4]->IsInt32())
      {
        try
        {
          unsigned __int64 arg0 = static_cast<unsigned __int64>(Nan::To<int64_t>(info[0]).FromMaybe(0));
          unsigned char arg1 = static_cast<unsigned char>(Nan::To<int32_t>(info[1]).FromMaybe(0));
          unsigned char arg2 = static_cast<unsigned char>(Nan::To<int32_t>(info[2]).FromMaybe(0));
          unsigned char arg3 = static_cast<unsigned char>(Nan::To<int32_t>(info[3]).FromMaybe(0));
          unsigned char arg4 = static_cast<unsigned char>(Nan::To<int32_t>(info[4]).FromMaybe(0));
          
          ::Windows::Devices::Midi2::MidiMessage32^ result;
          result = ::Windows::Devices::Midi2::MidiMessageConverter::ConvertMidi1Message(arg0, arg1, arg2, arg3, arg4);
          info.GetReturnValue().Set(WrapMidiMessage32(result));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void ConvertMidi1ChannelPressureMessage(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 3
        && info[0]->IsNumber()
        && info[1]->IsInt32()
        && NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi::MidiChannelPressureMessage^>(info[2]))
      {
        try
        {
          unsigned __int64 arg0 = static_cast<unsigned __int64>(Nan::To<int64_t>(info[0]).FromMaybe(0));
          unsigned char arg1 = static_cast<unsigned char>(Nan::To<int32_t>(info[1]).FromMaybe(0));
          ::Windows::Devices::Midi::MidiChannelPressureMessage^ arg2 = dynamic_cast<::Windows::Devices::Midi::MidiChannelPressureMessage^>(NodeRT::Utils::GetObjectInstance(info[2]));
          
          ::Windows::Devices::Midi2::MidiMessage32^ result;
          result = ::Windows::Devices::Midi2::MidiMessageConverter::ConvertMidi1ChannelPressureMessage(arg0, arg1, arg2);
          info.GetReturnValue().Set(WrapMidiMessage32(result));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void ConvertMidi1ContinueMessage(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 3
        && info[0]->IsNumber()
        && info[1]->IsInt32()
        && NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi::MidiContinueMessage^>(info[2]))
      {
        try
        {
          unsigned __int64 arg0 = static_cast<unsigned __int64>(Nan::To<int64_t>(info[0]).FromMaybe(0));
          unsigned char arg1 = static_cast<unsigned char>(Nan::To<int32_t>(info[1]).FromMaybe(0));
          ::Windows::Devices::Midi::MidiContinueMessage^ arg2 = dynamic_cast<::Windows::Devices::Midi::MidiContinueMessage^>(NodeRT::Utils::GetObjectInstance(info[2]));
          
          ::Windows::Devices::Midi2::MidiMessage32^ result;
          result = ::Windows::Devices::Midi2::MidiMessageConverter::ConvertMidi1ContinueMessage(arg0, arg1, arg2);
          info.GetReturnValue().Set(WrapMidiMessage32(result));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void ConvertMidi1NoteOffMessage(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 3
        && info[0]->IsNumber()
        && info[1]->IsInt32()
        && NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi::MidiNoteOffMessage^>(info[2]))
      {
        try
        {
          unsigned __int64 arg0 = static_cast<unsigned __int64>(Nan::To<int64_t>(info[0]).FromMaybe(0));
          unsigned char arg1 = static_cast<unsigned char>(Nan::To<int32_t>(info[1]).FromMaybe(0));
          ::Windows::Devices::Midi::MidiNoteOffMessage^ arg2 = dynamic_cast<::Windows::Devices::Midi::MidiNoteOffMessage^>(NodeRT::Utils::GetObjectInstance(info[2]));
          
          ::Windows::Devices::Midi2::MidiMessage32^ result;
          result = ::Windows::Devices::Midi2::MidiMessageConverter::ConvertMidi1NoteOffMessage(arg0, arg1, arg2);
          info.GetReturnValue().Set(WrapMidiMessage32(result));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void ConvertMidi1NoteOnMessage(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 3
        && info[0]->IsNumber()
        && info[1]->IsInt32()
        && NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi::MidiNoteOnMessage^>(info[2]))
      {
        try
        {
          unsigned __int64 arg0 = static_cast<unsigned __int64>(Nan::To<int64_t>(info[0]).FromMaybe(0));
          unsigned char arg1 = static_cast<unsigned char>(Nan::To<int32_t>(info[1]).FromMaybe(0));
          ::Windows::Devices::Midi::MidiNoteOnMessage^ arg2 = dynamic_cast<::Windows::Devices::Midi::MidiNoteOnMessage^>(NodeRT::Utils::GetObjectInstance(info[2]));
          
          ::Windows::Devices::Midi2::MidiMessage32^ result;
          result = ::Windows::Devices::Midi2::MidiMessageConverter::ConvertMidi1NoteOnMessage(arg0, arg1, arg2);
          info.GetReturnValue().Set(WrapMidiMessage32(result));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void ConvertMidi1PitchBendChangeMessage(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 3
        && info[0]->IsNumber()
        && info[1]->IsInt32()
        && NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi::MidiPitchBendChangeMessage^>(info[2]))
      {
        try
        {
          unsigned __int64 arg0 = static_cast<unsigned __int64>(Nan::To<int64_t>(info[0]).FromMaybe(0));
          unsigned char arg1 = static_cast<unsigned char>(Nan::To<int32_t>(info[1]).FromMaybe(0));
          ::Windows::Devices::Midi::MidiPitchBendChangeMessage^ arg2 = dynamic_cast<::Windows::Devices::Midi::MidiPitchBendChangeMessage^>(NodeRT::Utils::GetObjectInstance(info[2]));
          
          ::Windows::Devices::Midi2::MidiMessage32^ result;
          result = ::Windows::Devices::Midi2::MidiMessageConverter::ConvertMidi1PitchBendChangeMessage(arg0, arg1, arg2);
          info.GetReturnValue().Set(WrapMidiMessage32(result));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void ConvertMidi1PolyphonicKeyPressureMessage(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 3
        && info[0]->IsNumber()
        && info[1]->IsInt32()
        && NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi::MidiPolyphonicKeyPressureMessage^>(info[2]))
      {
        try
        {
          unsigned __int64 arg0 = static_cast<unsigned __int64>(Nan::To<int64_t>(info[0]).FromMaybe(0));
          unsigned char arg1 = static_cast<unsigned char>(Nan::To<int32_t>(info[1]).FromMaybe(0));
          ::Windows::Devices::Midi::MidiPolyphonicKeyPressureMessage^ arg2 = dynamic_cast<::Windows::Devices::Midi::MidiPolyphonicKeyPressureMessage^>(NodeRT::Utils::GetObjectInstance(info[2]));
          
          ::Windows::Devices::Midi2::MidiMessage32^ result;
          result = ::Windows::Devices::Midi2::MidiMessageConverter::ConvertMidi1PolyphonicKeyPressureMessage(arg0, arg1, arg2);
          info.GetReturnValue().Set(WrapMidiMessage32(result));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void ConvertMidi1ProgramChangeMessage(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 3
        && info[0]->IsNumber()
        && info[1]->IsInt32()
        && NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi::MidiProgramChangeMessage^>(info[2]))
      {
        try
        {
          unsigned __int64 arg0 = static_cast<unsigned __int64>(Nan::To<int64_t>(info[0]).FromMaybe(0));
          unsigned char arg1 = static_cast<unsigned char>(Nan::To<int32_t>(info[1]).FromMaybe(0));
          ::Windows::Devices::Midi::MidiProgramChangeMessage^ arg2 = dynamic_cast<::Windows::Devices::Midi::MidiProgramChangeMessage^>(NodeRT::Utils::GetObjectInstance(info[2]));
          
          ::Windows::Devices::Midi2::MidiMessage32^ result;
          result = ::Windows::Devices::Midi2::MidiMessageConverter::ConvertMidi1ProgramChangeMessage(arg0, arg1, arg2);
          info.GetReturnValue().Set(WrapMidiMessage32(result));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void ConvertMidi1SongPositionPointerMessage(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 3
        && info[0]->IsNumber()
        && info[1]->IsInt32()
        && NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi::MidiSongPositionPointerMessage^>(info[2]))
      {
        try
        {
          unsigned __int64 arg0 = static_cast<unsigned __int64>(Nan::To<int64_t>(info[0]).FromMaybe(0));
          unsigned char arg1 = static_cast<unsigned char>(Nan::To<int32_t>(info[1]).FromMaybe(0));
          ::Windows::Devices::Midi::MidiSongPositionPointerMessage^ arg2 = dynamic_cast<::Windows::Devices::Midi::MidiSongPositionPointerMessage^>(NodeRT::Utils::GetObjectInstance(info[2]));
          
          ::Windows::Devices::Midi2::MidiMessage32^ result;
          result = ::Windows::Devices::Midi2::MidiMessageConverter::ConvertMidi1SongPositionPointerMessage(arg0, arg1, arg2);
          info.GetReturnValue().Set(WrapMidiMessage32(result));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void ConvertMidi1SongSelectMessage(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 3
        && info[0]->IsNumber()
        && info[1]->IsInt32()
        && NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi::MidiSongSelectMessage^>(info[2]))
      {
        try
        {
          unsigned __int64 arg0 = static_cast<unsigned __int64>(Nan::To<int64_t>(info[0]).FromMaybe(0));
          unsigned char arg1 = static_cast<unsigned char>(Nan::To<int32_t>(info[1]).FromMaybe(0));
          ::Windows::Devices::Midi::MidiSongSelectMessage^ arg2 = dynamic_cast<::Windows::Devices::Midi::MidiSongSelectMessage^>(NodeRT::Utils::GetObjectInstance(info[2]));
          
          ::Windows::Devices::Midi2::MidiMessage32^ result;
          result = ::Windows::Devices::Midi2::MidiMessageConverter::ConvertMidi1SongSelectMessage(arg0, arg1, arg2);
          info.GetReturnValue().Set(WrapMidiMessage32(result));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void ConvertMidi1StartMessage(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 3
        && info[0]->IsNumber()
        && info[1]->IsInt32()
        && NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi::MidiStartMessage^>(info[2]))
      {
        try
        {
          unsigned __int64 arg0 = static_cast<unsigned __int64>(Nan::To<int64_t>(info[0]).FromMaybe(0));
          unsigned char arg1 = static_cast<unsigned char>(Nan::To<int32_t>(info[1]).FromMaybe(0));
          ::Windows::Devices::Midi::MidiStartMessage^ arg2 = dynamic_cast<::Windows::Devices::Midi::MidiStartMessage^>(NodeRT::Utils::GetObjectInstance(info[2]));
          
          ::Windows::Devices::Midi2::MidiMessage32^ result;
          result = ::Windows::Devices::Midi2::MidiMessageConverter::ConvertMidi1StartMessage(arg0, arg1, arg2);
          info.GetReturnValue().Set(WrapMidiMessage32(result));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void ConvertMidi1StopMessage(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 3
        && info[0]->IsNumber()
        && info[1]->IsInt32()
        && NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi::MidiStopMessage^>(info[2]))
      {
        try
        {
          unsigned __int64 arg0 = static_cast<unsigned __int64>(Nan::To<int64_t>(info[0]).FromMaybe(0));
          unsigned char arg1 = static_cast<unsigned char>(Nan::To<int32_t>(info[1]).FromMaybe(0));
          ::Windows::Devices::Midi::MidiStopMessage^ arg2 = dynamic_cast<::Windows::Devices::Midi::MidiStopMessage^>(NodeRT::Utils::GetObjectInstance(info[2]));
          
          ::Windows::Devices::Midi2::MidiMessage32^ result;
          result = ::Windows::Devices::Midi2::MidiMessageConverter::ConvertMidi1StopMessage(arg0, arg1, arg2);
          info.GetReturnValue().Set(WrapMidiMessage32(result));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void ConvertMidi1SystemResetMessage(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 3
        && info[0]->IsNumber()
        && info[1]->IsInt32()
        && NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi::MidiSystemResetMessage^>(info[2]))
      {
        try
        {
          unsigned __int64 arg0 = static_cast<unsigned __int64>(Nan::To<int64_t>(info[0]).FromMaybe(0));
          unsigned char arg1 = static_cast<unsigned char>(Nan::To<int32_t>(info[1]).FromMaybe(0));
          ::Windows::Devices::Midi::MidiSystemResetMessage^ arg2 = dynamic_cast<::Windows::Devices::Midi::MidiSystemResetMessage^>(NodeRT::Utils::GetObjectInstance(info[2]));
          
          ::Windows::Devices::Midi2::MidiMessage32^ result;
          result = ::Windows::Devices::Midi2::MidiMessageConverter::ConvertMidi1SystemResetMessage(arg0, arg1, arg2);
          info.GetReturnValue().Set(WrapMidiMessage32(result));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void ConvertMidi1TimeCodeMessage(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 3
        && info[0]->IsNumber()
        && info[1]->IsInt32()
        && NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi::MidiTimeCodeMessage^>(info[2]))
      {
        try
        {
          unsigned __int64 arg0 = static_cast<unsigned __int64>(Nan::To<int64_t>(info[0]).FromMaybe(0));
          unsigned char arg1 = static_cast<unsigned char>(Nan::To<int32_t>(info[1]).FromMaybe(0));
          ::Windows::Devices::Midi::MidiTimeCodeMessage^ arg2 = dynamic_cast<::Windows::Devices::Midi::MidiTimeCodeMessage^>(NodeRT::Utils::GetObjectInstance(info[2]));
          
          ::Windows::Devices::Midi2::MidiMessage32^ result;
          result = ::Windows::Devices::Midi2::MidiMessageConverter::ConvertMidi1TimeCodeMessage(arg0, arg1, arg2);
          info.GetReturnValue().Set(WrapMidiMessage32(result));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void ConvertMidi1TimingClockMessage(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 3
        && info[0]->IsNumber()
        && info[1]->IsInt32()
        && NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi::MidiTimingClockMessage^>(info[2]))
      {
        try
        {
          unsigned __int64 arg0 = static_cast<unsigned __int64>(Nan::To<int64_t>(info[0]).FromMaybe(0));
          unsigned char arg1 = static_cast<unsigned char>(Nan::To<int32_t>(info[1]).FromMaybe(0));
          ::Windows::Devices::Midi::MidiTimingClockMessage^ arg2 = dynamic_cast<::Windows::Devices::Midi::MidiTimingClockMessage^>(NodeRT::Utils::GetObjectInstance(info[2]));
          
          ::Windows::Devices::Midi2::MidiMessage32^ result;
          result = ::Windows::Devices::Midi2::MidiMessageConverter::ConvertMidi1TimingClockMessage(arg0, arg1, arg2);
          info.GetReturnValue().Set(WrapMidiMessage32(result));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void ConvertMidi1TuneRequestMessage(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 3
        && info[0]->IsNumber()
        && info[1]->IsInt32()
        && NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi::MidiTuneRequestMessage^>(info[2]))
      {
        try
        {
          unsigned __int64 arg0 = static_cast<unsigned __int64>(Nan::To<int64_t>(info[0]).FromMaybe(0));
          unsigned char arg1 = static_cast<unsigned char>(Nan::To<int32_t>(info[1]).FromMaybe(0));
          ::Windows::Devices::Midi::MidiTuneRequestMessage^ arg2 = dynamic_cast<::Windows::Devices::Midi::MidiTuneRequestMessage^>(NodeRT::Utils::GetObjectInstance(info[2]));
          
          ::Windows::Devices::Midi2::MidiMessage32^ result;
          result = ::Windows::Devices::Midi2::MidiMessageConverter::ConvertMidi1TuneRequestMessage(arg0, arg1, arg2);
          info.GetReturnValue().Set(WrapMidiMessage32(result));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }



    private:
      ::Windows::Devices::Midi2::MidiMessageConverter^ _instance;
      static Persistent<FunctionTemplate> s_constructorTemplate;

      friend v8::Local<v8::Value> WrapMidiMessageConverter(::Windows::Devices::Midi2::MidiMessageConverter^ wintRtInstance);
      friend ::Windows::Devices::Midi2::MidiMessageConverter^ UnwrapMidiMessageConverter(Local<Value> value);
  };

  Persistent<FunctionTemplate> MidiMessageConverter::s_constructorTemplate;

  v8::Local<v8::Value> WrapMidiMessageConverter(::Windows::Devices::Midi2::MidiMessageConverter^ winRtInstance) {
    EscapableHandleScope scope;

    if (winRtInstance == nullptr) {
      return scope.Escape(Undefined());
    }

    Local<Value> opaqueWrapper = CreateOpaqueWrapper(winRtInstance);
    Local<Value> args[] = {opaqueWrapper};
    Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(MidiMessageConverter::s_constructorTemplate);
    return scope.Escape(Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(),_countof(args), args).ToLocalChecked());
  }

  ::Windows::Devices::Midi2::MidiMessageConverter^ UnwrapMidiMessageConverter(Local<Value> value) {
     return MidiMessageConverter::Unwrap<MidiMessageConverter>(Nan::To<Object>(value).ToLocalChecked())->_instance;
  }

  void InitMidiMessageConverter(Local<Object> exports) {
    MidiMessageConverter::Init(exports);
  }

  class MidiMessageProcessingPluginInformation : public WrapperBase {
    public:
      
      static void Init(const Local<Object> exports) {
        HandleScope scope;

        Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(New);
        s_constructorTemplate.Reset(localRef);
        localRef->SetClassName(Nan::New<String>("MidiMessageProcessingPluginInformation").ToLocalChecked());
        localRef->InstanceTemplate()->SetInternalFieldCount(1);





          
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("author").ToLocalChecked(), AuthorGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("classId").ToLocalChecked(), ClassIdGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("clientConfigurationAssemblyName").ToLocalChecked(), ClientConfigurationAssemblyNameGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("description").ToLocalChecked(), DescriptionGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("iconPath").ToLocalChecked(), IconPathGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("isClientConfigurable").ToLocalChecked(), IsClientConfigurableGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("isEnabled").ToLocalChecked(), IsEnabledGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("isSystemManaged").ToLocalChecked(), IsSystemManagedGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("name").ToLocalChecked(), NameGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("registryKey").ToLocalChecked(), RegistryKeyGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("servicePluginFileName").ToLocalChecked(), ServicePluginFileNameGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("shortName").ToLocalChecked(), ShortNameGetter);

        Local<Object> constructor = Nan::To<Object>(Nan::GetFunction(localRef).ToLocalChecked()).ToLocalChecked();
        Nan::SetMethod(constructor, "castFrom", CastFrom);



        Nan::Set(exports, Nan::New<String>("MidiMessageProcessingPluginInformation").ToLocalChecked(), constructor);
      }

      virtual ::Platform::Object^ GetObjectInstance() const override {
        return _instance;
      }

    private:

      MidiMessageProcessingPluginInformation(::Windows::Devices::Midi2::MidiMessageProcessingPluginInformation^ instance) {
        _instance = instance;
      }

      
    static void New(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(s_constructorTemplate);

      // in case the constructor was called without the new operator
      if (!localRef->HasInstance(info.This())) {
        if (info.Length() > 0) {
          std::unique_ptr<Local<Value> []> constructorArgs(new Local<Value>[info.Length()]);

          Local<Value> *argsPtr = constructorArgs.get();
          for (int i = 0; i < info.Length(); i++) {
            argsPtr[i] = info[i];
          }

          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), constructorArgs.get());
          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        } else {
          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), nullptr);

          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        }
      }

      ::Windows::Devices::Midi2::MidiMessageProcessingPluginInformation^ winRtInstance;


      if (info.Length() == 1 && OpaqueWrapper::IsOpaqueWrapper(info[0]) &&
        NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessageProcessingPluginInformation^>(info[0])) {
        try {
          winRtInstance = (::Windows::Devices::Midi2::MidiMessageProcessingPluginInformation^) NodeRT::Utils::GetObjectInstance(info[0]);
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no suitable constructor found")));
        return;
      }

      NodeRT::Utils::SetHiddenValue(info.This(), Nan::New<String>("__winRtInstance__").ToLocalChecked(), True());

      MidiMessageProcessingPluginInformation *wrapperInstance = new MidiMessageProcessingPluginInformation(winRtInstance);
      wrapperInstance->Wrap(info.This());

      info.GetReturnValue().Set(info.This());
    }


      
    static void CastFrom(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;
      if (info.Length() < 1 || !NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessageProcessingPluginInformation^>(info[0])) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no object provided, or given object could not be casted to requested type")));
        return;
      }

      ::Windows::Devices::Midi2::MidiMessageProcessingPluginInformation^ winRtInstance;
      try {
        winRtInstance = (::Windows::Devices::Midi2::MidiMessageProcessingPluginInformation^) NodeRT::Utils::GetObjectInstance(info[0]);
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }

      info.GetReturnValue().Set(WrapMidiMessageProcessingPluginInformation(winRtInstance));
    }





    static void AuthorGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessageProcessingPluginInformation^>(info.This())) {
        return;
      }

      MidiMessageProcessingPluginInformation *wrapper = MidiMessageProcessingPluginInformation::Unwrap<MidiMessageProcessingPluginInformation>(info.This());

      try  {
        Platform::String^ result = wrapper->_instance->Author;
        info.GetReturnValue().Set(NodeRT::Utils::NewString(result->Data()));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void ClassIdGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessageProcessingPluginInformation^>(info.This())) {
        return;
      }

      MidiMessageProcessingPluginInformation *wrapper = MidiMessageProcessingPluginInformation::Unwrap<MidiMessageProcessingPluginInformation>(info.This());

      try  {
        ::Platform::Guid result = wrapper->_instance->ClassId;
        info.GetReturnValue().Set(NodeRT::Utils::GuidToJs(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void ClientConfigurationAssemblyNameGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessageProcessingPluginInformation^>(info.This())) {
        return;
      }

      MidiMessageProcessingPluginInformation *wrapper = MidiMessageProcessingPluginInformation::Unwrap<MidiMessageProcessingPluginInformation>(info.This());

      try  {
        Platform::String^ result = wrapper->_instance->ClientConfigurationAssemblyName;
        info.GetReturnValue().Set(NodeRT::Utils::NewString(result->Data()));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void DescriptionGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessageProcessingPluginInformation^>(info.This())) {
        return;
      }

      MidiMessageProcessingPluginInformation *wrapper = MidiMessageProcessingPluginInformation::Unwrap<MidiMessageProcessingPluginInformation>(info.This());

      try  {
        Platform::String^ result = wrapper->_instance->Description;
        info.GetReturnValue().Set(NodeRT::Utils::NewString(result->Data()));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void IconPathGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessageProcessingPluginInformation^>(info.This())) {
        return;
      }

      MidiMessageProcessingPluginInformation *wrapper = MidiMessageProcessingPluginInformation::Unwrap<MidiMessageProcessingPluginInformation>(info.This());

      try  {
        Platform::String^ result = wrapper->_instance->IconPath;
        info.GetReturnValue().Set(NodeRT::Utils::NewString(result->Data()));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void IsClientConfigurableGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessageProcessingPluginInformation^>(info.This())) {
        return;
      }

      MidiMessageProcessingPluginInformation *wrapper = MidiMessageProcessingPluginInformation::Unwrap<MidiMessageProcessingPluginInformation>(info.This());

      try  {
        bool result = wrapper->_instance->IsClientConfigurable;
        info.GetReturnValue().Set(Nan::New<Boolean>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void IsEnabledGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessageProcessingPluginInformation^>(info.This())) {
        return;
      }

      MidiMessageProcessingPluginInformation *wrapper = MidiMessageProcessingPluginInformation::Unwrap<MidiMessageProcessingPluginInformation>(info.This());

      try  {
        bool result = wrapper->_instance->IsEnabled;
        info.GetReturnValue().Set(Nan::New<Boolean>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void IsSystemManagedGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessageProcessingPluginInformation^>(info.This())) {
        return;
      }

      MidiMessageProcessingPluginInformation *wrapper = MidiMessageProcessingPluginInformation::Unwrap<MidiMessageProcessingPluginInformation>(info.This());

      try  {
        bool result = wrapper->_instance->IsSystemManaged;
        info.GetReturnValue().Set(Nan::New<Boolean>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void NameGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessageProcessingPluginInformation^>(info.This())) {
        return;
      }

      MidiMessageProcessingPluginInformation *wrapper = MidiMessageProcessingPluginInformation::Unwrap<MidiMessageProcessingPluginInformation>(info.This());

      try  {
        Platform::String^ result = wrapper->_instance->Name;
        info.GetReturnValue().Set(NodeRT::Utils::NewString(result->Data()));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void RegistryKeyGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessageProcessingPluginInformation^>(info.This())) {
        return;
      }

      MidiMessageProcessingPluginInformation *wrapper = MidiMessageProcessingPluginInformation::Unwrap<MidiMessageProcessingPluginInformation>(info.This());

      try  {
        Platform::String^ result = wrapper->_instance->RegistryKey;
        info.GetReturnValue().Set(NodeRT::Utils::NewString(result->Data()));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void ServicePluginFileNameGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessageProcessingPluginInformation^>(info.This())) {
        return;
      }

      MidiMessageProcessingPluginInformation *wrapper = MidiMessageProcessingPluginInformation::Unwrap<MidiMessageProcessingPluginInformation>(info.This());

      try  {
        Platform::String^ result = wrapper->_instance->ServicePluginFileName;
        info.GetReturnValue().Set(NodeRT::Utils::NewString(result->Data()));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void ShortNameGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessageProcessingPluginInformation^>(info.This())) {
        return;
      }

      MidiMessageProcessingPluginInformation *wrapper = MidiMessageProcessingPluginInformation::Unwrap<MidiMessageProcessingPluginInformation>(info.This());

      try  {
        Platform::String^ result = wrapper->_instance->ShortName;
        info.GetReturnValue().Set(NodeRT::Utils::NewString(result->Data()));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      


    private:
      ::Windows::Devices::Midi2::MidiMessageProcessingPluginInformation^ _instance;
      static Persistent<FunctionTemplate> s_constructorTemplate;

      friend v8::Local<v8::Value> WrapMidiMessageProcessingPluginInformation(::Windows::Devices::Midi2::MidiMessageProcessingPluginInformation^ wintRtInstance);
      friend ::Windows::Devices::Midi2::MidiMessageProcessingPluginInformation^ UnwrapMidiMessageProcessingPluginInformation(Local<Value> value);
  };

  Persistent<FunctionTemplate> MidiMessageProcessingPluginInformation::s_constructorTemplate;

  v8::Local<v8::Value> WrapMidiMessageProcessingPluginInformation(::Windows::Devices::Midi2::MidiMessageProcessingPluginInformation^ winRtInstance) {
    EscapableHandleScope scope;

    if (winRtInstance == nullptr) {
      return scope.Escape(Undefined());
    }

    Local<Value> opaqueWrapper = CreateOpaqueWrapper(winRtInstance);
    Local<Value> args[] = {opaqueWrapper};
    Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(MidiMessageProcessingPluginInformation::s_constructorTemplate);
    return scope.Escape(Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(),_countof(args), args).ToLocalChecked());
  }

  ::Windows::Devices::Midi2::MidiMessageProcessingPluginInformation^ UnwrapMidiMessageProcessingPluginInformation(Local<Value> value) {
     return MidiMessageProcessingPluginInformation::Unwrap<MidiMessageProcessingPluginInformation>(Nan::To<Object>(value).ToLocalChecked())->_instance;
  }

  void InitMidiMessageProcessingPluginInformation(Local<Object> exports) {
    MidiMessageProcessingPluginInformation::Init(exports);
  }

  class MidiMessageReceivedEventArgs : public WrapperBase {
    public:
      
      static void Init(const Local<Object> exports) {
        HandleScope scope;

        Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(New);
        s_constructorTemplate.Reset(localRef);
        localRef->SetClassName(Nan::New<String>("MidiMessageReceivedEventArgs").ToLocalChecked());
        localRef->InstanceTemplate()->SetInternalFieldCount(1);


          
            Nan::SetPrototypeMethod(localRef, "peekFirstWord", PeekFirstWord);
            Nan::SetPrototypeMethod(localRef, "getMessagePacket", GetMessagePacket);
            Nan::SetPrototypeMethod(localRef, "fillWords", FillWords);
            //Nan::SetPrototypeMethod(localRef, "fillMessageStruct", FillMessageStruct);
            Nan::SetPrototypeMethod(localRef, "fillMessage32", FillMessage32);
            Nan::SetPrototypeMethod(localRef, "fillMessage64", FillMessage64);
            Nan::SetPrototypeMethod(localRef, "fillMessage96", FillMessage96);
            Nan::SetPrototypeMethod(localRef, "fillMessage128", FillMessage128);
            Nan::SetPrototypeMethod(localRef, "fillWordArray", FillWordArray);
            Nan::SetPrototypeMethod(localRef, "fillByteArray", FillByteArray);
            Nan::SetPrototypeMethod(localRef, "fillBuffer", FillBuffer);
          



          
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("messageType").ToLocalChecked(), MessageTypeGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("packetType").ToLocalChecked(), PacketTypeGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("timestamp").ToLocalChecked(), TimestampGetter);

        Local<Object> constructor = Nan::To<Object>(Nan::GetFunction(localRef).ToLocalChecked()).ToLocalChecked();
        Nan::SetMethod(constructor, "castFrom", CastFrom);



        Nan::Set(exports, Nan::New<String>("MidiMessageReceivedEventArgs").ToLocalChecked(), constructor);
      }

      virtual ::Platform::Object^ GetObjectInstance() const override {
        return _instance;
      }

    private:

      MidiMessageReceivedEventArgs(::Windows::Devices::Midi2::MidiMessageReceivedEventArgs^ instance) {
        _instance = instance;
      }

      
    static void New(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(s_constructorTemplate);

      // in case the constructor was called without the new operator
      if (!localRef->HasInstance(info.This())) {
        if (info.Length() > 0) {
          std::unique_ptr<Local<Value> []> constructorArgs(new Local<Value>[info.Length()]);

          Local<Value> *argsPtr = constructorArgs.get();
          for (int i = 0; i < info.Length(); i++) {
            argsPtr[i] = info[i];
          }

          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), constructorArgs.get());
          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        } else {
          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), nullptr);

          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        }
      }

      ::Windows::Devices::Midi2::MidiMessageReceivedEventArgs^ winRtInstance;


      if (info.Length() == 1 && OpaqueWrapper::IsOpaqueWrapper(info[0]) &&
        NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessageReceivedEventArgs^>(info[0])) {
        try {
          winRtInstance = (::Windows::Devices::Midi2::MidiMessageReceivedEventArgs^) NodeRT::Utils::GetObjectInstance(info[0]);
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no suitable constructor found")));
        return;
      }

      NodeRT::Utils::SetHiddenValue(info.This(), Nan::New<String>("__winRtInstance__").ToLocalChecked(), True());

      MidiMessageReceivedEventArgs *wrapperInstance = new MidiMessageReceivedEventArgs(winRtInstance);
      wrapperInstance->Wrap(info.This());

      info.GetReturnValue().Set(info.This());
    }


      
    static void CastFrom(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;
      if (info.Length() < 1 || !NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessageReceivedEventArgs^>(info[0])) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no object provided, or given object could not be casted to requested type")));
        return;
      }

      ::Windows::Devices::Midi2::MidiMessageReceivedEventArgs^ winRtInstance;
      try {
        winRtInstance = (::Windows::Devices::Midi2::MidiMessageReceivedEventArgs^) NodeRT::Utils::GetObjectInstance(info[0]);
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }

      info.GetReturnValue().Set(WrapMidiMessageReceivedEventArgs(winRtInstance));
    }


    static void PeekFirstWord(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessageReceivedEventArgs^>(info.This())) {
        return;
      }

      MidiMessageReceivedEventArgs *wrapper = MidiMessageReceivedEventArgs::Unwrap<MidiMessageReceivedEventArgs>(info.This());

      if (info.Length() == 0)
      {
        try
        {
          unsigned int result;
          result = wrapper->_instance->PeekFirstWord();
          info.GetReturnValue().Set(Nan::New<Integer>(result));
          return;
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }
    static void GetMessagePacket(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessageReceivedEventArgs^>(info.This())) {
        return;
      }

      MidiMessageReceivedEventArgs *wrapper = MidiMessageReceivedEventArgs::Unwrap<MidiMessageReceivedEventArgs>(info.This());

      if (info.Length() == 0)
      {
        try
        {
          ::Windows::Devices::Midi2::IMidiUniversalPacket^ result;
          result = wrapper->_instance->GetMessagePacket();
          info.GetReturnValue().Set(WrapIMidiUniversalPacket(result));
          return;
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }
    static void FillWords(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessageReceivedEventArgs^>(info.This())) {
        return;
      }

      MidiMessageReceivedEventArgs *wrapper = MidiMessageReceivedEventArgs::Unwrap<MidiMessageReceivedEventArgs>(info.This());

      if (info.Length() == 0)
      {
        try
        {
          unsigned int arg0;
          unsigned int arg1;
          unsigned int arg2;
          unsigned int arg3;
          
          unsigned char result;
          result = wrapper->_instance->FillWords(&arg0, &arg1, &arg2, &arg3);
          Local<Object> resObj = Nan::New<Object>();
          Nan::Set(resObj, Nan::New<String>("number").ToLocalChecked(), Nan::New<Integer>(result));
          Nan::Set(resObj, Nan::New<String>("word0").ToLocalChecked(), Nan::New<Integer>(arg0));
          Nan::Set(resObj, Nan::New<String>("word1").ToLocalChecked(), Nan::New<Integer>(arg1));
          Nan::Set(resObj, Nan::New<String>("word2").ToLocalChecked(), Nan::New<Integer>(arg2));
          Nan::Set(resObj, Nan::New<String>("word3").ToLocalChecked(), Nan::New<Integer>(arg3));
          info.GetReturnValue().Set(resObj);
          return;
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }
 //   static void FillMessageStruct(Nan::NAN_METHOD_ARGS_TYPE info) {
 //     HandleScope scope;

 //     if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessageReceivedEventArgs^>(info.This())) {
 //       return;
 //     }

 //     MidiMessageReceivedEventArgs *wrapper = MidiMessageReceivedEventArgs::Unwrap<MidiMessageReceivedEventArgs>(info.This());

 //     if (info.Length() == 0)
 //     {
 //       try
 //       {
 //         ::Windows::Devices::Midi2::MidiMessageStruct arg0;
 //         
 //         unsigned char result;
 //         result = wrapper->_instance->FillMessageStruct(&arg0);
 //         Local<Object> resObj = Nan::New<Object>();
 //         Nan::Set(resObj, Nan::New<String>("number").ToLocalChecked(), Nan::New<Integer>(result));
 //         Nan::Set(resObj, Nan::New<String>("message").ToLocalChecked(), MidiMessageStructToJsObject(arg0));
 //         info.GetReturnValue().Set(resObj);
 //         return;
 //       } catch (Platform::Exception ^exception) {
 //         NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
 //         return;
 //       }
 //     }
 //else {
 //       Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
 //       return;
 //     }
 //   }
    static void FillMessage32(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessageReceivedEventArgs^>(info.This())) {
        return;
      }

      MidiMessageReceivedEventArgs *wrapper = MidiMessageReceivedEventArgs::Unwrap<MidiMessageReceivedEventArgs>(info.This());

      if (info.Length() == 1
        && NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessage32^>(info[0]))
      {
        try
        {
          ::Windows::Devices::Midi2::MidiMessage32^ arg0 = UnwrapMidiMessage32(info[0]);
          
          bool result;
          result = wrapper->_instance->FillMessage32(arg0);
          info.GetReturnValue().Set(Nan::New<Boolean>(result));
          return;
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }
    static void FillMessage64(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessageReceivedEventArgs^>(info.This())) {
        return;
      }

      MidiMessageReceivedEventArgs *wrapper = MidiMessageReceivedEventArgs::Unwrap<MidiMessageReceivedEventArgs>(info.This());

      if (info.Length() == 1
        && NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessage64^>(info[0]))
      {
        try
        {
          ::Windows::Devices::Midi2::MidiMessage64^ arg0 = UnwrapMidiMessage64(info[0]);
          
          bool result;
          result = wrapper->_instance->FillMessage64(arg0);
          info.GetReturnValue().Set(Nan::New<Boolean>(result));
          return;
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }
    static void FillMessage96(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessageReceivedEventArgs^>(info.This())) {
        return;
      }

      MidiMessageReceivedEventArgs *wrapper = MidiMessageReceivedEventArgs::Unwrap<MidiMessageReceivedEventArgs>(info.This());

      if (info.Length() == 1
        && NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessage96^>(info[0]))
      {
        try
        {
          ::Windows::Devices::Midi2::MidiMessage96^ arg0 = UnwrapMidiMessage96(info[0]);
          
          bool result;
          result = wrapper->_instance->FillMessage96(arg0);
          info.GetReturnValue().Set(Nan::New<Boolean>(result));
          return;
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }
    static void FillMessage128(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessageReceivedEventArgs^>(info.This())) {
        return;
      }

      MidiMessageReceivedEventArgs *wrapper = MidiMessageReceivedEventArgs::Unwrap<MidiMessageReceivedEventArgs>(info.This());

      if (info.Length() == 1
        && NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessage128^>(info[0]))
      {
        try
        {
          ::Windows::Devices::Midi2::MidiMessage128^ arg0 = UnwrapMidiMessage128(info[0]);
          
          bool result;
          result = wrapper->_instance->FillMessage128(arg0);
          info.GetReturnValue().Set(Nan::New<Boolean>(result));
          return;
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }
    static void FillWordArray(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;
      Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Not implemented")));
    }
    static void FillByteArray(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;
      Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Not implemented")));
    }
    static void FillBuffer(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessageReceivedEventArgs^>(info.This())) {
        return;
      }

      MidiMessageReceivedEventArgs *wrapper = MidiMessageReceivedEventArgs::Unwrap<MidiMessageReceivedEventArgs>(info.This());

      if (info.Length() == 2
        && NodeRT::Utils::IsWinRtWrapperOf<::Windows::Foundation::IMemoryBuffer^>(info[0])
        && info[1]->IsUint32())
      {
        try
        {
          ::Windows::Foundation::IMemoryBuffer^ arg0 = dynamic_cast<::Windows::Foundation::IMemoryBuffer^>(NodeRT::Utils::GetObjectInstance(info[0]));
          unsigned int arg1 = static_cast<unsigned int>(Nan::To<uint32_t>(info[1]).FromMaybe(0));
          
          unsigned char result;
          result = wrapper->_instance->FillBuffer(arg0, arg1);
          info.GetReturnValue().Set(Nan::New<Integer>(result));
          return;
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }



    static void MessageTypeGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessageReceivedEventArgs^>(info.This())) {
        return;
      }

      MidiMessageReceivedEventArgs *wrapper = MidiMessageReceivedEventArgs::Unwrap<MidiMessageReceivedEventArgs>(info.This());

      try  {
        ::Windows::Devices::Midi2::MidiMessageType result = wrapper->_instance->MessageType;
        info.GetReturnValue().Set(Nan::New<Integer>(static_cast<int>(result)));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void PacketTypeGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessageReceivedEventArgs^>(info.This())) {
        return;
      }

      MidiMessageReceivedEventArgs *wrapper = MidiMessageReceivedEventArgs::Unwrap<MidiMessageReceivedEventArgs>(info.This());

      try  {
        ::Windows::Devices::Midi2::MidiPacketType result = wrapper->_instance->PacketType;
        info.GetReturnValue().Set(Nan::New<Integer>(static_cast<int>(result)));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void TimestampGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessageReceivedEventArgs^>(info.This())) {
        return;
      }

      MidiMessageReceivedEventArgs *wrapper = MidiMessageReceivedEventArgs::Unwrap<MidiMessageReceivedEventArgs>(info.This());

      try  {
        unsigned __int64 result = wrapper->_instance->Timestamp;
        info.GetReturnValue().Set(Nan::New<Number>(static_cast<double>(result)));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      


    private:
      ::Windows::Devices::Midi2::MidiMessageReceivedEventArgs^ _instance;
      static Persistent<FunctionTemplate> s_constructorTemplate;

      friend v8::Local<v8::Value> WrapMidiMessageReceivedEventArgs(::Windows::Devices::Midi2::MidiMessageReceivedEventArgs^ wintRtInstance);
      friend ::Windows::Devices::Midi2::MidiMessageReceivedEventArgs^ UnwrapMidiMessageReceivedEventArgs(Local<Value> value);
  };

  Persistent<FunctionTemplate> MidiMessageReceivedEventArgs::s_constructorTemplate;

  v8::Local<v8::Value> WrapMidiMessageReceivedEventArgs(::Windows::Devices::Midi2::MidiMessageReceivedEventArgs^ winRtInstance) {
    EscapableHandleScope scope;

    if (winRtInstance == nullptr) {
      return scope.Escape(Undefined());
    }

    Local<Value> opaqueWrapper = CreateOpaqueWrapper(winRtInstance);
    Local<Value> args[] = {opaqueWrapper};
    Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(MidiMessageReceivedEventArgs::s_constructorTemplate);
    return scope.Escape(Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(),_countof(args), args).ToLocalChecked());
  }

  ::Windows::Devices::Midi2::MidiMessageReceivedEventArgs^ UnwrapMidiMessageReceivedEventArgs(Local<Value> value) {
     return MidiMessageReceivedEventArgs::Unwrap<MidiMessageReceivedEventArgs>(Nan::To<Object>(value).ToLocalChecked())->_instance;
  }

  void InitMidiMessageReceivedEventArgs(Local<Object> exports) {
    MidiMessageReceivedEventArgs::Init(exports);
  }

  class MidiMessageTranslator : public WrapperBase {
    public:
      
      static void Init(const Local<Object> exports) {
        HandleScope scope;

        Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(New);
        s_constructorTemplate.Reset(localRef);
        localRef->SetClassName(Nan::New<String>("MidiMessageTranslator").ToLocalChecked());
        localRef->InstanceTemplate()->SetInternalFieldCount(1);






        Local<Object> constructor = Nan::To<Object>(Nan::GetFunction(localRef).ToLocalChecked()).ToLocalChecked();
        Nan::SetMethod(constructor, "castFrom", CastFrom);

        Nan::SetMethod(constructor, "upscaleMidi1ChannelVoiceMessageToMidi2", UpscaleMidi1ChannelVoiceMessageToMidi2);
        Nan::SetMethod(constructor, "downscaleMidi2ChannelVoiceMessageToMidi1", DownscaleMidi2ChannelVoiceMessageToMidi1);


        Nan::Set(exports, Nan::New<String>("MidiMessageTranslator").ToLocalChecked(), constructor);
      }

      virtual ::Platform::Object^ GetObjectInstance() const override {
        return _instance;
      }

    private:

      MidiMessageTranslator(::Windows::Devices::Midi2::MidiMessageTranslator^ instance) {
        _instance = instance;
      }

      
    static void New(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(s_constructorTemplate);

      // in case the constructor was called without the new operator
      if (!localRef->HasInstance(info.This())) {
        if (info.Length() > 0) {
          std::unique_ptr<Local<Value> []> constructorArgs(new Local<Value>[info.Length()]);

          Local<Value> *argsPtr = constructorArgs.get();
          for (int i = 0; i < info.Length(); i++) {
            argsPtr[i] = info[i];
          }

          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), constructorArgs.get());
          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        } else {
          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), nullptr);

          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        }
      }

      ::Windows::Devices::Midi2::MidiMessageTranslator^ winRtInstance;


      if (info.Length() == 1 && OpaqueWrapper::IsOpaqueWrapper(info[0]) &&
        NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessageTranslator^>(info[0])) {
        try {
          winRtInstance = (::Windows::Devices::Midi2::MidiMessageTranslator^) NodeRT::Utils::GetObjectInstance(info[0]);
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no suitable constructor found")));
        return;
      }

      NodeRT::Utils::SetHiddenValue(info.This(), Nan::New<String>("__winRtInstance__").ToLocalChecked(), True());

      MidiMessageTranslator *wrapperInstance = new MidiMessageTranslator(winRtInstance);
      wrapperInstance->Wrap(info.This());

      info.GetReturnValue().Set(info.This());
    }


      
    static void CastFrom(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;
      if (info.Length() < 1 || !NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessageTranslator^>(info[0])) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no object provided, or given object could not be casted to requested type")));
        return;
      }

      ::Windows::Devices::Midi2::MidiMessageTranslator^ winRtInstance;
      try {
        winRtInstance = (::Windows::Devices::Midi2::MidiMessageTranslator^) NodeRT::Utils::GetObjectInstance(info[0]);
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }

      info.GetReturnValue().Set(WrapMidiMessageTranslator(winRtInstance));
    }





    static void UpscaleMidi1ChannelVoiceMessageToMidi2(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 1
        && NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessage32^>(info[0]))
      {
        try
        {
          ::Windows::Devices::Midi2::MidiMessage32^ arg0 = UnwrapMidiMessage32(info[0]);
          
          ::Windows::Devices::Midi2::MidiMessage64^ result;
          result = ::Windows::Devices::Midi2::MidiMessageTranslator::UpscaleMidi1ChannelVoiceMessageToMidi2(arg0);
          info.GetReturnValue().Set(WrapMidiMessage64(result));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
      else if (info.Length() == 3
        && info[0]->IsNumber()
        && info[1]->IsInt32()
        && info[2]->IsInt32())
      {
        try
        {
          unsigned __int64 arg0 = static_cast<unsigned __int64>(Nan::To<int64_t>(info[0]).FromMaybe(0));
          unsigned char arg1 = static_cast<unsigned char>(Nan::To<int32_t>(info[1]).FromMaybe(0));
          unsigned char arg2 = static_cast<unsigned char>(Nan::To<int32_t>(info[2]).FromMaybe(0));
          
          ::Windows::Devices::Midi2::MidiMessage64^ result;
          result = ::Windows::Devices::Midi2::MidiMessageTranslator::UpscaleMidi1ChannelVoiceMessageToMidi2(arg0, arg1, arg2);
          info.GetReturnValue().Set(WrapMidiMessage64(result));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
      else if (info.Length() == 4
        && info[0]->IsNumber()
        && info[1]->IsInt32()
        && info[2]->IsInt32()
        && info[3]->IsInt32())
      {
        try
        {
          unsigned __int64 arg0 = static_cast<unsigned __int64>(Nan::To<int64_t>(info[0]).FromMaybe(0));
          unsigned char arg1 = static_cast<unsigned char>(Nan::To<int32_t>(info[1]).FromMaybe(0));
          unsigned char arg2 = static_cast<unsigned char>(Nan::To<int32_t>(info[2]).FromMaybe(0));
          unsigned char arg3 = static_cast<unsigned char>(Nan::To<int32_t>(info[3]).FromMaybe(0));
          
          ::Windows::Devices::Midi2::MidiMessage64^ result;
          result = ::Windows::Devices::Midi2::MidiMessageTranslator::UpscaleMidi1ChannelVoiceMessageToMidi2(arg0, arg1, arg2, arg3);
          info.GetReturnValue().Set(WrapMidiMessage64(result));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
      else if (info.Length() == 5
        && info[0]->IsNumber()
        && info[1]->IsInt32()
        && info[2]->IsInt32()
        && info[3]->IsInt32()
        && info[4]->IsInt32())
      {
        try
        {
          unsigned __int64 arg0 = static_cast<unsigned __int64>(Nan::To<int64_t>(info[0]).FromMaybe(0));
          unsigned char arg1 = static_cast<unsigned char>(Nan::To<int32_t>(info[1]).FromMaybe(0));
          unsigned char arg2 = static_cast<unsigned char>(Nan::To<int32_t>(info[2]).FromMaybe(0));
          unsigned char arg3 = static_cast<unsigned char>(Nan::To<int32_t>(info[3]).FromMaybe(0));
          unsigned char arg4 = static_cast<unsigned char>(Nan::To<int32_t>(info[4]).FromMaybe(0));
          
          ::Windows::Devices::Midi2::MidiMessage64^ result;
          result = ::Windows::Devices::Midi2::MidiMessageTranslator::UpscaleMidi1ChannelVoiceMessageToMidi2(arg0, arg1, arg2, arg3, arg4);
          info.GetReturnValue().Set(WrapMidiMessage64(result));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void DownscaleMidi2ChannelVoiceMessageToMidi1(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 1
        && NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessage64^>(info[0]))
      {
        try
        {
          ::Windows::Devices::Midi2::MidiMessage64^ arg0 = UnwrapMidiMessage64(info[0]);
          
          ::Windows::Devices::Midi2::MidiMessage32^ result;
          result = ::Windows::Devices::Midi2::MidiMessageTranslator::DownscaleMidi2ChannelVoiceMessageToMidi1(arg0);
          info.GetReturnValue().Set(WrapMidiMessage32(result));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }



    private:
      ::Windows::Devices::Midi2::MidiMessageTranslator^ _instance;
      static Persistent<FunctionTemplate> s_constructorTemplate;

      friend v8::Local<v8::Value> WrapMidiMessageTranslator(::Windows::Devices::Midi2::MidiMessageTranslator^ wintRtInstance);
      friend ::Windows::Devices::Midi2::MidiMessageTranslator^ UnwrapMidiMessageTranslator(Local<Value> value);
  };

  Persistent<FunctionTemplate> MidiMessageTranslator::s_constructorTemplate;

  v8::Local<v8::Value> WrapMidiMessageTranslator(::Windows::Devices::Midi2::MidiMessageTranslator^ winRtInstance) {
    EscapableHandleScope scope;

    if (winRtInstance == nullptr) {
      return scope.Escape(Undefined());
    }

    Local<Value> opaqueWrapper = CreateOpaqueWrapper(winRtInstance);
    Local<Value> args[] = {opaqueWrapper};
    Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(MidiMessageTranslator::s_constructorTemplate);
    return scope.Escape(Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(),_countof(args), args).ToLocalChecked());
  }

  ::Windows::Devices::Midi2::MidiMessageTranslator^ UnwrapMidiMessageTranslator(Local<Value> value) {
     return MidiMessageTranslator::Unwrap<MidiMessageTranslator>(Nan::To<Object>(value).ToLocalChecked())->_instance;
  }

  void InitMidiMessageTranslator(Local<Object> exports) {
    MidiMessageTranslator::Init(exports);
  }

  class MidiMessageTypeEndpointListener : public WrapperBase {
    public:
      
      static void Init(const Local<Object> exports) {
        HandleScope scope;

        Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(New);
        s_constructorTemplate.Reset(localRef);
        localRef->SetClassName(Nan::New<String>("MidiMessageTypeEndpointListener").ToLocalChecked());
        localRef->InstanceTemplate()->SetInternalFieldCount(1);


          
            Nan::SetPrototypeMethod(localRef, "initialize", Initialize);
            Nan::SetPrototypeMethod(localRef, "onEndpointConnectionOpened", OnEndpointConnectionOpened);
            Nan::SetPrototypeMethod(localRef, "processIncomingMessage", ProcessIncomingMessage);
            Nan::SetPrototypeMethod(localRef, "cleanup", Cleanup);
          


          
          Nan::SetPrototypeMethod(localRef,"addListener", AddListener);
          Nan::SetPrototypeMethod(localRef,"on", AddListener);
          Nan::SetPrototypeMethod(localRef,"removeListener", RemoveListener);
          Nan::SetPrototypeMethod(localRef, "off", RemoveListener);

          
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("tag").ToLocalChecked(), TagGetter, TagSetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("name").ToLocalChecked(), NameGetter, NameSetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("isEnabled").ToLocalChecked(), IsEnabledGetter, IsEnabledSetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("id").ToLocalChecked(), IdGetter, IdSetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("includeMessageTypes").ToLocalChecked(), IncludeMessageTypesGetter);

        Local<Object> constructor = Nan::To<Object>(Nan::GetFunction(localRef).ToLocalChecked()).ToLocalChecked();
        Nan::SetMethod(constructor, "castFrom", CastFrom);



        Nan::Set(exports, Nan::New<String>("MidiMessageTypeEndpointListener").ToLocalChecked(), constructor);
      }

      virtual ::Platform::Object^ GetObjectInstance() const override {
        return _instance;
      }

    private:

      MidiMessageTypeEndpointListener(::Windows::Devices::Midi2::MidiMessageTypeEndpointListener^ instance) {
        _instance = instance;
      }

      
    static void New(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(s_constructorTemplate);

      // in case the constructor was called without the new operator
      if (!localRef->HasInstance(info.This())) {
        if (info.Length() > 0) {
          std::unique_ptr<Local<Value> []> constructorArgs(new Local<Value>[info.Length()]);

          Local<Value> *argsPtr = constructorArgs.get();
          for (int i = 0; i < info.Length(); i++) {
            argsPtr[i] = info[i];
          }

          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), constructorArgs.get());
          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        } else {
          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), nullptr);

          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        }
      }

      ::Windows::Devices::Midi2::MidiMessageTypeEndpointListener^ winRtInstance;


      if (info.Length() == 1 && OpaqueWrapper::IsOpaqueWrapper(info[0]) &&
        NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessageTypeEndpointListener^>(info[0])) {
        try {
          winRtInstance = (::Windows::Devices::Midi2::MidiMessageTypeEndpointListener^) NodeRT::Utils::GetObjectInstance(info[0]);
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
      else if (info.Length() == 0)
      {
        try {
          winRtInstance = ref new ::Windows::Devices::Midi2::MidiMessageTypeEndpointListener();
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no suitable constructor found")));
        return;
      }

      NodeRT::Utils::SetHiddenValue(info.This(), Nan::New<String>("__winRtInstance__").ToLocalChecked(), True());

      MidiMessageTypeEndpointListener *wrapperInstance = new MidiMessageTypeEndpointListener(winRtInstance);
      wrapperInstance->Wrap(info.This());

      info.GetReturnValue().Set(info.This());
    }


      
    static void CastFrom(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;
      if (info.Length() < 1 || !NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessageTypeEndpointListener^>(info[0])) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no object provided, or given object could not be casted to requested type")));
        return;
      }

      ::Windows::Devices::Midi2::MidiMessageTypeEndpointListener^ winRtInstance;
      try {
        winRtInstance = (::Windows::Devices::Midi2::MidiMessageTypeEndpointListener^) NodeRT::Utils::GetObjectInstance(info[0]);
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }

      info.GetReturnValue().Set(WrapMidiMessageTypeEndpointListener(winRtInstance));
    }


    static void Initialize(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessageTypeEndpointListener^>(info.This())) {
        return;
      }

      MidiMessageTypeEndpointListener *wrapper = MidiMessageTypeEndpointListener::Unwrap<MidiMessageTypeEndpointListener>(info.This());

      if (info.Length() == 1
        && NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::IMidiEndpointConnectionSource^>(info[0]))
      {
        try
        {
          ::Windows::Devices::Midi2::IMidiEndpointConnectionSource^ arg0 = UnwrapIMidiEndpointConnectionSource(info[0]);
          
          wrapper->_instance->Initialize(arg0);
          return;
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }
    static void OnEndpointConnectionOpened(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessageTypeEndpointListener^>(info.This())) {
        return;
      }

      MidiMessageTypeEndpointListener *wrapper = MidiMessageTypeEndpointListener::Unwrap<MidiMessageTypeEndpointListener>(info.This());

      if (info.Length() == 0)
      {
        try
        {
          wrapper->_instance->OnEndpointConnectionOpened();
          return;
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }
    static void ProcessIncomingMessage(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessageTypeEndpointListener^>(info.This())) {
        return;
      }

      MidiMessageTypeEndpointListener *wrapper = MidiMessageTypeEndpointListener::Unwrap<MidiMessageTypeEndpointListener>(info.This());

      if (info.Length() == 1
        && NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessageReceivedEventArgs^>(info[0]))
      {
        try
        {
          ::Windows::Devices::Midi2::MidiMessageReceivedEventArgs^ arg0 = UnwrapMidiMessageReceivedEventArgs(info[0]);
          bool arg1;
          bool arg2;
          
          wrapper->_instance->ProcessIncomingMessage(arg0, &arg1, &arg2);
          Local<Object> resObj = Nan::New<Object>();
          Nan::Set(resObj, Nan::New<String>("skipFurtherListeners").ToLocalChecked(), Nan::New<Boolean>(arg1));
          Nan::Set(resObj, Nan::New<String>("skipMainMessageReceivedEvent").ToLocalChecked(), Nan::New<Boolean>(arg2));
          info.GetReturnValue().Set(resObj);
          return;
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }
    static void Cleanup(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessageTypeEndpointListener^>(info.This())) {
        return;
      }

      MidiMessageTypeEndpointListener *wrapper = MidiMessageTypeEndpointListener::Unwrap<MidiMessageTypeEndpointListener>(info.This());

      if (info.Length() == 0)
      {
        try
        {
          wrapper->_instance->Cleanup();
          return;
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }



    static void TagGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessageTypeEndpointListener^>(info.This())) {
        return;
      }

      MidiMessageTypeEndpointListener *wrapper = MidiMessageTypeEndpointListener::Unwrap<MidiMessageTypeEndpointListener>(info.This());

      try  {
        ::Platform::Object^ result = wrapper->_instance->Tag;
        info.GetReturnValue().Set(CreateOpaqueWrapper(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void TagSetter(Local<String> property, Local<Value> value, const Nan::PropertyCallbackInfo<void> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Platform::Object^>(value)) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Value to set is of unexpected type")));
        return;
      }

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessageTypeEndpointListener^>(info.This())) {
        return;
      }

      MidiMessageTypeEndpointListener *wrapper = MidiMessageTypeEndpointListener::Unwrap<MidiMessageTypeEndpointListener>(info.This());

      try {

        ::Platform::Object^ winRtValue = dynamic_cast<::Platform::Object^>(NodeRT::Utils::GetObjectInstance(value));

        wrapper->_instance->Tag = winRtValue;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
      }
    }
      
    static void NameGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessageTypeEndpointListener^>(info.This())) {
        return;
      }

      MidiMessageTypeEndpointListener *wrapper = MidiMessageTypeEndpointListener::Unwrap<MidiMessageTypeEndpointListener>(info.This());

      try  {
        Platform::String^ result = wrapper->_instance->Name;
        info.GetReturnValue().Set(NodeRT::Utils::NewString(result->Data()));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void NameSetter(Local<String> property, Local<Value> value, const Nan::PropertyCallbackInfo<void> &info) {
      HandleScope scope;

      if (!value->IsString()) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Value to set is of unexpected type")));
        return;
      }

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessageTypeEndpointListener^>(info.This())) {
        return;
      }

      MidiMessageTypeEndpointListener *wrapper = MidiMessageTypeEndpointListener::Unwrap<MidiMessageTypeEndpointListener>(info.This());

      try {

        Platform::String^ winRtValue = ref new Platform::String(NodeRT::Utils::StringToWchar(v8::String::Value(v8::Isolate::GetCurrent(), value)));

        wrapper->_instance->Name = winRtValue;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
      }
    }
      
    static void IsEnabledGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessageTypeEndpointListener^>(info.This())) {
        return;
      }

      MidiMessageTypeEndpointListener *wrapper = MidiMessageTypeEndpointListener::Unwrap<MidiMessageTypeEndpointListener>(info.This());

      try  {
        bool result = wrapper->_instance->IsEnabled;
        info.GetReturnValue().Set(Nan::New<Boolean>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void IsEnabledSetter(Local<String> property, Local<Value> value, const Nan::PropertyCallbackInfo<void> &info) {
      HandleScope scope;

      if (!value->IsBoolean()) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Value to set is of unexpected type")));
        return;
      }

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessageTypeEndpointListener^>(info.This())) {
        return;
      }

      MidiMessageTypeEndpointListener *wrapper = MidiMessageTypeEndpointListener::Unwrap<MidiMessageTypeEndpointListener>(info.This());

      try {

        bool winRtValue = Nan::To<bool>(value).FromMaybe(false);

        wrapper->_instance->IsEnabled = winRtValue;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
      }
    }
      
    static void IdGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessageTypeEndpointListener^>(info.This())) {
        return;
      }

      MidiMessageTypeEndpointListener *wrapper = MidiMessageTypeEndpointListener::Unwrap<MidiMessageTypeEndpointListener>(info.This());

      try  {
        Platform::String^ result = wrapper->_instance->Id;
        info.GetReturnValue().Set(NodeRT::Utils::NewString(result->Data()));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void IdSetter(Local<String> property, Local<Value> value, const Nan::PropertyCallbackInfo<void> &info) {
      HandleScope scope;

      if (!value->IsString()) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Value to set is of unexpected type")));
        return;
      }

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessageTypeEndpointListener^>(info.This())) {
        return;
      }

      MidiMessageTypeEndpointListener *wrapper = MidiMessageTypeEndpointListener::Unwrap<MidiMessageTypeEndpointListener>(info.This());

      try {

        Platform::String^ winRtValue = ref new Platform::String(NodeRT::Utils::StringToWchar(v8::String::Value(v8::Isolate::GetCurrent(), value)));

        wrapper->_instance->Id = winRtValue;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
      }
    }
      
    static void IncludeMessageTypesGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessageTypeEndpointListener^>(info.This())) {
        return;
      }

      MidiMessageTypeEndpointListener *wrapper = MidiMessageTypeEndpointListener::Unwrap<MidiMessageTypeEndpointListener>(info.This());

      try  {
        ::Windows::Foundation::Collections::IVector<::Windows::Devices::Midi2::MidiMessageType>^ result = wrapper->_instance->IncludeMessageTypes;
        info.GetReturnValue().Set(NodeRT::Collections::VectorWrapper<::Windows::Devices::Midi2::MidiMessageType>::CreateVectorWrapper(result, 
            [](::Windows::Devices::Midi2::MidiMessageType val) -> Local<Value> {
              return Nan::New<Integer>(static_cast<int>(val));
            },
            [](Local<Value> value) -> bool {
              return value->IsInt32();
            },
            [](Local<Value> value) -> ::Windows::Devices::Midi2::MidiMessageType {
              return static_cast<::Windows::Devices::Midi2::MidiMessageType>(Nan::To<int32_t>(value).FromMaybe(0));
            }
          ));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      


    static void AddListener(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() < 2 || !info[0]->IsString() || !info[1]->IsFunction()) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"wrong arguments, expected arguments are eventName(string),callback(function)")));
        return;
      }

      String::Value eventName(v8::Isolate::GetCurrent(), info[0]);
      auto str = *eventName;

      Local<Function> callback = info[1].As<Function>();

      ::Windows::Foundation::EventRegistrationToken registrationToken;
      if (NodeRT::Utils::CaseInsenstiveEquals(L"messageReceived", str))
      {
        if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessageTypeEndpointListener^>(info.This()))
        {
          Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"The caller of this method isn't of the expected type or internal WinRt object was disposed")));
      return;
        }
        MidiMessageTypeEndpointListener *wrapper = MidiMessageTypeEndpointListener::Unwrap<MidiMessageTypeEndpointListener>(info.This());
      
        try {
          Persistent<Object>* perstPtr = new Persistent<Object>();
          perstPtr->Reset(NodeRT::Utils::CreateCallbackObjectInDomain(callback));
          std::shared_ptr<Persistent<Object>> callbackObjPtr(perstPtr,
            [] (Persistent<Object> *ptr ) {
              NodeUtils::Async::RunOnMain([ptr]() {
                ptr->Reset();
                delete ptr;
            });
          });

          registrationToken = wrapper->_instance->MessageReceived::add(
            ref new ::Windows::Foundation::TypedEventHandler<::Windows::Devices::Midi2::IMidiMessageReceivedEventSource^, ::Windows::Devices::Midi2::MidiMessageReceivedEventArgs^>(
            [callbackObjPtr](::Windows::Devices::Midi2::IMidiMessageReceivedEventSource^ arg0, ::Windows::Devices::Midi2::MidiMessageReceivedEventArgs^ arg1) {
              NodeUtils::Async::RunOnMain([callbackObjPtr , arg0, arg1]() {
                HandleScope scope;


                Local<Value> wrappedArg0;
                Local<Value> wrappedArg1;

                {
                  TryCatch tryCatch;


                  wrappedArg0 = WrapIMidiMessageReceivedEventSource(arg0);
                  wrappedArg1 = WrapMidiMessageReceivedEventArgs(arg1);


                  if (wrappedArg0.IsEmpty()) wrappedArg0 = Undefined();
                  if (wrappedArg1.IsEmpty()) wrappedArg1 = Undefined();
                }

                Local<Value> args[] = { wrappedArg0, wrappedArg1 };
                Local<Object> callbackObjLocalRef = Nan::New<Object>(*callbackObjPtr);
                NodeRT::Utils::CallCallbackInDomain(callbackObjLocalRef, _countof(args), args);
              });
            })
          );
        }
        catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }

      }
 else  {
        Nan::ThrowError(Nan::Error(String::Concat(v8::Isolate::GetCurrent(), NodeRT::Utils::NewString(L"given event name isn't supported: "), info[0].As<String>())));
        return;
      }

      Local<Value> tokenMapVal = NodeRT::Utils::GetHiddenValue(callback, Nan::New<String>(REGISTRATION_TOKEN_MAP_PROPERTY_NAME).ToLocalChecked());
      Local<Object> tokenMap;

      if (tokenMapVal.IsEmpty() || Nan::Equals(tokenMapVal, Undefined()).FromMaybe(false)) {
        tokenMap = Nan::New<Object>();
        NodeRT::Utils::SetHiddenValueWithObject(callback, Nan::New<String>(REGISTRATION_TOKEN_MAP_PROPERTY_NAME).ToLocalChecked(), tokenMap);
      } else {
        tokenMap = Nan::To<Object>(tokenMapVal).ToLocalChecked();
      }

      Nan::Set(tokenMap, info[0], CreateOpaqueWrapper(::Windows::Foundation::PropertyValue::CreateInt64(registrationToken.Value)));
    }

    static void RemoveListener(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() < 2 || !info[0]->IsString() || !info[1]->IsFunction()) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"wrong arguments, expected a string and a callback")));
        return;
      }

      String::Value eventName(v8::Isolate::GetCurrent(), info[0]);
      auto str = *eventName;

      if ((!NodeRT::Utils::CaseInsenstiveEquals(L"messageReceived", str))) {
        Nan::ThrowError(Nan::Error(String::Concat(v8::Isolate::GetCurrent(), NodeRT::Utils::NewString(L"given event name isn't supported: "), info[0].As<String>())));
        return;
      }

      Local<Function> callback = info[1].As<Function>();
      Local<Value> tokenMap = NodeRT::Utils::GetHiddenValue(callback, Nan::New<String>(REGISTRATION_TOKEN_MAP_PROPERTY_NAME).ToLocalChecked());

      if (tokenMap.IsEmpty() || Nan::Equals(tokenMap, Undefined()).FromMaybe(false)) {
        return;
      }

      Local<Value> opaqueWrapperObj =  Nan::Get(Nan::To<Object>(tokenMap).ToLocalChecked(), info[0]).ToLocalChecked();

      if (opaqueWrapperObj.IsEmpty() || Nan::Equals(opaqueWrapperObj,Undefined()).FromMaybe(false)) {
        return;
      }

      OpaqueWrapper *opaqueWrapper = OpaqueWrapper::Unwrap<OpaqueWrapper>(opaqueWrapperObj.As<Object>());

      long long tokenValue = (long long) opaqueWrapper->GetObjectInstance();
      ::Windows::Foundation::EventRegistrationToken registrationToken;
      registrationToken.Value = tokenValue;

      try  {
        if (NodeRT::Utils::CaseInsenstiveEquals(L"messageReceived", str)) {
          if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessageTypeEndpointListener^>(info.This()))
          {
            Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"The caller of this method isn't of the expected type or internal WinRt object was disposed")));
            return;
          }
          MidiMessageTypeEndpointListener *wrapper = MidiMessageTypeEndpointListener::Unwrap<MidiMessageTypeEndpointListener>(info.This());
          wrapper->_instance->MessageReceived::remove(registrationToken);
        }
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
      }

      Nan::Delete(Nan::To<Object>(tokenMap).ToLocalChecked(), Nan::To<String>(info[0]).ToLocalChecked());
    }
    private:
      ::Windows::Devices::Midi2::MidiMessageTypeEndpointListener^ _instance;
      static Persistent<FunctionTemplate> s_constructorTemplate;

      friend v8::Local<v8::Value> WrapMidiMessageTypeEndpointListener(::Windows::Devices::Midi2::MidiMessageTypeEndpointListener^ wintRtInstance);
      friend ::Windows::Devices::Midi2::MidiMessageTypeEndpointListener^ UnwrapMidiMessageTypeEndpointListener(Local<Value> value);
  };

  Persistent<FunctionTemplate> MidiMessageTypeEndpointListener::s_constructorTemplate;

  v8::Local<v8::Value> WrapMidiMessageTypeEndpointListener(::Windows::Devices::Midi2::MidiMessageTypeEndpointListener^ winRtInstance) {
    EscapableHandleScope scope;

    if (winRtInstance == nullptr) {
      return scope.Escape(Undefined());
    }

    Local<Value> opaqueWrapper = CreateOpaqueWrapper(winRtInstance);
    Local<Value> args[] = {opaqueWrapper};
    Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(MidiMessageTypeEndpointListener::s_constructorTemplate);
    return scope.Escape(Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(),_countof(args), args).ToLocalChecked());
  }

  ::Windows::Devices::Midi2::MidiMessageTypeEndpointListener^ UnwrapMidiMessageTypeEndpointListener(Local<Value> value) {
     return MidiMessageTypeEndpointListener::Unwrap<MidiMessageTypeEndpointListener>(Nan::To<Object>(value).ToLocalChecked())->_instance;
  }

  void InitMidiMessageTypeEndpointListener(Local<Object> exports) {
    MidiMessageTypeEndpointListener::Init(exports);
  }

  class MidiMessageUtility : public WrapperBase {
    public:
      
      static void Init(const Local<Object> exports) {
        HandleScope scope;

        Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(New);
        s_constructorTemplate.Reset(localRef);
        localRef->SetClassName(Nan::New<String>("MidiMessageUtility").ToLocalChecked());
        localRef->InstanceTemplate()->SetInternalFieldCount(1);






        Local<Object> constructor = Nan::To<Object>(Nan::GetFunction(localRef).ToLocalChecked()).ToLocalChecked();
        Nan::SetMethod(constructor, "castFrom", CastFrom);

        Nan::SetMethod(constructor, "validateMessage32MessageType", ValidateMessage32MessageType);
        Nan::SetMethod(constructor, "validateMessage64MessageType", ValidateMessage64MessageType);
        Nan::SetMethod(constructor, "validateMessage96MessageType", ValidateMessage96MessageType);
        Nan::SetMethod(constructor, "validateMessage128MessageType", ValidateMessage128MessageType);
        Nan::SetMethod(constructor, "getMessageTypeFromMessageFirstWord", GetMessageTypeFromMessageFirstWord);
        Nan::SetMethod(constructor, "getPacketTypeFromMessageFirstWord", GetPacketTypeFromMessageFirstWord);
        Nan::SetMethod(constructor, "messageTypeHasGroupField", MessageTypeHasGroupField);
        Nan::SetMethod(constructor, "replaceGroupInMessageFirstWord", ReplaceGroupInMessageFirstWord);
        Nan::SetMethod(constructor, "getGroupFromMessageFirstWord", GetGroupFromMessageFirstWord);
        Nan::SetMethod(constructor, "getStatusFromUtilityMessage", GetStatusFromUtilityMessage);
        Nan::SetMethod(constructor, "getStatusFromMidi1ChannelVoiceMessage", GetStatusFromMidi1ChannelVoiceMessage);
        Nan::SetMethod(constructor, "getStatusFromMidi2ChannelVoiceMessageFirstWord", GetStatusFromMidi2ChannelVoiceMessageFirstWord);
        Nan::SetMethod(constructor, "getStatusBankFromFlexDataMessageFirstWord", GetStatusBankFromFlexDataMessageFirstWord);
        Nan::SetMethod(constructor, "getStatusFromFlexDataMessageFirstWord", GetStatusFromFlexDataMessageFirstWord);
        Nan::SetMethod(constructor, "getStatusFromSystemCommonMessage", GetStatusFromSystemCommonMessage);
        Nan::SetMethod(constructor, "getStatusFromDataMessage64FirstWord", GetStatusFromDataMessage64FirstWord);
        Nan::SetMethod(constructor, "getNumberOfBytesFromDataMessage64FirstWord", GetNumberOfBytesFromDataMessage64FirstWord);
        Nan::SetMethod(constructor, "getStatusFromDataMessage128FirstWord", GetStatusFromDataMessage128FirstWord);
        Nan::SetMethod(constructor, "getNumberOfBytesFromDataMessage128FirstWord", GetNumberOfBytesFromDataMessage128FirstWord);
        Nan::SetMethod(constructor, "messageTypeHasChannelField", MessageTypeHasChannelField);
        Nan::SetMethod(constructor, "replaceChannelInMessageFirstWord", ReplaceChannelInMessageFirstWord);
        Nan::SetMethod(constructor, "getChannelFromMessageFirstWord", GetChannelFromMessageFirstWord);
        Nan::SetMethod(constructor, "getFormFromStreamMessageFirstWord", GetFormFromStreamMessageFirstWord);
        Nan::SetMethod(constructor, "getStatusFromStreamMessageFirstWord", GetStatusFromStreamMessageFirstWord);
        Nan::SetMethod(constructor, "getMessageFriendlyNameFromFirstWord", GetMessageFriendlyNameFromFirstWord);


        Nan::Set(exports, Nan::New<String>("MidiMessageUtility").ToLocalChecked(), constructor);
      }

      virtual ::Platform::Object^ GetObjectInstance() const override {
        return _instance;
      }

    private:

      MidiMessageUtility(::Windows::Devices::Midi2::MidiMessageUtility^ instance) {
        _instance = instance;
      }

      
    static void New(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(s_constructorTemplate);

      // in case the constructor was called without the new operator
      if (!localRef->HasInstance(info.This())) {
        if (info.Length() > 0) {
          std::unique_ptr<Local<Value> []> constructorArgs(new Local<Value>[info.Length()]);

          Local<Value> *argsPtr = constructorArgs.get();
          for (int i = 0; i < info.Length(); i++) {
            argsPtr[i] = info[i];
          }

          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), constructorArgs.get());
          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        } else {
          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), nullptr);

          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        }
      }

      ::Windows::Devices::Midi2::MidiMessageUtility^ winRtInstance;


      if (info.Length() == 1 && OpaqueWrapper::IsOpaqueWrapper(info[0]) &&
        NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessageUtility^>(info[0])) {
        try {
          winRtInstance = (::Windows::Devices::Midi2::MidiMessageUtility^) NodeRT::Utils::GetObjectInstance(info[0]);
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no suitable constructor found")));
        return;
      }

      NodeRT::Utils::SetHiddenValue(info.This(), Nan::New<String>("__winRtInstance__").ToLocalChecked(), True());

      MidiMessageUtility *wrapperInstance = new MidiMessageUtility(winRtInstance);
      wrapperInstance->Wrap(info.This());

      info.GetReturnValue().Set(info.This());
    }


      
    static void CastFrom(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;
      if (info.Length() < 1 || !NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessageUtility^>(info[0])) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no object provided, or given object could not be casted to requested type")));
        return;
      }

      ::Windows::Devices::Midi2::MidiMessageUtility^ winRtInstance;
      try {
        winRtInstance = (::Windows::Devices::Midi2::MidiMessageUtility^) NodeRT::Utils::GetObjectInstance(info[0]);
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }

      info.GetReturnValue().Set(WrapMidiMessageUtility(winRtInstance));
    }





    static void ValidateMessage32MessageType(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 1
        && info[0]->IsUint32())
      {
        try
        {
          unsigned int arg0 = static_cast<unsigned int>(Nan::To<uint32_t>(info[0]).FromMaybe(0));
          
          bool result;
          result = ::Windows::Devices::Midi2::MidiMessageUtility::ValidateMessage32MessageType(arg0);
          info.GetReturnValue().Set(Nan::New<Boolean>(result));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void ValidateMessage64MessageType(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 1
        && info[0]->IsUint32())
      {
        try
        {
          unsigned int arg0 = static_cast<unsigned int>(Nan::To<uint32_t>(info[0]).FromMaybe(0));
          
          bool result;
          result = ::Windows::Devices::Midi2::MidiMessageUtility::ValidateMessage64MessageType(arg0);
          info.GetReturnValue().Set(Nan::New<Boolean>(result));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void ValidateMessage96MessageType(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 1
        && info[0]->IsUint32())
      {
        try
        {
          unsigned int arg0 = static_cast<unsigned int>(Nan::To<uint32_t>(info[0]).FromMaybe(0));
          
          bool result;
          result = ::Windows::Devices::Midi2::MidiMessageUtility::ValidateMessage96MessageType(arg0);
          info.GetReturnValue().Set(Nan::New<Boolean>(result));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void ValidateMessage128MessageType(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 1
        && info[0]->IsUint32())
      {
        try
        {
          unsigned int arg0 = static_cast<unsigned int>(Nan::To<uint32_t>(info[0]).FromMaybe(0));
          
          bool result;
          result = ::Windows::Devices::Midi2::MidiMessageUtility::ValidateMessage128MessageType(arg0);
          info.GetReturnValue().Set(Nan::New<Boolean>(result));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void GetMessageTypeFromMessageFirstWord(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 1
        && info[0]->IsUint32())
      {
        try
        {
          unsigned int arg0 = static_cast<unsigned int>(Nan::To<uint32_t>(info[0]).FromMaybe(0));
          
          ::Windows::Devices::Midi2::MidiMessageType result;
          result = ::Windows::Devices::Midi2::MidiMessageUtility::GetMessageTypeFromMessageFirstWord(arg0);
          info.GetReturnValue().Set(Nan::New<Integer>(static_cast<int>(result)));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void GetPacketTypeFromMessageFirstWord(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 1
        && info[0]->IsUint32())
      {
        try
        {
          unsigned int arg0 = static_cast<unsigned int>(Nan::To<uint32_t>(info[0]).FromMaybe(0));
          
          ::Windows::Devices::Midi2::MidiPacketType result;
          result = ::Windows::Devices::Midi2::MidiMessageUtility::GetPacketTypeFromMessageFirstWord(arg0);
          info.GetReturnValue().Set(Nan::New<Integer>(static_cast<int>(result)));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void MessageTypeHasGroupField(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 1
        && info[0]->IsInt32())
      {
        try
        {
          ::Windows::Devices::Midi2::MidiMessageType arg0 = static_cast<::Windows::Devices::Midi2::MidiMessageType>(Nan::To<int32_t>(info[0]).FromMaybe(0));
          
          bool result;
          result = ::Windows::Devices::Midi2::MidiMessageUtility::MessageTypeHasGroupField(arg0);
          info.GetReturnValue().Set(Nan::New<Boolean>(result));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void ReplaceGroupInMessageFirstWord(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 2
        && info[0]->IsUint32()
        && NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiGroup^>(info[1]))
      {
        try
        {
          unsigned int arg0 = static_cast<unsigned int>(Nan::To<uint32_t>(info[0]).FromMaybe(0));
          ::Windows::Devices::Midi2::MidiGroup^ arg1 = UnwrapMidiGroup(info[1]);
          
          unsigned int result;
          result = ::Windows::Devices::Midi2::MidiMessageUtility::ReplaceGroupInMessageFirstWord(arg0, arg1);
          info.GetReturnValue().Set(Nan::New<Integer>(result));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void GetGroupFromMessageFirstWord(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 1
        && info[0]->IsUint32())
      {
        try
        {
          unsigned int arg0 = static_cast<unsigned int>(Nan::To<uint32_t>(info[0]).FromMaybe(0));
          
          ::Windows::Devices::Midi2::MidiGroup^ result;
          result = ::Windows::Devices::Midi2::MidiMessageUtility::GetGroupFromMessageFirstWord(arg0);
          info.GetReturnValue().Set(WrapMidiGroup(result));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void GetStatusFromUtilityMessage(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 1
        && info[0]->IsUint32())
      {
        try
        {
          unsigned int arg0 = static_cast<unsigned int>(Nan::To<uint32_t>(info[0]).FromMaybe(0));
          
          unsigned char result;
          result = ::Windows::Devices::Midi2::MidiMessageUtility::GetStatusFromUtilityMessage(arg0);
          info.GetReturnValue().Set(Nan::New<Integer>(result));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void GetStatusFromMidi1ChannelVoiceMessage(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 1
        && info[0]->IsUint32())
      {
        try
        {
          unsigned int arg0 = static_cast<unsigned int>(Nan::To<uint32_t>(info[0]).FromMaybe(0));
          
          ::Windows::Devices::Midi2::Midi1ChannelVoiceMessageStatus result;
          result = ::Windows::Devices::Midi2::MidiMessageUtility::GetStatusFromMidi1ChannelVoiceMessage(arg0);
          info.GetReturnValue().Set(Nan::New<Integer>(static_cast<int>(result)));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void GetStatusFromMidi2ChannelVoiceMessageFirstWord(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 1
        && info[0]->IsUint32())
      {
        try
        {
          unsigned int arg0 = static_cast<unsigned int>(Nan::To<uint32_t>(info[0]).FromMaybe(0));
          
          ::Windows::Devices::Midi2::Midi2ChannelVoiceMessageStatus result;
          result = ::Windows::Devices::Midi2::MidiMessageUtility::GetStatusFromMidi2ChannelVoiceMessageFirstWord(arg0);
          info.GetReturnValue().Set(Nan::New<Integer>(static_cast<int>(result)));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void GetStatusBankFromFlexDataMessageFirstWord(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 1
        && info[0]->IsUint32())
      {
        try
        {
          unsigned int arg0 = static_cast<unsigned int>(Nan::To<uint32_t>(info[0]).FromMaybe(0));
          
          unsigned char result;
          result = ::Windows::Devices::Midi2::MidiMessageUtility::GetStatusBankFromFlexDataMessageFirstWord(arg0);
          info.GetReturnValue().Set(Nan::New<Integer>(result));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void GetStatusFromFlexDataMessageFirstWord(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 1
        && info[0]->IsUint32())
      {
        try
        {
          unsigned int arg0 = static_cast<unsigned int>(Nan::To<uint32_t>(info[0]).FromMaybe(0));
          
          unsigned char result;
          result = ::Windows::Devices::Midi2::MidiMessageUtility::GetStatusFromFlexDataMessageFirstWord(arg0);
          info.GetReturnValue().Set(Nan::New<Integer>(result));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void GetStatusFromSystemCommonMessage(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 1
        && info[0]->IsUint32())
      {
        try
        {
          unsigned int arg0 = static_cast<unsigned int>(Nan::To<uint32_t>(info[0]).FromMaybe(0));
          
          unsigned char result;
          result = ::Windows::Devices::Midi2::MidiMessageUtility::GetStatusFromSystemCommonMessage(arg0);
          info.GetReturnValue().Set(Nan::New<Integer>(result));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void GetStatusFromDataMessage64FirstWord(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 1
        && info[0]->IsUint32())
      {
        try
        {
          unsigned int arg0 = static_cast<unsigned int>(Nan::To<uint32_t>(info[0]).FromMaybe(0));
          
          unsigned char result;
          result = ::Windows::Devices::Midi2::MidiMessageUtility::GetStatusFromDataMessage64FirstWord(arg0);
          info.GetReturnValue().Set(Nan::New<Integer>(result));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void GetNumberOfBytesFromDataMessage64FirstWord(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 1
        && info[0]->IsUint32())
      {
        try
        {
          unsigned int arg0 = static_cast<unsigned int>(Nan::To<uint32_t>(info[0]).FromMaybe(0));
          
          unsigned char result;
          result = ::Windows::Devices::Midi2::MidiMessageUtility::GetNumberOfBytesFromDataMessage64FirstWord(arg0);
          info.GetReturnValue().Set(Nan::New<Integer>(result));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void GetStatusFromDataMessage128FirstWord(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 1
        && info[0]->IsUint32())
      {
        try
        {
          unsigned int arg0 = static_cast<unsigned int>(Nan::To<uint32_t>(info[0]).FromMaybe(0));
          
          unsigned char result;
          result = ::Windows::Devices::Midi2::MidiMessageUtility::GetStatusFromDataMessage128FirstWord(arg0);
          info.GetReturnValue().Set(Nan::New<Integer>(result));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void GetNumberOfBytesFromDataMessage128FirstWord(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 1
        && info[0]->IsUint32())
      {
        try
        {
          unsigned int arg0 = static_cast<unsigned int>(Nan::To<uint32_t>(info[0]).FromMaybe(0));
          
          unsigned char result;
          result = ::Windows::Devices::Midi2::MidiMessageUtility::GetNumberOfBytesFromDataMessage128FirstWord(arg0);
          info.GetReturnValue().Set(Nan::New<Integer>(result));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void MessageTypeHasChannelField(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 1
        && info[0]->IsInt32())
      {
        try
        {
          ::Windows::Devices::Midi2::MidiMessageType arg0 = static_cast<::Windows::Devices::Midi2::MidiMessageType>(Nan::To<int32_t>(info[0]).FromMaybe(0));
          
          bool result;
          result = ::Windows::Devices::Midi2::MidiMessageUtility::MessageTypeHasChannelField(arg0);
          info.GetReturnValue().Set(Nan::New<Boolean>(result));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void ReplaceChannelInMessageFirstWord(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 2
        && info[0]->IsUint32()
        && NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiChannel^>(info[1]))
      {
        try
        {
          unsigned int arg0 = static_cast<unsigned int>(Nan::To<uint32_t>(info[0]).FromMaybe(0));
          ::Windows::Devices::Midi2::MidiChannel^ arg1 = UnwrapMidiChannel(info[1]);
          
          unsigned int result;
          result = ::Windows::Devices::Midi2::MidiMessageUtility::ReplaceChannelInMessageFirstWord(arg0, arg1);
          info.GetReturnValue().Set(Nan::New<Integer>(result));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void GetChannelFromMessageFirstWord(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 1
        && info[0]->IsUint32())
      {
        try
        {
          unsigned int arg0 = static_cast<unsigned int>(Nan::To<uint32_t>(info[0]).FromMaybe(0));
          
          ::Windows::Devices::Midi2::MidiChannel^ result;
          result = ::Windows::Devices::Midi2::MidiMessageUtility::GetChannelFromMessageFirstWord(arg0);
          info.GetReturnValue().Set(WrapMidiChannel(result));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void GetFormFromStreamMessageFirstWord(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 1
        && info[0]->IsUint32())
      {
        try
        {
          unsigned int arg0 = static_cast<unsigned int>(Nan::To<uint32_t>(info[0]).FromMaybe(0));
          
          unsigned char result;
          result = ::Windows::Devices::Midi2::MidiMessageUtility::GetFormFromStreamMessageFirstWord(arg0);
          info.GetReturnValue().Set(Nan::New<Integer>(result));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void GetStatusFromStreamMessageFirstWord(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 1
        && info[0]->IsUint32())
      {
        try
        {
          unsigned int arg0 = static_cast<unsigned int>(Nan::To<uint32_t>(info[0]).FromMaybe(0));
          
          unsigned short result;
          result = ::Windows::Devices::Midi2::MidiMessageUtility::GetStatusFromStreamMessageFirstWord(arg0);
          info.GetReturnValue().Set(Nan::New<Integer>(result));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void GetMessageFriendlyNameFromFirstWord(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 1
        && info[0]->IsUint32())
      {
        try
        {
          unsigned int arg0 = static_cast<unsigned int>(Nan::To<uint32_t>(info[0]).FromMaybe(0));
          
          Platform::String^ result;
          result = ::Windows::Devices::Midi2::MidiMessageUtility::GetMessageFriendlyNameFromFirstWord(arg0);
          info.GetReturnValue().Set(NodeRT::Utils::NewString(result->Data()));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }



    private:
      ::Windows::Devices::Midi2::MidiMessageUtility^ _instance;
      static Persistent<FunctionTemplate> s_constructorTemplate;

      friend v8::Local<v8::Value> WrapMidiMessageUtility(::Windows::Devices::Midi2::MidiMessageUtility^ wintRtInstance);
      friend ::Windows::Devices::Midi2::MidiMessageUtility^ UnwrapMidiMessageUtility(Local<Value> value);
  };

  Persistent<FunctionTemplate> MidiMessageUtility::s_constructorTemplate;

  v8::Local<v8::Value> WrapMidiMessageUtility(::Windows::Devices::Midi2::MidiMessageUtility^ winRtInstance) {
    EscapableHandleScope scope;

    if (winRtInstance == nullptr) {
      return scope.Escape(Undefined());
    }

    Local<Value> opaqueWrapper = CreateOpaqueWrapper(winRtInstance);
    Local<Value> args[] = {opaqueWrapper};
    Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(MidiMessageUtility::s_constructorTemplate);
    return scope.Escape(Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(),_countof(args), args).ToLocalChecked());
  }

  ::Windows::Devices::Midi2::MidiMessageUtility^ UnwrapMidiMessageUtility(Local<Value> value) {
     return MidiMessageUtility::Unwrap<MidiMessageUtility>(Nan::To<Object>(value).ToLocalChecked())->_instance;
  }

  void InitMidiMessageUtility(Local<Object> exports) {
    MidiMessageUtility::Init(exports);
  }

  class MidiService : public WrapperBase {
    public:
      
      static void Init(const Local<Object> exports) {
        HandleScope scope;

        Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(New);
        s_constructorTemplate.Reset(localRef);
        localRef->SetClassName(Nan::New<String>("MidiService").ToLocalChecked());
        localRef->InstanceTemplate()->SetInternalFieldCount(1);






        Local<Object> constructor = Nan::To<Object>(Nan::GetFunction(localRef).ToLocalChecked()).ToLocalChecked();
        Nan::SetMethod(constructor, "castFrom", CastFrom);

        Nan::SetMethod(constructor, "pingService", PingService);
        Nan::SetMethod(constructor, "getInstalledTransportPlugins", GetInstalledTransportPlugins);
        Nan::SetMethod(constructor, "getInstalledMessageProcessingPlugins", GetInstalledMessageProcessingPlugins);
        Nan::SetMethod(constructor, "getOutgoingMessageQueueMaxMessageCapacity", GetOutgoingMessageQueueMaxMessageCapacity);


        Nan::Set(exports, Nan::New<String>("MidiService").ToLocalChecked(), constructor);
      }

      virtual ::Platform::Object^ GetObjectInstance() const override {
        return _instance;
      }

    private:

      MidiService(::Windows::Devices::Midi2::MidiService^ instance) {
        _instance = instance;
      }

      
    static void New(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(s_constructorTemplate);

      // in case the constructor was called without the new operator
      if (!localRef->HasInstance(info.This())) {
        if (info.Length() > 0) {
          std::unique_ptr<Local<Value> []> constructorArgs(new Local<Value>[info.Length()]);

          Local<Value> *argsPtr = constructorArgs.get();
          for (int i = 0; i < info.Length(); i++) {
            argsPtr[i] = info[i];
          }

          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), constructorArgs.get());
          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        } else {
          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), nullptr);

          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        }
      }

      ::Windows::Devices::Midi2::MidiService^ winRtInstance;


      if (info.Length() == 1 && OpaqueWrapper::IsOpaqueWrapper(info[0]) &&
        NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiService^>(info[0])) {
        try {
          winRtInstance = (::Windows::Devices::Midi2::MidiService^) NodeRT::Utils::GetObjectInstance(info[0]);
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no suitable constructor found")));
        return;
      }

      NodeRT::Utils::SetHiddenValue(info.This(), Nan::New<String>("__winRtInstance__").ToLocalChecked(), True());

      MidiService *wrapperInstance = new MidiService(winRtInstance);
      wrapperInstance->Wrap(info.This());

      info.GetReturnValue().Set(info.This());
    }


      
    static void CastFrom(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;
      if (info.Length() < 1 || !NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiService^>(info[0])) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no object provided, or given object could not be casted to requested type")));
        return;
      }

      ::Windows::Devices::Midi2::MidiService^ winRtInstance;
      try {
        winRtInstance = (::Windows::Devices::Midi2::MidiService^) NodeRT::Utils::GetObjectInstance(info[0]);
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }

      info.GetReturnValue().Set(WrapMidiService(winRtInstance));
    }





    static void PingService(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 1
        && info[0]->IsInt32())
      {
        try
        {
          unsigned char arg0 = static_cast<unsigned char>(Nan::To<int32_t>(info[0]).FromMaybe(0));
          
          ::Windows::Devices::Midi2::MidiServicePingResponseSummary^ result;
          result = ::Windows::Devices::Midi2::MidiService::PingService(arg0);
          info.GetReturnValue().Set(WrapMidiServicePingResponseSummary(result));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
      else if (info.Length() == 2
        && info[0]->IsInt32()
        && info[1]->IsUint32())
      {
        try
        {
          unsigned char arg0 = static_cast<unsigned char>(Nan::To<int32_t>(info[0]).FromMaybe(0));
          unsigned int arg1 = static_cast<unsigned int>(Nan::To<uint32_t>(info[1]).FromMaybe(0));
          
          ::Windows::Devices::Midi2::MidiServicePingResponseSummary^ result;
          result = ::Windows::Devices::Midi2::MidiService::PingService(arg0, arg1);
          info.GetReturnValue().Set(WrapMidiServicePingResponseSummary(result));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void GetInstalledTransportPlugins(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 0)
      {
        try
        {
          ::Windows::Foundation::Collections::IVectorView<::Windows::Devices::Midi2::MidiTransportPluginInformation^>^ result;
          result = ::Windows::Devices::Midi2::MidiService::GetInstalledTransportPlugins();
          info.GetReturnValue().Set(NodeRT::Collections::VectorViewWrapper<::Windows::Devices::Midi2::MidiTransportPluginInformation^>::CreateVectorViewWrapper(result, 
            [](::Windows::Devices::Midi2::MidiTransportPluginInformation^ val) -> Local<Value> {
              return WrapMidiTransportPluginInformation(val);
            },
            [](Local<Value> value) -> bool {
              return NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiTransportPluginInformation^>(value);
            },
            [](Local<Value> value) -> ::Windows::Devices::Midi2::MidiTransportPluginInformation^ {
              return UnwrapMidiTransportPluginInformation(value);
            }
          ));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void GetInstalledMessageProcessingPlugins(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 0)
      {
        try
        {
          ::Windows::Foundation::Collections::IVectorView<::Windows::Devices::Midi2::MidiMessageProcessingPluginInformation^>^ result;
          result = ::Windows::Devices::Midi2::MidiService::GetInstalledMessageProcessingPlugins();
          info.GetReturnValue().Set(NodeRT::Collections::VectorViewWrapper<::Windows::Devices::Midi2::MidiMessageProcessingPluginInformation^>::CreateVectorViewWrapper(result, 
            [](::Windows::Devices::Midi2::MidiMessageProcessingPluginInformation^ val) -> Local<Value> {
              return WrapMidiMessageProcessingPluginInformation(val);
            },
            [](Local<Value> value) -> bool {
              return NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessageProcessingPluginInformation^>(value);
            },
            [](Local<Value> value) -> ::Windows::Devices::Midi2::MidiMessageProcessingPluginInformation^ {
              return UnwrapMidiMessageProcessingPluginInformation(value);
            }
          ));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void GetOutgoingMessageQueueMaxMessageCapacity(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 0)
      {
        try
        {
          unsigned int result;
          result = ::Windows::Devices::Midi2::MidiService::GetOutgoingMessageQueueMaxMessageCapacity();
          info.GetReturnValue().Set(Nan::New<Integer>(result));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }



    private:
      ::Windows::Devices::Midi2::MidiService^ _instance;
      static Persistent<FunctionTemplate> s_constructorTemplate;

      friend v8::Local<v8::Value> WrapMidiService(::Windows::Devices::Midi2::MidiService^ wintRtInstance);
      friend ::Windows::Devices::Midi2::MidiService^ UnwrapMidiService(Local<Value> value);
  };

  Persistent<FunctionTemplate> MidiService::s_constructorTemplate;

  v8::Local<v8::Value> WrapMidiService(::Windows::Devices::Midi2::MidiService^ winRtInstance) {
    EscapableHandleScope scope;

    if (winRtInstance == nullptr) {
      return scope.Escape(Undefined());
    }

    Local<Value> opaqueWrapper = CreateOpaqueWrapper(winRtInstance);
    Local<Value> args[] = {opaqueWrapper};
    Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(MidiService::s_constructorTemplate);
    return scope.Escape(Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(),_countof(args), args).ToLocalChecked());
  }

  ::Windows::Devices::Midi2::MidiService^ UnwrapMidiService(Local<Value> value) {
     return MidiService::Unwrap<MidiService>(Nan::To<Object>(value).ToLocalChecked())->_instance;
  }

  void InitMidiService(Local<Object> exports) {
    MidiService::Init(exports);
  }

  class MidiServicePingResponse : public WrapperBase {
    public:
      
      static void Init(const Local<Object> exports) {
        HandleScope scope;

        Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(New);
        s_constructorTemplate.Reset(localRef);
        localRef->SetClassName(Nan::New<String>("MidiServicePingResponse").ToLocalChecked());
        localRef->InstanceTemplate()->SetInternalFieldCount(1);





          
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("clientDeltaTimestamp").ToLocalChecked(), ClientDeltaTimestampGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("clientReceiveMidiTimestamp").ToLocalChecked(), ClientReceiveMidiTimestampGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("clientSendMidiTimestamp").ToLocalChecked(), ClientSendMidiTimestampGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("index").ToLocalChecked(), IndexGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("serviceReportedMidiTimestamp").ToLocalChecked(), ServiceReportedMidiTimestampGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("sourceId").ToLocalChecked(), SourceIdGetter);

        Local<Object> constructor = Nan::To<Object>(Nan::GetFunction(localRef).ToLocalChecked()).ToLocalChecked();
        Nan::SetMethod(constructor, "castFrom", CastFrom);



        Nan::Set(exports, Nan::New<String>("MidiServicePingResponse").ToLocalChecked(), constructor);
      }

      virtual ::Platform::Object^ GetObjectInstance() const override {
        return _instance;
      }

    private:

      MidiServicePingResponse(::Windows::Devices::Midi2::MidiServicePingResponse^ instance) {
        _instance = instance;
      }

      
    static void New(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(s_constructorTemplate);

      // in case the constructor was called without the new operator
      if (!localRef->HasInstance(info.This())) {
        if (info.Length() > 0) {
          std::unique_ptr<Local<Value> []> constructorArgs(new Local<Value>[info.Length()]);

          Local<Value> *argsPtr = constructorArgs.get();
          for (int i = 0; i < info.Length(); i++) {
            argsPtr[i] = info[i];
          }

          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), constructorArgs.get());
          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        } else {
          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), nullptr);

          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        }
      }

      ::Windows::Devices::Midi2::MidiServicePingResponse^ winRtInstance;


      if (info.Length() == 1 && OpaqueWrapper::IsOpaqueWrapper(info[0]) &&
        NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiServicePingResponse^>(info[0])) {
        try {
          winRtInstance = (::Windows::Devices::Midi2::MidiServicePingResponse^) NodeRT::Utils::GetObjectInstance(info[0]);
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no suitable constructor found")));
        return;
      }

      NodeRT::Utils::SetHiddenValue(info.This(), Nan::New<String>("__winRtInstance__").ToLocalChecked(), True());

      MidiServicePingResponse *wrapperInstance = new MidiServicePingResponse(winRtInstance);
      wrapperInstance->Wrap(info.This());

      info.GetReturnValue().Set(info.This());
    }


      
    static void CastFrom(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;
      if (info.Length() < 1 || !NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiServicePingResponse^>(info[0])) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no object provided, or given object could not be casted to requested type")));
        return;
      }

      ::Windows::Devices::Midi2::MidiServicePingResponse^ winRtInstance;
      try {
        winRtInstance = (::Windows::Devices::Midi2::MidiServicePingResponse^) NodeRT::Utils::GetObjectInstance(info[0]);
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }

      info.GetReturnValue().Set(WrapMidiServicePingResponse(winRtInstance));
    }





    static void ClientDeltaTimestampGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiServicePingResponse^>(info.This())) {
        return;
      }

      MidiServicePingResponse *wrapper = MidiServicePingResponse::Unwrap<MidiServicePingResponse>(info.This());

      try  {
        unsigned __int64 result = wrapper->_instance->ClientDeltaTimestamp;
        info.GetReturnValue().Set(Nan::New<Number>(static_cast<double>(result)));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void ClientReceiveMidiTimestampGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiServicePingResponse^>(info.This())) {
        return;
      }

      MidiServicePingResponse *wrapper = MidiServicePingResponse::Unwrap<MidiServicePingResponse>(info.This());

      try  {
        unsigned __int64 result = wrapper->_instance->ClientReceiveMidiTimestamp;
        info.GetReturnValue().Set(Nan::New<Number>(static_cast<double>(result)));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void ClientSendMidiTimestampGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiServicePingResponse^>(info.This())) {
        return;
      }

      MidiServicePingResponse *wrapper = MidiServicePingResponse::Unwrap<MidiServicePingResponse>(info.This());

      try  {
        unsigned __int64 result = wrapper->_instance->ClientSendMidiTimestamp;
        info.GetReturnValue().Set(Nan::New<Number>(static_cast<double>(result)));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void IndexGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiServicePingResponse^>(info.This())) {
        return;
      }

      MidiServicePingResponse *wrapper = MidiServicePingResponse::Unwrap<MidiServicePingResponse>(info.This());

      try  {
        unsigned int result = wrapper->_instance->Index;
        info.GetReturnValue().Set(Nan::New<Integer>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void ServiceReportedMidiTimestampGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiServicePingResponse^>(info.This())) {
        return;
      }

      MidiServicePingResponse *wrapper = MidiServicePingResponse::Unwrap<MidiServicePingResponse>(info.This());

      try  {
        unsigned __int64 result = wrapper->_instance->ServiceReportedMidiTimestamp;
        info.GetReturnValue().Set(Nan::New<Number>(static_cast<double>(result)));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void SourceIdGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiServicePingResponse^>(info.This())) {
        return;
      }

      MidiServicePingResponse *wrapper = MidiServicePingResponse::Unwrap<MidiServicePingResponse>(info.This());

      try  {
        unsigned int result = wrapper->_instance->SourceId;
        info.GetReturnValue().Set(Nan::New<Integer>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      


    private:
      ::Windows::Devices::Midi2::MidiServicePingResponse^ _instance;
      static Persistent<FunctionTemplate> s_constructorTemplate;

      friend v8::Local<v8::Value> WrapMidiServicePingResponse(::Windows::Devices::Midi2::MidiServicePingResponse^ wintRtInstance);
      friend ::Windows::Devices::Midi2::MidiServicePingResponse^ UnwrapMidiServicePingResponse(Local<Value> value);
  };

  Persistent<FunctionTemplate> MidiServicePingResponse::s_constructorTemplate;

  v8::Local<v8::Value> WrapMidiServicePingResponse(::Windows::Devices::Midi2::MidiServicePingResponse^ winRtInstance) {
    EscapableHandleScope scope;

    if (winRtInstance == nullptr) {
      return scope.Escape(Undefined());
    }

    Local<Value> opaqueWrapper = CreateOpaqueWrapper(winRtInstance);
    Local<Value> args[] = {opaqueWrapper};
    Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(MidiServicePingResponse::s_constructorTemplate);
    return scope.Escape(Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(),_countof(args), args).ToLocalChecked());
  }

  ::Windows::Devices::Midi2::MidiServicePingResponse^ UnwrapMidiServicePingResponse(Local<Value> value) {
     return MidiServicePingResponse::Unwrap<MidiServicePingResponse>(Nan::To<Object>(value).ToLocalChecked())->_instance;
  }

  void InitMidiServicePingResponse(Local<Object> exports) {
    MidiServicePingResponse::Init(exports);
  }

  class MidiServicePingResponseSummary : public WrapperBase {
    public:
      
      static void Init(const Local<Object> exports) {
        HandleScope scope;

        Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(New);
        s_constructorTemplate.Reset(localRef);
        localRef->SetClassName(Nan::New<String>("MidiServicePingResponseSummary").ToLocalChecked());
        localRef->InstanceTemplate()->SetInternalFieldCount(1);





          
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("averagePingRoundTripMidiClock").ToLocalChecked(), AveragePingRoundTripMidiClockGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("failureReason").ToLocalChecked(), FailureReasonGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("responses").ToLocalChecked(), ResponsesGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("success").ToLocalChecked(), SuccessGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("totalPingRoundTripMidiClock").ToLocalChecked(), TotalPingRoundTripMidiClockGetter);

        Local<Object> constructor = Nan::To<Object>(Nan::GetFunction(localRef).ToLocalChecked()).ToLocalChecked();
        Nan::SetMethod(constructor, "castFrom", CastFrom);



        Nan::Set(exports, Nan::New<String>("MidiServicePingResponseSummary").ToLocalChecked(), constructor);
      }

      virtual ::Platform::Object^ GetObjectInstance() const override {
        return _instance;
      }

    private:

      MidiServicePingResponseSummary(::Windows::Devices::Midi2::MidiServicePingResponseSummary^ instance) {
        _instance = instance;
      }

      
    static void New(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(s_constructorTemplate);

      // in case the constructor was called without the new operator
      if (!localRef->HasInstance(info.This())) {
        if (info.Length() > 0) {
          std::unique_ptr<Local<Value> []> constructorArgs(new Local<Value>[info.Length()]);

          Local<Value> *argsPtr = constructorArgs.get();
          for (int i = 0; i < info.Length(); i++) {
            argsPtr[i] = info[i];
          }

          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), constructorArgs.get());
          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        } else {
          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), nullptr);

          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        }
      }

      ::Windows::Devices::Midi2::MidiServicePingResponseSummary^ winRtInstance;


      if (info.Length() == 1 && OpaqueWrapper::IsOpaqueWrapper(info[0]) &&
        NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiServicePingResponseSummary^>(info[0])) {
        try {
          winRtInstance = (::Windows::Devices::Midi2::MidiServicePingResponseSummary^) NodeRT::Utils::GetObjectInstance(info[0]);
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no suitable constructor found")));
        return;
      }

      NodeRT::Utils::SetHiddenValue(info.This(), Nan::New<String>("__winRtInstance__").ToLocalChecked(), True());

      MidiServicePingResponseSummary *wrapperInstance = new MidiServicePingResponseSummary(winRtInstance);
      wrapperInstance->Wrap(info.This());

      info.GetReturnValue().Set(info.This());
    }


      
    static void CastFrom(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;
      if (info.Length() < 1 || !NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiServicePingResponseSummary^>(info[0])) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no object provided, or given object could not be casted to requested type")));
        return;
      }

      ::Windows::Devices::Midi2::MidiServicePingResponseSummary^ winRtInstance;
      try {
        winRtInstance = (::Windows::Devices::Midi2::MidiServicePingResponseSummary^) NodeRT::Utils::GetObjectInstance(info[0]);
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }

      info.GetReturnValue().Set(WrapMidiServicePingResponseSummary(winRtInstance));
    }





    static void AveragePingRoundTripMidiClockGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiServicePingResponseSummary^>(info.This())) {
        return;
      }

      MidiServicePingResponseSummary *wrapper = MidiServicePingResponseSummary::Unwrap<MidiServicePingResponseSummary>(info.This());

      try  {
        unsigned __int64 result = wrapper->_instance->AveragePingRoundTripMidiClock;
        info.GetReturnValue().Set(Nan::New<Number>(static_cast<double>(result)));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void FailureReasonGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiServicePingResponseSummary^>(info.This())) {
        return;
      }

      MidiServicePingResponseSummary *wrapper = MidiServicePingResponseSummary::Unwrap<MidiServicePingResponseSummary>(info.This());

      try  {
        Platform::String^ result = wrapper->_instance->FailureReason;
        info.GetReturnValue().Set(NodeRT::Utils::NewString(result->Data()));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void ResponsesGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiServicePingResponseSummary^>(info.This())) {
        return;
      }

      MidiServicePingResponseSummary *wrapper = MidiServicePingResponseSummary::Unwrap<MidiServicePingResponseSummary>(info.This());

      try  {
        ::Windows::Foundation::Collections::IVectorView<::Windows::Devices::Midi2::MidiServicePingResponse^>^ result = wrapper->_instance->Responses;
        info.GetReturnValue().Set(NodeRT::Collections::VectorViewWrapper<::Windows::Devices::Midi2::MidiServicePingResponse^>::CreateVectorViewWrapper(result, 
            [](::Windows::Devices::Midi2::MidiServicePingResponse^ val) -> Local<Value> {
              return WrapMidiServicePingResponse(val);
            },
            [](Local<Value> value) -> bool {
              return NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiServicePingResponse^>(value);
            },
            [](Local<Value> value) -> ::Windows::Devices::Midi2::MidiServicePingResponse^ {
              return UnwrapMidiServicePingResponse(value);
            }
          ));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void SuccessGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiServicePingResponseSummary^>(info.This())) {
        return;
      }

      MidiServicePingResponseSummary *wrapper = MidiServicePingResponseSummary::Unwrap<MidiServicePingResponseSummary>(info.This());

      try  {
        bool result = wrapper->_instance->Success;
        info.GetReturnValue().Set(Nan::New<Boolean>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void TotalPingRoundTripMidiClockGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiServicePingResponseSummary^>(info.This())) {
        return;
      }

      MidiServicePingResponseSummary *wrapper = MidiServicePingResponseSummary::Unwrap<MidiServicePingResponseSummary>(info.This());

      try  {
        unsigned __int64 result = wrapper->_instance->TotalPingRoundTripMidiClock;
        info.GetReturnValue().Set(Nan::New<Number>(static_cast<double>(result)));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      


    private:
      ::Windows::Devices::Midi2::MidiServicePingResponseSummary^ _instance;
      static Persistent<FunctionTemplate> s_constructorTemplate;

      friend v8::Local<v8::Value> WrapMidiServicePingResponseSummary(::Windows::Devices::Midi2::MidiServicePingResponseSummary^ wintRtInstance);
      friend ::Windows::Devices::Midi2::MidiServicePingResponseSummary^ UnwrapMidiServicePingResponseSummary(Local<Value> value);
  };

  Persistent<FunctionTemplate> MidiServicePingResponseSummary::s_constructorTemplate;

  v8::Local<v8::Value> WrapMidiServicePingResponseSummary(::Windows::Devices::Midi2::MidiServicePingResponseSummary^ winRtInstance) {
    EscapableHandleScope scope;

    if (winRtInstance == nullptr) {
      return scope.Escape(Undefined());
    }

    Local<Value> opaqueWrapper = CreateOpaqueWrapper(winRtInstance);
    Local<Value> args[] = {opaqueWrapper};
    Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(MidiServicePingResponseSummary::s_constructorTemplate);
    return scope.Escape(Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(),_countof(args), args).ToLocalChecked());
  }

  ::Windows::Devices::Midi2::MidiServicePingResponseSummary^ UnwrapMidiServicePingResponseSummary(Local<Value> value) {
     return MidiServicePingResponseSummary::Unwrap<MidiServicePingResponseSummary>(Nan::To<Object>(value).ToLocalChecked())->_instance;
  }

  void InitMidiServicePingResponseSummary(Local<Object> exports) {
    MidiServicePingResponseSummary::Init(exports);
  }

  class MidiSession : public WrapperBase {
    public:
      
      static void Init(const Local<Object> exports) {
        HandleScope scope;

        Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(New);
        s_constructorTemplate.Reset(localRef);
        localRef->SetClassName(Nan::New<String>("MidiSession").ToLocalChecked());
        localRef->InstanceTemplate()->SetInternalFieldCount(1);


          
            Nan::SetPrototypeMethod(localRef, "createEndpointConnection", CreateEndpointConnection);
            Nan::SetPrototypeMethod(localRef, "createVirtualDeviceAndConnection", CreateVirtualDeviceAndConnection);
            Nan::SetPrototypeMethod(localRef, "disconnectEndpointConnection", DisconnectEndpointConnection);
            Nan::SetPrototypeMethod(localRef, "close", Close);
          



          
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("connections").ToLocalChecked(), ConnectionsGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("id").ToLocalChecked(), IdGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("isOpen").ToLocalChecked(), IsOpenGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("name").ToLocalChecked(), NameGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("settings").ToLocalChecked(), SettingsGetter);

        Local<Object> constructor = Nan::To<Object>(Nan::GetFunction(localRef).ToLocalChecked()).ToLocalChecked();
        Nan::SetMethod(constructor, "castFrom", CastFrom);

        Nan::SetMethod(constructor, "createSession", CreateSession);


        Nan::Set(exports, Nan::New<String>("MidiSession").ToLocalChecked(), constructor);
      }

      virtual ::Platform::Object^ GetObjectInstance() const override {
        return _instance;
      }

    private:

      MidiSession(::Windows::Devices::Midi2::MidiSession^ instance) {
        _instance = instance;
      }

      
    static void New(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(s_constructorTemplate);

      // in case the constructor was called without the new operator
      if (!localRef->HasInstance(info.This())) {
        if (info.Length() > 0) {
          std::unique_ptr<Local<Value> []> constructorArgs(new Local<Value>[info.Length()]);

          Local<Value> *argsPtr = constructorArgs.get();
          for (int i = 0; i < info.Length(); i++) {
            argsPtr[i] = info[i];
          }

          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), constructorArgs.get());
          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        } else {
          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), nullptr);

          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        }
      }

      ::Windows::Devices::Midi2::MidiSession^ winRtInstance;


      if (info.Length() == 1 && OpaqueWrapper::IsOpaqueWrapper(info[0]) &&
        NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiSession^>(info[0])) {
        try {
          winRtInstance = (::Windows::Devices::Midi2::MidiSession^) NodeRT::Utils::GetObjectInstance(info[0]);
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no suitable constructor found")));
        return;
      }

      NodeRT::Utils::SetHiddenValue(info.This(), Nan::New<String>("__winRtInstance__").ToLocalChecked(), True());

      MidiSession *wrapperInstance = new MidiSession(winRtInstance);
      wrapperInstance->Wrap(info.This());

      info.GetReturnValue().Set(info.This());
    }


      
    static void CastFrom(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;
      if (info.Length() < 1 || !NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiSession^>(info[0])) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no object provided, or given object could not be casted to requested type")));
        return;
      }

      ::Windows::Devices::Midi2::MidiSession^ winRtInstance;
      try {
        winRtInstance = (::Windows::Devices::Midi2::MidiSession^) NodeRT::Utils::GetObjectInstance(info[0]);
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }

      info.GetReturnValue().Set(WrapMidiSession(winRtInstance));
    }


    static void CreateEndpointConnection(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiSession^>(info.This())) {
        return;
      }

      MidiSession *wrapper = MidiSession::Unwrap<MidiSession>(info.This());

      if (info.Length() == 1
        && info[0]->IsString())
      {
        try
        {
          Platform::String^ arg0 = ref new Platform::String(NodeRT::Utils::StringToWchar(v8::String::Value(v8::Isolate::GetCurrent(), info[0])));
          
          ::Windows::Devices::Midi2::MidiEndpointConnection^ result;
          result = wrapper->_instance->CreateEndpointConnection(arg0);
          info.GetReturnValue().Set(WrapMidiEndpointConnection(result));
          return;
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
      else if (info.Length() == 2
        && info[0]->IsString()
        && NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointConnectionOptions^>(info[1]))
      {
        try
        {
          Platform::String^ arg0 = ref new Platform::String(NodeRT::Utils::StringToWchar(v8::String::Value(v8::Isolate::GetCurrent(), info[0])));
          ::Windows::Devices::Midi2::MidiEndpointConnectionOptions^ arg1 = UnwrapMidiEndpointConnectionOptions(info[1]);
          
          ::Windows::Devices::Midi2::MidiEndpointConnection^ result;
          result = wrapper->_instance->CreateEndpointConnection(arg0, arg1);
          info.GetReturnValue().Set(WrapMidiEndpointConnection(result));
          return;
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
      else if (info.Length() == 3
        && info[0]->IsString()
        && NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiEndpointConnectionOptions^>(info[1])
        && NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::IMidiEndpointDefinedConnectionSettings^>(info[2]))
      {
        try
        {
          Platform::String^ arg0 = ref new Platform::String(NodeRT::Utils::StringToWchar(v8::String::Value(v8::Isolate::GetCurrent(), info[0])));
          ::Windows::Devices::Midi2::MidiEndpointConnectionOptions^ arg1 = UnwrapMidiEndpointConnectionOptions(info[1]);
          ::Windows::Devices::Midi2::IMidiEndpointDefinedConnectionSettings^ arg2 = UnwrapIMidiEndpointDefinedConnectionSettings(info[2]);
          
          ::Windows::Devices::Midi2::MidiEndpointConnection^ result;
          result = wrapper->_instance->CreateEndpointConnection(arg0, arg1, arg2);
          info.GetReturnValue().Set(WrapMidiEndpointConnection(result));
          return;
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }
    static void CreateVirtualDeviceAndConnection(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiSession^>(info.This())) {
        return;
      }

      MidiSession *wrapper = MidiSession::Unwrap<MidiSession>(info.This());

      if (info.Length() == 2
        && info[0]->IsString()
        && info[1]->IsString())
      {
        try
        {
          Platform::String^ arg0 = ref new Platform::String(NodeRT::Utils::StringToWchar(v8::String::Value(v8::Isolate::GetCurrent(), info[0])));
          Platform::String^ arg1 = ref new Platform::String(NodeRT::Utils::StringToWchar(v8::String::Value(v8::Isolate::GetCurrent(), info[1])));
          
          ::Windows::Devices::Midi2::MidiEndpointConnection^ result;
          result = wrapper->_instance->CreateVirtualDeviceAndConnection(arg0, arg1);
          info.GetReturnValue().Set(WrapMidiEndpointConnection(result));
          return;
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }
    static void DisconnectEndpointConnection(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiSession^>(info.This())) {
        return;
      }

      MidiSession *wrapper = MidiSession::Unwrap<MidiSession>(info.This());

      if (info.Length() == 1
        && NodeRT::Utils::IsGuid(info[0]))
      {
        try
        {
          ::Platform::Guid arg0 = NodeRT::Utils::GuidFromJs(info[0]);
          
          wrapper->_instance->DisconnectEndpointConnection(arg0);
          return;
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void Close(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiSession^>(info.This())) {
        return;
      }

      MidiSession *wrapper = MidiSession::Unwrap<MidiSession>(info.This());

      if (info.Length() == 0) {
        try {
          delete wrapper->_instance;
          wrapper->_instance = nullptr;
          return;
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      } else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }



    static void CreateSession(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 1
        && info[0]->IsString())
      {
        try
        {
          Platform::String^ arg0 = ref new Platform::String(NodeRT::Utils::StringToWchar(v8::String::Value(v8::Isolate::GetCurrent(), info[0])));
          
          ::Windows::Devices::Midi2::MidiSession^ result;
          result = ::Windows::Devices::Midi2::MidiSession::CreateSession(arg0);
          info.GetReturnValue().Set(WrapMidiSession(result));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
      else if (info.Length() == 2
        && info[0]->IsString()
        && NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiSessionSettings^>(info[1]))
      {
        try
        {
          Platform::String^ arg0 = ref new Platform::String(NodeRT::Utils::StringToWchar(v8::String::Value(v8::Isolate::GetCurrent(), info[0])));
          ::Windows::Devices::Midi2::MidiSessionSettings^ arg1 = UnwrapMidiSessionSettings(info[1]);
          
          ::Windows::Devices::Midi2::MidiSession^ result;
          result = ::Windows::Devices::Midi2::MidiSession::CreateSession(arg0, arg1);
          info.GetReturnValue().Set(WrapMidiSession(result));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void ConnectionsGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiSession^>(info.This())) {
        return;
      }

      MidiSession *wrapper = MidiSession::Unwrap<MidiSession>(info.This());

      try  {
        ::Windows::Foundation::Collections::IMapView<::Platform::Guid, ::Windows::Devices::Midi2::MidiEndpointConnection^>^ result = wrapper->_instance->Connections;
        info.GetReturnValue().Set(NodeRT::Collections::MapViewWrapper<::Platform::Guid,::Windows::Devices::Midi2::MidiEndpointConnection^>::CreateMapViewWrapper(result, 
            [](::Platform::Guid val) -> Local<Value> {
              return NodeRT::Utils::GuidToJs(val);
            },
            [](Local<Value> value) -> bool {
              return NodeRT::Utils::IsGuid(value);
            },
            [](Local<Value> value) -> ::Platform::Guid {
              return NodeRT::Utils::GuidFromJs(value);
            },
            [](::Windows::Devices::Midi2::MidiEndpointConnection^ val) -> Local<Value> {
              return WrapMidiEndpointConnection(val);
            }
          ));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void IdGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiSession^>(info.This())) {
        return;
      }

      MidiSession *wrapper = MidiSession::Unwrap<MidiSession>(info.This());

      try  {
        ::Platform::Guid result = wrapper->_instance->Id;
        info.GetReturnValue().Set(NodeRT::Utils::GuidToJs(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void IsOpenGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiSession^>(info.This())) {
        return;
      }

      MidiSession *wrapper = MidiSession::Unwrap<MidiSession>(info.This());

      try  {
        bool result = wrapper->_instance->IsOpen;
        info.GetReturnValue().Set(Nan::New<Boolean>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void NameGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiSession^>(info.This())) {
        return;
      }

      MidiSession *wrapper = MidiSession::Unwrap<MidiSession>(info.This());

      try  {
        Platform::String^ result = wrapper->_instance->Name;
        info.GetReturnValue().Set(NodeRT::Utils::NewString(result->Data()));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void SettingsGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiSession^>(info.This())) {
        return;
      }

      MidiSession *wrapper = MidiSession::Unwrap<MidiSession>(info.This());

      try  {
        ::Windows::Devices::Midi2::MidiSessionSettings^ result = wrapper->_instance->Settings;
        info.GetReturnValue().Set(WrapMidiSessionSettings(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      


    private:
      ::Windows::Devices::Midi2::MidiSession^ _instance;
      static Persistent<FunctionTemplate> s_constructorTemplate;

      friend v8::Local<v8::Value> WrapMidiSession(::Windows::Devices::Midi2::MidiSession^ wintRtInstance);
      friend ::Windows::Devices::Midi2::MidiSession^ UnwrapMidiSession(Local<Value> value);
  };

  Persistent<FunctionTemplate> MidiSession::s_constructorTemplate;

  v8::Local<v8::Value> WrapMidiSession(::Windows::Devices::Midi2::MidiSession^ winRtInstance) {
    EscapableHandleScope scope;

    if (winRtInstance == nullptr) {
      return scope.Escape(Undefined());
    }

    Local<Value> opaqueWrapper = CreateOpaqueWrapper(winRtInstance);
    Local<Value> args[] = {opaqueWrapper};
    Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(MidiSession::s_constructorTemplate);
    return scope.Escape(Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(),_countof(args), args).ToLocalChecked());
  }

  ::Windows::Devices::Midi2::MidiSession^ UnwrapMidiSession(Local<Value> value) {
     return MidiSession::Unwrap<MidiSession>(Nan::To<Object>(value).ToLocalChecked())->_instance;
  }

  void InitMidiSession(Local<Object> exports) {
    MidiSession::Init(exports);
  }

  class MidiSessionSettings : public WrapperBase {
    public:
      
      static void Init(const Local<Object> exports) {
        HandleScope scope;

        Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(New);
        s_constructorTemplate.Reset(localRef);
        localRef->SetClassName(Nan::New<String>("MidiSessionSettings").ToLocalChecked());
        localRef->InstanceTemplate()->SetInternalFieldCount(1);





          
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("useMmcssThreads").ToLocalChecked(), UseMmcssThreadsGetter, UseMmcssThreadsSetter);

        Local<Object> constructor = Nan::To<Object>(Nan::GetFunction(localRef).ToLocalChecked()).ToLocalChecked();
        Nan::SetMethod(constructor, "castFrom", CastFrom);



        Nan::Set(exports, Nan::New<String>("MidiSessionSettings").ToLocalChecked(), constructor);
      }

      virtual ::Platform::Object^ GetObjectInstance() const override {
        return _instance;
      }

    private:

      MidiSessionSettings(::Windows::Devices::Midi2::MidiSessionSettings^ instance) {
        _instance = instance;
      }

      
    static void New(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(s_constructorTemplate);

      // in case the constructor was called without the new operator
      if (!localRef->HasInstance(info.This())) {
        if (info.Length() > 0) {
          std::unique_ptr<Local<Value> []> constructorArgs(new Local<Value>[info.Length()]);

          Local<Value> *argsPtr = constructorArgs.get();
          for (int i = 0; i < info.Length(); i++) {
            argsPtr[i] = info[i];
          }

          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), constructorArgs.get());
          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        } else {
          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), nullptr);

          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        }
      }

      ::Windows::Devices::Midi2::MidiSessionSettings^ winRtInstance;


      if (info.Length() == 1 && OpaqueWrapper::IsOpaqueWrapper(info[0]) &&
        NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiSessionSettings^>(info[0])) {
        try {
          winRtInstance = (::Windows::Devices::Midi2::MidiSessionSettings^) NodeRT::Utils::GetObjectInstance(info[0]);
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
      else if (info.Length() == 0)
      {
        try {
          winRtInstance = ref new ::Windows::Devices::Midi2::MidiSessionSettings();
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no suitable constructor found")));
        return;
      }

      NodeRT::Utils::SetHiddenValue(info.This(), Nan::New<String>("__winRtInstance__").ToLocalChecked(), True());

      MidiSessionSettings *wrapperInstance = new MidiSessionSettings(winRtInstance);
      wrapperInstance->Wrap(info.This());

      info.GetReturnValue().Set(info.This());
    }


      
    static void CastFrom(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;
      if (info.Length() < 1 || !NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiSessionSettings^>(info[0])) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no object provided, or given object could not be casted to requested type")));
        return;
      }

      ::Windows::Devices::Midi2::MidiSessionSettings^ winRtInstance;
      try {
        winRtInstance = (::Windows::Devices::Midi2::MidiSessionSettings^) NodeRT::Utils::GetObjectInstance(info[0]);
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }

      info.GetReturnValue().Set(WrapMidiSessionSettings(winRtInstance));
    }





    static void UseMmcssThreadsGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiSessionSettings^>(info.This())) {
        return;
      }

      MidiSessionSettings *wrapper = MidiSessionSettings::Unwrap<MidiSessionSettings>(info.This());

      try  {
        bool result = wrapper->_instance->UseMmcssThreads;
        info.GetReturnValue().Set(Nan::New<Boolean>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void UseMmcssThreadsSetter(Local<String> property, Local<Value> value, const Nan::PropertyCallbackInfo<void> &info) {
      HandleScope scope;

      if (!value->IsBoolean()) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Value to set is of unexpected type")));
        return;
      }

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiSessionSettings^>(info.This())) {
        return;
      }

      MidiSessionSettings *wrapper = MidiSessionSettings::Unwrap<MidiSessionSettings>(info.This());

      try {

        bool winRtValue = Nan::To<bool>(value).FromMaybe(false);

        wrapper->_instance->UseMmcssThreads = winRtValue;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
      }
    }
      


    private:
      ::Windows::Devices::Midi2::MidiSessionSettings^ _instance;
      static Persistent<FunctionTemplate> s_constructorTemplate;

      friend v8::Local<v8::Value> WrapMidiSessionSettings(::Windows::Devices::Midi2::MidiSessionSettings^ wintRtInstance);
      friend ::Windows::Devices::Midi2::MidiSessionSettings^ UnwrapMidiSessionSettings(Local<Value> value);
  };

  Persistent<FunctionTemplate> MidiSessionSettings::s_constructorTemplate;

  v8::Local<v8::Value> WrapMidiSessionSettings(::Windows::Devices::Midi2::MidiSessionSettings^ winRtInstance) {
    EscapableHandleScope scope;

    if (winRtInstance == nullptr) {
      return scope.Escape(Undefined());
    }

    Local<Value> opaqueWrapper = CreateOpaqueWrapper(winRtInstance);
    Local<Value> args[] = {opaqueWrapper};
    Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(MidiSessionSettings::s_constructorTemplate);
    return scope.Escape(Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(),_countof(args), args).ToLocalChecked());
  }

  ::Windows::Devices::Midi2::MidiSessionSettings^ UnwrapMidiSessionSettings(Local<Value> value) {
     return MidiSessionSettings::Unwrap<MidiSessionSettings>(Nan::To<Object>(value).ToLocalChecked())->_instance;
  }

  void InitMidiSessionSettings(Local<Object> exports) {
    MidiSessionSettings::Init(exports);
  }

  class MidiStreamConfigurationRequestedSettings : public WrapperBase {
    public:
      
      static void Init(const Local<Object> exports) {
        HandleScope scope;

        Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(New);
        s_constructorTemplate.Reset(localRef);
        localRef->SetClassName(Nan::New<String>("MidiStreamConfigurationRequestedSettings").ToLocalChecked());
        localRef->InstanceTemplate()->SetInternalFieldCount(1);





          
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("specificationVersionMinor").ToLocalChecked(), SpecificationVersionMinorGetter, SpecificationVersionMinorSetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("specificationVersionMajor").ToLocalChecked(), SpecificationVersionMajorGetter, SpecificationVersionMajorSetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("requestEndpointTransmitJitterReductionTimestamps").ToLocalChecked(), RequestEndpointTransmitJitterReductionTimestampsGetter, RequestEndpointTransmitJitterReductionTimestampsSetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("requestEndpointReceiveJitterReductionTimestamps").ToLocalChecked(), RequestEndpointReceiveJitterReductionTimestampsGetter, RequestEndpointReceiveJitterReductionTimestampsSetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("preferredMidiProtocol").ToLocalChecked(), PreferredMidiProtocolGetter, PreferredMidiProtocolSetter);

        Local<Object> constructor = Nan::To<Object>(Nan::GetFunction(localRef).ToLocalChecked()).ToLocalChecked();
        Nan::SetMethod(constructor, "castFrom", CastFrom);



        Nan::Set(exports, Nan::New<String>("MidiStreamConfigurationRequestedSettings").ToLocalChecked(), constructor);
      }

      virtual ::Platform::Object^ GetObjectInstance() const override {
        return _instance;
      }

    private:

      MidiStreamConfigurationRequestedSettings(::Windows::Devices::Midi2::MidiStreamConfigurationRequestedSettings^ instance) {
        _instance = instance;
      }

      
    static void New(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(s_constructorTemplate);

      // in case the constructor was called without the new operator
      if (!localRef->HasInstance(info.This())) {
        if (info.Length() > 0) {
          std::unique_ptr<Local<Value> []> constructorArgs(new Local<Value>[info.Length()]);

          Local<Value> *argsPtr = constructorArgs.get();
          for (int i = 0; i < info.Length(); i++) {
            argsPtr[i] = info[i];
          }

          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), constructorArgs.get());
          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        } else {
          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), nullptr);

          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        }
      }

      ::Windows::Devices::Midi2::MidiStreamConfigurationRequestedSettings^ winRtInstance;


      if (info.Length() == 1 && OpaqueWrapper::IsOpaqueWrapper(info[0]) &&
        NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiStreamConfigurationRequestedSettings^>(info[0])) {
        try {
          winRtInstance = (::Windows::Devices::Midi2::MidiStreamConfigurationRequestedSettings^) NodeRT::Utils::GetObjectInstance(info[0]);
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
      else if (info.Length() == 0)
      {
        try {
          winRtInstance = ref new ::Windows::Devices::Midi2::MidiStreamConfigurationRequestedSettings();
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no suitable constructor found")));
        return;
      }

      NodeRT::Utils::SetHiddenValue(info.This(), Nan::New<String>("__winRtInstance__").ToLocalChecked(), True());

      MidiStreamConfigurationRequestedSettings *wrapperInstance = new MidiStreamConfigurationRequestedSettings(winRtInstance);
      wrapperInstance->Wrap(info.This());

      info.GetReturnValue().Set(info.This());
    }


      
    static void CastFrom(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;
      if (info.Length() < 1 || !NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiStreamConfigurationRequestedSettings^>(info[0])) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no object provided, or given object could not be casted to requested type")));
        return;
      }

      ::Windows::Devices::Midi2::MidiStreamConfigurationRequestedSettings^ winRtInstance;
      try {
        winRtInstance = (::Windows::Devices::Midi2::MidiStreamConfigurationRequestedSettings^) NodeRT::Utils::GetObjectInstance(info[0]);
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }

      info.GetReturnValue().Set(WrapMidiStreamConfigurationRequestedSettings(winRtInstance));
    }





    static void SpecificationVersionMinorGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiStreamConfigurationRequestedSettings^>(info.This())) {
        return;
      }

      MidiStreamConfigurationRequestedSettings *wrapper = MidiStreamConfigurationRequestedSettings::Unwrap<MidiStreamConfigurationRequestedSettings>(info.This());

      try  {
        unsigned char result = wrapper->_instance->SpecificationVersionMinor;
        info.GetReturnValue().Set(Nan::New<Integer>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void SpecificationVersionMinorSetter(Local<String> property, Local<Value> value, const Nan::PropertyCallbackInfo<void> &info) {
      HandleScope scope;

      if (!value->IsInt32()) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Value to set is of unexpected type")));
        return;
      }

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiStreamConfigurationRequestedSettings^>(info.This())) {
        return;
      }

      MidiStreamConfigurationRequestedSettings *wrapper = MidiStreamConfigurationRequestedSettings::Unwrap<MidiStreamConfigurationRequestedSettings>(info.This());

      try {

        unsigned char winRtValue = static_cast<unsigned char>(Nan::To<int32_t>(value).FromMaybe(0));

        wrapper->_instance->SpecificationVersionMinor = winRtValue;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
      }
    }
      
    static void SpecificationVersionMajorGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiStreamConfigurationRequestedSettings^>(info.This())) {
        return;
      }

      MidiStreamConfigurationRequestedSettings *wrapper = MidiStreamConfigurationRequestedSettings::Unwrap<MidiStreamConfigurationRequestedSettings>(info.This());

      try  {
        unsigned char result = wrapper->_instance->SpecificationVersionMajor;
        info.GetReturnValue().Set(Nan::New<Integer>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void SpecificationVersionMajorSetter(Local<String> property, Local<Value> value, const Nan::PropertyCallbackInfo<void> &info) {
      HandleScope scope;

      if (!value->IsInt32()) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Value to set is of unexpected type")));
        return;
      }

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiStreamConfigurationRequestedSettings^>(info.This())) {
        return;
      }

      MidiStreamConfigurationRequestedSettings *wrapper = MidiStreamConfigurationRequestedSettings::Unwrap<MidiStreamConfigurationRequestedSettings>(info.This());

      try {

        unsigned char winRtValue = static_cast<unsigned char>(Nan::To<int32_t>(value).FromMaybe(0));

        wrapper->_instance->SpecificationVersionMajor = winRtValue;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
      }
    }
      
    static void RequestEndpointTransmitJitterReductionTimestampsGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiStreamConfigurationRequestedSettings^>(info.This())) {
        return;
      }

      MidiStreamConfigurationRequestedSettings *wrapper = MidiStreamConfigurationRequestedSettings::Unwrap<MidiStreamConfigurationRequestedSettings>(info.This());

      try  {
        bool result = wrapper->_instance->RequestEndpointTransmitJitterReductionTimestamps;
        info.GetReturnValue().Set(Nan::New<Boolean>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void RequestEndpointTransmitJitterReductionTimestampsSetter(Local<String> property, Local<Value> value, const Nan::PropertyCallbackInfo<void> &info) {
      HandleScope scope;

      if (!value->IsBoolean()) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Value to set is of unexpected type")));
        return;
      }

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiStreamConfigurationRequestedSettings^>(info.This())) {
        return;
      }

      MidiStreamConfigurationRequestedSettings *wrapper = MidiStreamConfigurationRequestedSettings::Unwrap<MidiStreamConfigurationRequestedSettings>(info.This());

      try {

        bool winRtValue = Nan::To<bool>(value).FromMaybe(false);

        wrapper->_instance->RequestEndpointTransmitJitterReductionTimestamps = winRtValue;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
      }
    }
      
    static void RequestEndpointReceiveJitterReductionTimestampsGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiStreamConfigurationRequestedSettings^>(info.This())) {
        return;
      }

      MidiStreamConfigurationRequestedSettings *wrapper = MidiStreamConfigurationRequestedSettings::Unwrap<MidiStreamConfigurationRequestedSettings>(info.This());

      try  {
        bool result = wrapper->_instance->RequestEndpointReceiveJitterReductionTimestamps;
        info.GetReturnValue().Set(Nan::New<Boolean>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void RequestEndpointReceiveJitterReductionTimestampsSetter(Local<String> property, Local<Value> value, const Nan::PropertyCallbackInfo<void> &info) {
      HandleScope scope;

      if (!value->IsBoolean()) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Value to set is of unexpected type")));
        return;
      }

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiStreamConfigurationRequestedSettings^>(info.This())) {
        return;
      }

      MidiStreamConfigurationRequestedSettings *wrapper = MidiStreamConfigurationRequestedSettings::Unwrap<MidiStreamConfigurationRequestedSettings>(info.This());

      try {

        bool winRtValue = Nan::To<bool>(value).FromMaybe(false);

        wrapper->_instance->RequestEndpointReceiveJitterReductionTimestamps = winRtValue;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
      }
    }
      
    static void PreferredMidiProtocolGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiStreamConfigurationRequestedSettings^>(info.This())) {
        return;
      }

      MidiStreamConfigurationRequestedSettings *wrapper = MidiStreamConfigurationRequestedSettings::Unwrap<MidiStreamConfigurationRequestedSettings>(info.This());

      try  {
        ::Windows::Devices::Midi2::MidiProtocol result = wrapper->_instance->PreferredMidiProtocol;
        info.GetReturnValue().Set(Nan::New<Integer>(static_cast<int>(result)));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void PreferredMidiProtocolSetter(Local<String> property, Local<Value> value, const Nan::PropertyCallbackInfo<void> &info) {
      HandleScope scope;

      if (!value->IsInt32()) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Value to set is of unexpected type")));
        return;
      }

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiStreamConfigurationRequestedSettings^>(info.This())) {
        return;
      }

      MidiStreamConfigurationRequestedSettings *wrapper = MidiStreamConfigurationRequestedSettings::Unwrap<MidiStreamConfigurationRequestedSettings>(info.This());

      try {

        ::Windows::Devices::Midi2::MidiProtocol winRtValue = static_cast<::Windows::Devices::Midi2::MidiProtocol>(Nan::To<int32_t>(value).FromMaybe(0));

        wrapper->_instance->PreferredMidiProtocol = winRtValue;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
      }
    }
      


    private:
      ::Windows::Devices::Midi2::MidiStreamConfigurationRequestedSettings^ _instance;
      static Persistent<FunctionTemplate> s_constructorTemplate;

      friend v8::Local<v8::Value> WrapMidiStreamConfigurationRequestedSettings(::Windows::Devices::Midi2::MidiStreamConfigurationRequestedSettings^ wintRtInstance);
      friend ::Windows::Devices::Midi2::MidiStreamConfigurationRequestedSettings^ UnwrapMidiStreamConfigurationRequestedSettings(Local<Value> value);
  };

  Persistent<FunctionTemplate> MidiStreamConfigurationRequestedSettings::s_constructorTemplate;

  v8::Local<v8::Value> WrapMidiStreamConfigurationRequestedSettings(::Windows::Devices::Midi2::MidiStreamConfigurationRequestedSettings^ winRtInstance) {
    EscapableHandleScope scope;

    if (winRtInstance == nullptr) {
      return scope.Escape(Undefined());
    }

    Local<Value> opaqueWrapper = CreateOpaqueWrapper(winRtInstance);
    Local<Value> args[] = {opaqueWrapper};
    Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(MidiStreamConfigurationRequestedSettings::s_constructorTemplate);
    return scope.Escape(Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(),_countof(args), args).ToLocalChecked());
  }

  ::Windows::Devices::Midi2::MidiStreamConfigurationRequestedSettings^ UnwrapMidiStreamConfigurationRequestedSettings(Local<Value> value) {
     return MidiStreamConfigurationRequestedSettings::Unwrap<MidiStreamConfigurationRequestedSettings>(Nan::To<Object>(value).ToLocalChecked())->_instance;
  }

  void InitMidiStreamConfigurationRequestedSettings(Local<Object> exports) {
    MidiStreamConfigurationRequestedSettings::Init(exports);
  }

  class MidiStreamMessageBuilder : public WrapperBase {
    public:
      
      static void Init(const Local<Object> exports) {
        HandleScope scope;

        Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(New);
        s_constructorTemplate.Reset(localRef);
        localRef->SetClassName(Nan::New<String>("MidiStreamMessageBuilder").ToLocalChecked());
        localRef->InstanceTemplate()->SetInternalFieldCount(1);






        Local<Object> constructor = Nan::To<Object>(Nan::GetFunction(localRef).ToLocalChecked()).ToLocalChecked();
        Nan::SetMethod(constructor, "castFrom", CastFrom);

        Nan::SetMethod(constructor, "buildEndpointDiscoveryMessage", BuildEndpointDiscoveryMessage);
        Nan::SetMethod(constructor, "buildEndpointInformationNotificationMessage", BuildEndpointInformationNotificationMessage);
        Nan::SetMethod(constructor, "buildDeviceIdentityNotificationMessage", BuildDeviceIdentityNotificationMessage);
        Nan::SetMethod(constructor, "buildEndpointNameNotificationMessages", BuildEndpointNameNotificationMessages);
        Nan::SetMethod(constructor, "buildProductInstanceIdNotificationMessages", BuildProductInstanceIdNotificationMessages);
        Nan::SetMethod(constructor, "parseEndpointNameNotificationMessages", ParseEndpointNameNotificationMessages);
        Nan::SetMethod(constructor, "parseProductInstanceIdNotificationMessages", ParseProductInstanceIdNotificationMessages);
        Nan::SetMethod(constructor, "buildStreamConfigurationRequestMessage", BuildStreamConfigurationRequestMessage);
        Nan::SetMethod(constructor, "buildStreamConfigurationNotificationMessage", BuildStreamConfigurationNotificationMessage);
        Nan::SetMethod(constructor, "buildFunctionBlockDiscoveryMessage", BuildFunctionBlockDiscoveryMessage);
        Nan::SetMethod(constructor, "buildFunctionBlockInfoNotificationMessage", BuildFunctionBlockInfoNotificationMessage);
        Nan::SetMethod(constructor, "buildFunctionBlockNameNotificationMessages", BuildFunctionBlockNameNotificationMessages);
        Nan::SetMethod(constructor, "parseFunctionBlockNameNotificationMessages", ParseFunctionBlockNameNotificationMessages);


        Nan::Set(exports, Nan::New<String>("MidiStreamMessageBuilder").ToLocalChecked(), constructor);
      }

      virtual ::Platform::Object^ GetObjectInstance() const override {
        return _instance;
      }

    private:

      MidiStreamMessageBuilder(::Windows::Devices::Midi2::MidiStreamMessageBuilder^ instance) {
        _instance = instance;
      }

      
    static void New(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(s_constructorTemplate);

      // in case the constructor was called without the new operator
      if (!localRef->HasInstance(info.This())) {
        if (info.Length() > 0) {
          std::unique_ptr<Local<Value> []> constructorArgs(new Local<Value>[info.Length()]);

          Local<Value> *argsPtr = constructorArgs.get();
          for (int i = 0; i < info.Length(); i++) {
            argsPtr[i] = info[i];
          }

          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), constructorArgs.get());
          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        } else {
          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), nullptr);

          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        }
      }

      ::Windows::Devices::Midi2::MidiStreamMessageBuilder^ winRtInstance;


      if (info.Length() == 1 && OpaqueWrapper::IsOpaqueWrapper(info[0]) &&
        NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiStreamMessageBuilder^>(info[0])) {
        try {
          winRtInstance = (::Windows::Devices::Midi2::MidiStreamMessageBuilder^) NodeRT::Utils::GetObjectInstance(info[0]);
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no suitable constructor found")));
        return;
      }

      NodeRT::Utils::SetHiddenValue(info.This(), Nan::New<String>("__winRtInstance__").ToLocalChecked(), True());

      MidiStreamMessageBuilder *wrapperInstance = new MidiStreamMessageBuilder(winRtInstance);
      wrapperInstance->Wrap(info.This());

      info.GetReturnValue().Set(info.This());
    }


      
    static void CastFrom(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;
      if (info.Length() < 1 || !NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiStreamMessageBuilder^>(info[0])) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no object provided, or given object could not be casted to requested type")));
        return;
      }

      ::Windows::Devices::Midi2::MidiStreamMessageBuilder^ winRtInstance;
      try {
        winRtInstance = (::Windows::Devices::Midi2::MidiStreamMessageBuilder^) NodeRT::Utils::GetObjectInstance(info[0]);
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }

      info.GetReturnValue().Set(WrapMidiStreamMessageBuilder(winRtInstance));
    }





    static void BuildEndpointDiscoveryMessage(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 4
        && info[0]->IsNumber()
        && info[1]->IsInt32()
        && info[2]->IsInt32()
        && info[3]->IsInt32())
      {
        try
        {
          unsigned __int64 arg0 = static_cast<unsigned __int64>(Nan::To<int64_t>(info[0]).FromMaybe(0));
          unsigned char arg1 = static_cast<unsigned char>(Nan::To<int32_t>(info[1]).FromMaybe(0));
          unsigned char arg2 = static_cast<unsigned char>(Nan::To<int32_t>(info[2]).FromMaybe(0));
          ::Windows::Devices::Midi2::MidiEndpointDiscoveryFilterFlags arg3 = static_cast<::Windows::Devices::Midi2::MidiEndpointDiscoveryFilterFlags>(Nan::To<int32_t>(info[3]).FromMaybe(0));
          
          ::Windows::Devices::Midi2::MidiMessage128^ result;
          result = ::Windows::Devices::Midi2::MidiStreamMessageBuilder::BuildEndpointDiscoveryMessage(arg0, arg1, arg2, arg3);
          info.GetReturnValue().Set(WrapMidiMessage128(result));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void BuildEndpointInformationNotificationMessage(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 9
        && info[0]->IsNumber()
        && info[1]->IsInt32()
        && info[2]->IsInt32()
        && info[3]->IsBoolean()
        && info[4]->IsInt32()
        && info[5]->IsBoolean()
        && info[6]->IsBoolean()
        && info[7]->IsBoolean()
        && info[8]->IsBoolean())
      {
        try
        {
          unsigned __int64 arg0 = static_cast<unsigned __int64>(Nan::To<int64_t>(info[0]).FromMaybe(0));
          unsigned char arg1 = static_cast<unsigned char>(Nan::To<int32_t>(info[1]).FromMaybe(0));
          unsigned char arg2 = static_cast<unsigned char>(Nan::To<int32_t>(info[2]).FromMaybe(0));
          bool arg3 = Nan::To<bool>(info[3]).FromMaybe(false);
          unsigned char arg4 = static_cast<unsigned char>(Nan::To<int32_t>(info[4]).FromMaybe(0));
          bool arg5 = Nan::To<bool>(info[5]).FromMaybe(false);
          bool arg6 = Nan::To<bool>(info[6]).FromMaybe(false);
          bool arg7 = Nan::To<bool>(info[7]).FromMaybe(false);
          bool arg8 = Nan::To<bool>(info[8]).FromMaybe(false);
          
          ::Windows::Devices::Midi2::MidiMessage128^ result;
          result = ::Windows::Devices::Midi2::MidiStreamMessageBuilder::BuildEndpointInformationNotificationMessage(arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8);
          info.GetReturnValue().Set(WrapMidiMessage128(result));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void BuildDeviceIdentityNotificationMessage(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 12
        && info[0]->IsNumber()
        && info[1]->IsInt32()
        && info[2]->IsInt32()
        && info[3]->IsInt32()
        && info[4]->IsInt32()
        && info[5]->IsInt32()
        && info[6]->IsInt32()
        && info[7]->IsInt32()
        && info[8]->IsInt32()
        && info[9]->IsInt32()
        && info[10]->IsInt32()
        && info[11]->IsInt32())
      {
        try
        {
          unsigned __int64 arg0 = static_cast<unsigned __int64>(Nan::To<int64_t>(info[0]).FromMaybe(0));
          unsigned char arg1 = static_cast<unsigned char>(Nan::To<int32_t>(info[1]).FromMaybe(0));
          unsigned char arg2 = static_cast<unsigned char>(Nan::To<int32_t>(info[2]).FromMaybe(0));
          unsigned char arg3 = static_cast<unsigned char>(Nan::To<int32_t>(info[3]).FromMaybe(0));
          unsigned char arg4 = static_cast<unsigned char>(Nan::To<int32_t>(info[4]).FromMaybe(0));
          unsigned char arg5 = static_cast<unsigned char>(Nan::To<int32_t>(info[5]).FromMaybe(0));
          unsigned char arg6 = static_cast<unsigned char>(Nan::To<int32_t>(info[6]).FromMaybe(0));
          unsigned char arg7 = static_cast<unsigned char>(Nan::To<int32_t>(info[7]).FromMaybe(0));
          unsigned char arg8 = static_cast<unsigned char>(Nan::To<int32_t>(info[8]).FromMaybe(0));
          unsigned char arg9 = static_cast<unsigned char>(Nan::To<int32_t>(info[9]).FromMaybe(0));
          unsigned char arg10 = static_cast<unsigned char>(Nan::To<int32_t>(info[10]).FromMaybe(0));
          unsigned char arg11 = static_cast<unsigned char>(Nan::To<int32_t>(info[11]).FromMaybe(0));
          
          ::Windows::Devices::Midi2::MidiMessage128^ result;
          result = ::Windows::Devices::Midi2::MidiStreamMessageBuilder::BuildDeviceIdentityNotificationMessage(arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11);
          info.GetReturnValue().Set(WrapMidiMessage128(result));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void BuildEndpointNameNotificationMessages(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 2
        && info[0]->IsNumber()
        && info[1]->IsString())
      {
        try
        {
          unsigned __int64 arg0 = static_cast<unsigned __int64>(Nan::To<int64_t>(info[0]).FromMaybe(0));
          Platform::String^ arg1 = ref new Platform::String(NodeRT::Utils::StringToWchar(v8::String::Value(v8::Isolate::GetCurrent(), info[1])));
          
          ::Windows::Foundation::Collections::IVector<::Windows::Devices::Midi2::MidiMessage128^>^ result;
          result = ::Windows::Devices::Midi2::MidiStreamMessageBuilder::BuildEndpointNameNotificationMessages(arg0, arg1);
          info.GetReturnValue().Set(NodeRT::Collections::VectorWrapper<::Windows::Devices::Midi2::MidiMessage128^>::CreateVectorWrapper(result, 
            [](::Windows::Devices::Midi2::MidiMessage128^ val) -> Local<Value> {
              return WrapMidiMessage128(val);
            },
            [](Local<Value> value) -> bool {
              return NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessage128^>(value);
            },
            [](Local<Value> value) -> ::Windows::Devices::Midi2::MidiMessage128^ {
              return UnwrapMidiMessage128(value);
            }
          ));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void BuildProductInstanceIdNotificationMessages(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 2
        && info[0]->IsNumber()
        && info[1]->IsString())
      {
        try
        {
          unsigned __int64 arg0 = static_cast<unsigned __int64>(Nan::To<int64_t>(info[0]).FromMaybe(0));
          Platform::String^ arg1 = ref new Platform::String(NodeRT::Utils::StringToWchar(v8::String::Value(v8::Isolate::GetCurrent(), info[1])));
          
          ::Windows::Foundation::Collections::IVector<::Windows::Devices::Midi2::MidiMessage128^>^ result;
          result = ::Windows::Devices::Midi2::MidiStreamMessageBuilder::BuildProductInstanceIdNotificationMessages(arg0, arg1);
          info.GetReturnValue().Set(NodeRT::Collections::VectorWrapper<::Windows::Devices::Midi2::MidiMessage128^>::CreateVectorWrapper(result, 
            [](::Windows::Devices::Midi2::MidiMessage128^ val) -> Local<Value> {
              return WrapMidiMessage128(val);
            },
            [](Local<Value> value) -> bool {
              return NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessage128^>(value);
            },
            [](Local<Value> value) -> ::Windows::Devices::Midi2::MidiMessage128^ {
              return UnwrapMidiMessage128(value);
            }
          ));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void ParseEndpointNameNotificationMessages(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 1
        && (NodeRT::Utils::IsWinRtWrapperOf<::Windows::Foundation::Collections::IVector<::Windows::Devices::Midi2::MidiMessage128^>^>(info[0]) || info[0]->IsArray()))
      {
        try
        {
          ::Windows::Foundation::Collections::IVector<::Windows::Devices::Midi2::MidiMessage128^>^ arg0 = 
            [] (v8::Local<v8::Value> value) -> ::Windows::Foundation::Collections::IVector<::Windows::Devices::Midi2::MidiMessage128^>^
            {
              if (value->IsArray())
              {
                return NodeRT::Collections::JsArrayToWinrtVector<::Windows::Devices::Midi2::MidiMessage128^>(value.As<Array>(), 
                 [](Local<Value> value) -> bool {
                   return NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessage128^>(value);
                 },
                 [](Local<Value> value) -> ::Windows::Devices::Midi2::MidiMessage128^ {
                   return UnwrapMidiMessage128(value);
                 }
                );
              }
              else
              {
                return dynamic_cast<::Windows::Foundation::Collections::IVector<::Windows::Devices::Midi2::MidiMessage128^>^>(NodeRT::Utils::GetObjectInstance(value));
              }
            } (info[0]);
          
          Platform::String^ result;
          result = ::Windows::Devices::Midi2::MidiStreamMessageBuilder::ParseEndpointNameNotificationMessages(arg0);
          info.GetReturnValue().Set(NodeRT::Utils::NewString(result->Data()));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void ParseProductInstanceIdNotificationMessages(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 1
        && (NodeRT::Utils::IsWinRtWrapperOf<::Windows::Foundation::Collections::IVector<::Windows::Devices::Midi2::MidiMessage128^>^>(info[0]) || info[0]->IsArray()))
      {
        try
        {
          ::Windows::Foundation::Collections::IVector<::Windows::Devices::Midi2::MidiMessage128^>^ arg0 = 
            [] (v8::Local<v8::Value> value) -> ::Windows::Foundation::Collections::IVector<::Windows::Devices::Midi2::MidiMessage128^>^
            {
              if (value->IsArray())
              {
                return NodeRT::Collections::JsArrayToWinrtVector<::Windows::Devices::Midi2::MidiMessage128^>(value.As<Array>(), 
                 [](Local<Value> value) -> bool {
                   return NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessage128^>(value);
                 },
                 [](Local<Value> value) -> ::Windows::Devices::Midi2::MidiMessage128^ {
                   return UnwrapMidiMessage128(value);
                 }
                );
              }
              else
              {
                return dynamic_cast<::Windows::Foundation::Collections::IVector<::Windows::Devices::Midi2::MidiMessage128^>^>(NodeRT::Utils::GetObjectInstance(value));
              }
            } (info[0]);
          
          Platform::String^ result;
          result = ::Windows::Devices::Midi2::MidiStreamMessageBuilder::ParseProductInstanceIdNotificationMessages(arg0);
          info.GetReturnValue().Set(NodeRT::Utils::NewString(result->Data()));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void BuildStreamConfigurationRequestMessage(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 4
        && info[0]->IsNumber()
        && info[1]->IsInt32()
        && info[2]->IsBoolean()
        && info[3]->IsBoolean())
      {
        try
        {
          unsigned __int64 arg0 = static_cast<unsigned __int64>(Nan::To<int64_t>(info[0]).FromMaybe(0));
          unsigned char arg1 = static_cast<unsigned char>(Nan::To<int32_t>(info[1]).FromMaybe(0));
          bool arg2 = Nan::To<bool>(info[2]).FromMaybe(false);
          bool arg3 = Nan::To<bool>(info[3]).FromMaybe(false);
          
          ::Windows::Devices::Midi2::MidiMessage128^ result;
          result = ::Windows::Devices::Midi2::MidiStreamMessageBuilder::BuildStreamConfigurationRequestMessage(arg0, arg1, arg2, arg3);
          info.GetReturnValue().Set(WrapMidiMessage128(result));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void BuildStreamConfigurationNotificationMessage(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 4
        && info[0]->IsNumber()
        && info[1]->IsInt32()
        && info[2]->IsBoolean()
        && info[3]->IsBoolean())
      {
        try
        {
          unsigned __int64 arg0 = static_cast<unsigned __int64>(Nan::To<int64_t>(info[0]).FromMaybe(0));
          unsigned char arg1 = static_cast<unsigned char>(Nan::To<int32_t>(info[1]).FromMaybe(0));
          bool arg2 = Nan::To<bool>(info[2]).FromMaybe(false);
          bool arg3 = Nan::To<bool>(info[3]).FromMaybe(false);
          
          ::Windows::Devices::Midi2::MidiMessage128^ result;
          result = ::Windows::Devices::Midi2::MidiStreamMessageBuilder::BuildStreamConfigurationNotificationMessage(arg0, arg1, arg2, arg3);
          info.GetReturnValue().Set(WrapMidiMessage128(result));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void BuildFunctionBlockDiscoveryMessage(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 3
        && info[0]->IsNumber()
        && info[1]->IsInt32()
        && info[2]->IsInt32())
      {
        try
        {
          unsigned __int64 arg0 = static_cast<unsigned __int64>(Nan::To<int64_t>(info[0]).FromMaybe(0));
          unsigned char arg1 = static_cast<unsigned char>(Nan::To<int32_t>(info[1]).FromMaybe(0));
          ::Windows::Devices::Midi2::MidiFunctionBlockDiscoveryFilterFlags arg2 = static_cast<::Windows::Devices::Midi2::MidiFunctionBlockDiscoveryFilterFlags>(Nan::To<int32_t>(info[2]).FromMaybe(0));
          
          ::Windows::Devices::Midi2::MidiMessage128^ result;
          result = ::Windows::Devices::Midi2::MidiStreamMessageBuilder::BuildFunctionBlockDiscoveryMessage(arg0, arg1, arg2);
          info.GetReturnValue().Set(WrapMidiMessage128(result));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void BuildFunctionBlockInfoNotificationMessage(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 10
        && info[0]->IsNumber()
        && info[1]->IsBoolean()
        && info[2]->IsInt32()
        && info[3]->IsInt32()
        && info[4]->IsInt32()
        && info[5]->IsInt32()
        && info[6]->IsInt32()
        && info[7]->IsInt32()
        && info[8]->IsInt32()
        && info[9]->IsInt32())
      {
        try
        {
          unsigned __int64 arg0 = static_cast<unsigned __int64>(Nan::To<int64_t>(info[0]).FromMaybe(0));
          bool arg1 = Nan::To<bool>(info[1]).FromMaybe(false);
          unsigned char arg2 = static_cast<unsigned char>(Nan::To<int32_t>(info[2]).FromMaybe(0));
          ::Windows::Devices::Midi2::MidiFunctionBlockUIHint arg3 = static_cast<::Windows::Devices::Midi2::MidiFunctionBlockUIHint>(Nan::To<int32_t>(info[3]).FromMaybe(0));
          ::Windows::Devices::Midi2::MidiFunctionBlockMidi10 arg4 = static_cast<::Windows::Devices::Midi2::MidiFunctionBlockMidi10>(Nan::To<int32_t>(info[4]).FromMaybe(0));
          ::Windows::Devices::Midi2::MidiFunctionBlockDirection arg5 = static_cast<::Windows::Devices::Midi2::MidiFunctionBlockDirection>(Nan::To<int32_t>(info[5]).FromMaybe(0));
          unsigned char arg6 = static_cast<unsigned char>(Nan::To<int32_t>(info[6]).FromMaybe(0));
          unsigned char arg7 = static_cast<unsigned char>(Nan::To<int32_t>(info[7]).FromMaybe(0));
          unsigned char arg8 = static_cast<unsigned char>(Nan::To<int32_t>(info[8]).FromMaybe(0));
          unsigned char arg9 = static_cast<unsigned char>(Nan::To<int32_t>(info[9]).FromMaybe(0));
          
          ::Windows::Devices::Midi2::MidiMessage128^ result;
          result = ::Windows::Devices::Midi2::MidiStreamMessageBuilder::BuildFunctionBlockInfoNotificationMessage(arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9);
          info.GetReturnValue().Set(WrapMidiMessage128(result));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void BuildFunctionBlockNameNotificationMessages(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 3
        && info[0]->IsNumber()
        && info[1]->IsInt32()
        && info[2]->IsString())
      {
        try
        {
          unsigned __int64 arg0 = static_cast<unsigned __int64>(Nan::To<int64_t>(info[0]).FromMaybe(0));
          unsigned char arg1 = static_cast<unsigned char>(Nan::To<int32_t>(info[1]).FromMaybe(0));
          Platform::String^ arg2 = ref new Platform::String(NodeRT::Utils::StringToWchar(v8::String::Value(v8::Isolate::GetCurrent(), info[2])));
          
          ::Windows::Foundation::Collections::IVector<::Windows::Devices::Midi2::MidiMessage128^>^ result;
          result = ::Windows::Devices::Midi2::MidiStreamMessageBuilder::BuildFunctionBlockNameNotificationMessages(arg0, arg1, arg2);
          info.GetReturnValue().Set(NodeRT::Collections::VectorWrapper<::Windows::Devices::Midi2::MidiMessage128^>::CreateVectorWrapper(result, 
            [](::Windows::Devices::Midi2::MidiMessage128^ val) -> Local<Value> {
              return WrapMidiMessage128(val);
            },
            [](Local<Value> value) -> bool {
              return NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessage128^>(value);
            },
            [](Local<Value> value) -> ::Windows::Devices::Midi2::MidiMessage128^ {
              return UnwrapMidiMessage128(value);
            }
          ));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }

    static void ParseFunctionBlockNameNotificationMessages(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (info.Length() == 1
        && (NodeRT::Utils::IsWinRtWrapperOf<::Windows::Foundation::Collections::IVector<::Windows::Devices::Midi2::MidiMessage128^>^>(info[0]) || info[0]->IsArray()))
      {
        try
        {
          ::Windows::Foundation::Collections::IVector<::Windows::Devices::Midi2::MidiMessage128^>^ arg0 = 
            [] (v8::Local<v8::Value> value) -> ::Windows::Foundation::Collections::IVector<::Windows::Devices::Midi2::MidiMessage128^>^
            {
              if (value->IsArray())
              {
                return NodeRT::Collections::JsArrayToWinrtVector<::Windows::Devices::Midi2::MidiMessage128^>(value.As<Array>(), 
                 [](Local<Value> value) -> bool {
                   return NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessage128^>(value);
                 },
                 [](Local<Value> value) -> ::Windows::Devices::Midi2::MidiMessage128^ {
                   return UnwrapMidiMessage128(value);
                 }
                );
              }
              else
              {
                return dynamic_cast<::Windows::Foundation::Collections::IVector<::Windows::Devices::Midi2::MidiMessage128^>^>(NodeRT::Utils::GetObjectInstance(value));
              }
            } (info[0]);
          
          Platform::String^ result;
          result = ::Windows::Devices::Midi2::MidiStreamMessageBuilder::ParseFunctionBlockNameNotificationMessages(arg0);
          info.GetReturnValue().Set(NodeRT::Utils::NewString(result->Data()));
          return;
        }
        catch (Platform::Exception ^exception)
        {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else  {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }



    private:
      ::Windows::Devices::Midi2::MidiStreamMessageBuilder^ _instance;
      static Persistent<FunctionTemplate> s_constructorTemplate;

      friend v8::Local<v8::Value> WrapMidiStreamMessageBuilder(::Windows::Devices::Midi2::MidiStreamMessageBuilder^ wintRtInstance);
      friend ::Windows::Devices::Midi2::MidiStreamMessageBuilder^ UnwrapMidiStreamMessageBuilder(Local<Value> value);
  };

  Persistent<FunctionTemplate> MidiStreamMessageBuilder::s_constructorTemplate;

  v8::Local<v8::Value> WrapMidiStreamMessageBuilder(::Windows::Devices::Midi2::MidiStreamMessageBuilder^ winRtInstance) {
    EscapableHandleScope scope;

    if (winRtInstance == nullptr) {
      return scope.Escape(Undefined());
    }

    Local<Value> opaqueWrapper = CreateOpaqueWrapper(winRtInstance);
    Local<Value> args[] = {opaqueWrapper};
    Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(MidiStreamMessageBuilder::s_constructorTemplate);
    return scope.Escape(Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(),_countof(args), args).ToLocalChecked());
  }

  ::Windows::Devices::Midi2::MidiStreamMessageBuilder^ UnwrapMidiStreamMessageBuilder(Local<Value> value) {
     return MidiStreamMessageBuilder::Unwrap<MidiStreamMessageBuilder>(Nan::To<Object>(value).ToLocalChecked())->_instance;
  }

  void InitMidiStreamMessageBuilder(Local<Object> exports) {
    MidiStreamMessageBuilder::Init(exports);
  }

  class MidiTransportPluginInformation : public WrapperBase {
    public:
      
      static void Init(const Local<Object> exports) {
        HandleScope scope;

        Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(New);
        s_constructorTemplate.Reset(localRef);
        localRef->SetClassName(Nan::New<String>("MidiTransportPluginInformation").ToLocalChecked());
        localRef->InstanceTemplate()->SetInternalFieldCount(1);





          
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("author").ToLocalChecked(), AuthorGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("classId").ToLocalChecked(), ClassIdGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("clientConfigurationAssemblyName").ToLocalChecked(), ClientConfigurationAssemblyNameGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("description").ToLocalChecked(), DescriptionGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("iconPath").ToLocalChecked(), IconPathGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("isClientConfigurable").ToLocalChecked(), IsClientConfigurableGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("isEnabled").ToLocalChecked(), IsEnabledGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("isRuntimeCreatable").ToLocalChecked(), IsRuntimeCreatableGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("isSystemManaged").ToLocalChecked(), IsSystemManagedGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("mnemonic").ToLocalChecked(), MnemonicGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("name").ToLocalChecked(), NameGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("registryKey").ToLocalChecked(), RegistryKeyGetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("shortName").ToLocalChecked(), ShortNameGetter);

        Local<Object> constructor = Nan::To<Object>(Nan::GetFunction(localRef).ToLocalChecked()).ToLocalChecked();
        Nan::SetMethod(constructor, "castFrom", CastFrom);



        Nan::Set(exports, Nan::New<String>("MidiTransportPluginInformation").ToLocalChecked(), constructor);
      }

      virtual ::Platform::Object^ GetObjectInstance() const override {
        return _instance;
      }

    private:

      MidiTransportPluginInformation(::Windows::Devices::Midi2::MidiTransportPluginInformation^ instance) {
        _instance = instance;
      }

      
    static void New(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(s_constructorTemplate);

      // in case the constructor was called without the new operator
      if (!localRef->HasInstance(info.This())) {
        if (info.Length() > 0) {
          std::unique_ptr<Local<Value> []> constructorArgs(new Local<Value>[info.Length()]);

          Local<Value> *argsPtr = constructorArgs.get();
          for (int i = 0; i < info.Length(); i++) {
            argsPtr[i] = info[i];
          }

          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), constructorArgs.get());
          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        } else {
          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), nullptr);

          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        }
      }

      ::Windows::Devices::Midi2::MidiTransportPluginInformation^ winRtInstance;


      if (info.Length() == 1 && OpaqueWrapper::IsOpaqueWrapper(info[0]) &&
        NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiTransportPluginInformation^>(info[0])) {
        try {
          winRtInstance = (::Windows::Devices::Midi2::MidiTransportPluginInformation^) NodeRT::Utils::GetObjectInstance(info[0]);
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no suitable constructor found")));
        return;
      }

      NodeRT::Utils::SetHiddenValue(info.This(), Nan::New<String>("__winRtInstance__").ToLocalChecked(), True());

      MidiTransportPluginInformation *wrapperInstance = new MidiTransportPluginInformation(winRtInstance);
      wrapperInstance->Wrap(info.This());

      info.GetReturnValue().Set(info.This());
    }


      
    static void CastFrom(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;
      if (info.Length() < 1 || !NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiTransportPluginInformation^>(info[0])) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no object provided, or given object could not be casted to requested type")));
        return;
      }

      ::Windows::Devices::Midi2::MidiTransportPluginInformation^ winRtInstance;
      try {
        winRtInstance = (::Windows::Devices::Midi2::MidiTransportPluginInformation^) NodeRT::Utils::GetObjectInstance(info[0]);
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }

      info.GetReturnValue().Set(WrapMidiTransportPluginInformation(winRtInstance));
    }





    static void AuthorGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiTransportPluginInformation^>(info.This())) {
        return;
      }

      MidiTransportPluginInformation *wrapper = MidiTransportPluginInformation::Unwrap<MidiTransportPluginInformation>(info.This());

      try  {
        Platform::String^ result = wrapper->_instance->Author;
        info.GetReturnValue().Set(NodeRT::Utils::NewString(result->Data()));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void ClassIdGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiTransportPluginInformation^>(info.This())) {
        return;
      }

      MidiTransportPluginInformation *wrapper = MidiTransportPluginInformation::Unwrap<MidiTransportPluginInformation>(info.This());

      try  {
        ::Platform::Guid result = wrapper->_instance->ClassId;
        info.GetReturnValue().Set(NodeRT::Utils::GuidToJs(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void ClientConfigurationAssemblyNameGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiTransportPluginInformation^>(info.This())) {
        return;
      }

      MidiTransportPluginInformation *wrapper = MidiTransportPluginInformation::Unwrap<MidiTransportPluginInformation>(info.This());

      try  {
        Platform::String^ result = wrapper->_instance->ClientConfigurationAssemblyName;
        info.GetReturnValue().Set(NodeRT::Utils::NewString(result->Data()));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void DescriptionGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiTransportPluginInformation^>(info.This())) {
        return;
      }

      MidiTransportPluginInformation *wrapper = MidiTransportPluginInformation::Unwrap<MidiTransportPluginInformation>(info.This());

      try  {
        Platform::String^ result = wrapper->_instance->Description;
        info.GetReturnValue().Set(NodeRT::Utils::NewString(result->Data()));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void IconPathGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiTransportPluginInformation^>(info.This())) {
        return;
      }

      MidiTransportPluginInformation *wrapper = MidiTransportPluginInformation::Unwrap<MidiTransportPluginInformation>(info.This());

      try  {
        Platform::String^ result = wrapper->_instance->IconPath;
        info.GetReturnValue().Set(NodeRT::Utils::NewString(result->Data()));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void IsClientConfigurableGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiTransportPluginInformation^>(info.This())) {
        return;
      }

      MidiTransportPluginInformation *wrapper = MidiTransportPluginInformation::Unwrap<MidiTransportPluginInformation>(info.This());

      try  {
        bool result = wrapper->_instance->IsClientConfigurable;
        info.GetReturnValue().Set(Nan::New<Boolean>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void IsEnabledGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiTransportPluginInformation^>(info.This())) {
        return;
      }

      MidiTransportPluginInformation *wrapper = MidiTransportPluginInformation::Unwrap<MidiTransportPluginInformation>(info.This());

      try  {
        bool result = wrapper->_instance->IsEnabled;
        info.GetReturnValue().Set(Nan::New<Boolean>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void IsRuntimeCreatableGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiTransportPluginInformation^>(info.This())) {
        return;
      }

      MidiTransportPluginInformation *wrapper = MidiTransportPluginInformation::Unwrap<MidiTransportPluginInformation>(info.This());

      try  {
        bool result = wrapper->_instance->IsRuntimeCreatable;
        info.GetReturnValue().Set(Nan::New<Boolean>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void IsSystemManagedGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiTransportPluginInformation^>(info.This())) {
        return;
      }

      MidiTransportPluginInformation *wrapper = MidiTransportPluginInformation::Unwrap<MidiTransportPluginInformation>(info.This());

      try  {
        bool result = wrapper->_instance->IsSystemManaged;
        info.GetReturnValue().Set(Nan::New<Boolean>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void MnemonicGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiTransportPluginInformation^>(info.This())) {
        return;
      }

      MidiTransportPluginInformation *wrapper = MidiTransportPluginInformation::Unwrap<MidiTransportPluginInformation>(info.This());

      try  {
        Platform::String^ result = wrapper->_instance->Mnemonic;
        info.GetReturnValue().Set(NodeRT::Utils::NewString(result->Data()));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void NameGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiTransportPluginInformation^>(info.This())) {
        return;
      }

      MidiTransportPluginInformation *wrapper = MidiTransportPluginInformation::Unwrap<MidiTransportPluginInformation>(info.This());

      try  {
        Platform::String^ result = wrapper->_instance->Name;
        info.GetReturnValue().Set(NodeRT::Utils::NewString(result->Data()));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void RegistryKeyGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiTransportPluginInformation^>(info.This())) {
        return;
      }

      MidiTransportPluginInformation *wrapper = MidiTransportPluginInformation::Unwrap<MidiTransportPluginInformation>(info.This());

      try  {
        Platform::String^ result = wrapper->_instance->RegistryKey;
        info.GetReturnValue().Set(NodeRT::Utils::NewString(result->Data()));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void ShortNameGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiTransportPluginInformation^>(info.This())) {
        return;
      }

      MidiTransportPluginInformation *wrapper = MidiTransportPluginInformation::Unwrap<MidiTransportPluginInformation>(info.This());

      try  {
        Platform::String^ result = wrapper->_instance->ShortName;
        info.GetReturnValue().Set(NodeRT::Utils::NewString(result->Data()));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      


    private:
      ::Windows::Devices::Midi2::MidiTransportPluginInformation^ _instance;
      static Persistent<FunctionTemplate> s_constructorTemplate;

      friend v8::Local<v8::Value> WrapMidiTransportPluginInformation(::Windows::Devices::Midi2::MidiTransportPluginInformation^ wintRtInstance);
      friend ::Windows::Devices::Midi2::MidiTransportPluginInformation^ UnwrapMidiTransportPluginInformation(Local<Value> value);
  };

  Persistent<FunctionTemplate> MidiTransportPluginInformation::s_constructorTemplate;

  v8::Local<v8::Value> WrapMidiTransportPluginInformation(::Windows::Devices::Midi2::MidiTransportPluginInformation^ winRtInstance) {
    EscapableHandleScope scope;

    if (winRtInstance == nullptr) {
      return scope.Escape(Undefined());
    }

    Local<Value> opaqueWrapper = CreateOpaqueWrapper(winRtInstance);
    Local<Value> args[] = {opaqueWrapper};
    Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(MidiTransportPluginInformation::s_constructorTemplate);
    return scope.Escape(Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(),_countof(args), args).ToLocalChecked());
  }

  ::Windows::Devices::Midi2::MidiTransportPluginInformation^ UnwrapMidiTransportPluginInformation(Local<Value> value) {
     return MidiTransportPluginInformation::Unwrap<MidiTransportPluginInformation>(Nan::To<Object>(value).ToLocalChecked())->_instance;
  }

  void InitMidiTransportPluginInformation(Local<Object> exports) {
    MidiTransportPluginInformation::Init(exports);
  }

  class MidiUniqueId : public WrapperBase {
    public:
      
      static void Init(const Local<Object> exports) {
        HandleScope scope;

        Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(New);
        s_constructorTemplate.Reset(localRef);
        localRef->SetClassName(Nan::New<String>("MidiUniqueId").ToLocalChecked());
        localRef->InstanceTemplate()->SetInternalFieldCount(1);





          
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("byte4").ToLocalChecked(), Byte4Getter, Byte4Setter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("byte3").ToLocalChecked(), Byte3Getter, Byte3Setter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("byte2").ToLocalChecked(), Byte2Getter, Byte2Setter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("byte1").ToLocalChecked(), Byte1Getter, Byte1Setter);

        Local<Object> constructor = Nan::To<Object>(Nan::GetFunction(localRef).ToLocalChecked()).ToLocalChecked();
        Nan::SetMethod(constructor, "castFrom", CastFrom);

        Nan::SetAccessor(constructor, Nan::New<String>("labelFull").ToLocalChecked(), LabelFullGetter);
        Nan::SetAccessor(constructor, Nan::New<String>("labelShort").ToLocalChecked(), LabelShortGetter);


        Nan::Set(exports, Nan::New<String>("MidiUniqueId").ToLocalChecked(), constructor);
      }

      virtual ::Platform::Object^ GetObjectInstance() const override {
        return _instance;
      }

    private:

      MidiUniqueId(::Windows::Devices::Midi2::MidiUniqueId^ instance) {
        _instance = instance;
      }

      
    static void New(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(s_constructorTemplate);

      // in case the constructor was called without the new operator
      if (!localRef->HasInstance(info.This())) {
        if (info.Length() > 0) {
          std::unique_ptr<Local<Value> []> constructorArgs(new Local<Value>[info.Length()]);

          Local<Value> *argsPtr = constructorArgs.get();
          for (int i = 0; i < info.Length(); i++) {
            argsPtr[i] = info[i];
          }

          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), constructorArgs.get());
          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        } else {
          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), nullptr);

          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        }
      }

      ::Windows::Devices::Midi2::MidiUniqueId^ winRtInstance;


      if (info.Length() == 1 && OpaqueWrapper::IsOpaqueWrapper(info[0]) &&
        NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiUniqueId^>(info[0])) {
        try {
          winRtInstance = (::Windows::Devices::Midi2::MidiUniqueId^) NodeRT::Utils::GetObjectInstance(info[0]);
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
      else if (info.Length() == 4
        && info[0]->IsInt32()
        && info[1]->IsInt32()
        && info[2]->IsInt32()
        && info[3]->IsInt32())
      {
        try {
          unsigned char arg0 = static_cast<unsigned char>(Nan::To<int32_t>(info[0]).FromMaybe(0));
          unsigned char arg1 = static_cast<unsigned char>(Nan::To<int32_t>(info[1]).FromMaybe(0));
          unsigned char arg2 = static_cast<unsigned char>(Nan::To<int32_t>(info[2]).FromMaybe(0));
          unsigned char arg3 = static_cast<unsigned char>(Nan::To<int32_t>(info[3]).FromMaybe(0));
          
          winRtInstance = ref new ::Windows::Devices::Midi2::MidiUniqueId(arg0,arg1,arg2,arg3);
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
      else if (info.Length() == 0)
      {
        try {
          winRtInstance = ref new ::Windows::Devices::Midi2::MidiUniqueId();
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no suitable constructor found")));
        return;
      }

      NodeRT::Utils::SetHiddenValue(info.This(), Nan::New<String>("__winRtInstance__").ToLocalChecked(), True());

      MidiUniqueId *wrapperInstance = new MidiUniqueId(winRtInstance);
      wrapperInstance->Wrap(info.This());

      info.GetReturnValue().Set(info.This());
    }


      
    static void CastFrom(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;
      if (info.Length() < 1 || !NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiUniqueId^>(info[0])) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no object provided, or given object could not be casted to requested type")));
        return;
      }

      ::Windows::Devices::Midi2::MidiUniqueId^ winRtInstance;
      try {
        winRtInstance = (::Windows::Devices::Midi2::MidiUniqueId^) NodeRT::Utils::GetObjectInstance(info[0]);
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }

      info.GetReturnValue().Set(WrapMidiUniqueId(winRtInstance));
    }





    static void Byte4Getter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiUniqueId^>(info.This())) {
        return;
      }

      MidiUniqueId *wrapper = MidiUniqueId::Unwrap<MidiUniqueId>(info.This());

      try  {
        unsigned char result = wrapper->_instance->Byte4;
        info.GetReturnValue().Set(Nan::New<Integer>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void Byte4Setter(Local<String> property, Local<Value> value, const Nan::PropertyCallbackInfo<void> &info) {
      HandleScope scope;

      if (!value->IsInt32()) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Value to set is of unexpected type")));
        return;
      }

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiUniqueId^>(info.This())) {
        return;
      }

      MidiUniqueId *wrapper = MidiUniqueId::Unwrap<MidiUniqueId>(info.This());

      try {

        unsigned char winRtValue = static_cast<unsigned char>(Nan::To<int32_t>(value).FromMaybe(0));

        wrapper->_instance->Byte4 = winRtValue;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
      }
    }
      
    static void Byte3Getter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiUniqueId^>(info.This())) {
        return;
      }

      MidiUniqueId *wrapper = MidiUniqueId::Unwrap<MidiUniqueId>(info.This());

      try  {
        unsigned char result = wrapper->_instance->Byte3;
        info.GetReturnValue().Set(Nan::New<Integer>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void Byte3Setter(Local<String> property, Local<Value> value, const Nan::PropertyCallbackInfo<void> &info) {
      HandleScope scope;

      if (!value->IsInt32()) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Value to set is of unexpected type")));
        return;
      }

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiUniqueId^>(info.This())) {
        return;
      }

      MidiUniqueId *wrapper = MidiUniqueId::Unwrap<MidiUniqueId>(info.This());

      try {

        unsigned char winRtValue = static_cast<unsigned char>(Nan::To<int32_t>(value).FromMaybe(0));

        wrapper->_instance->Byte3 = winRtValue;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
      }
    }
      
    static void Byte2Getter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiUniqueId^>(info.This())) {
        return;
      }

      MidiUniqueId *wrapper = MidiUniqueId::Unwrap<MidiUniqueId>(info.This());

      try  {
        unsigned char result = wrapper->_instance->Byte2;
        info.GetReturnValue().Set(Nan::New<Integer>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void Byte2Setter(Local<String> property, Local<Value> value, const Nan::PropertyCallbackInfo<void> &info) {
      HandleScope scope;

      if (!value->IsInt32()) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Value to set is of unexpected type")));
        return;
      }

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiUniqueId^>(info.This())) {
        return;
      }

      MidiUniqueId *wrapper = MidiUniqueId::Unwrap<MidiUniqueId>(info.This());

      try {

        unsigned char winRtValue = static_cast<unsigned char>(Nan::To<int32_t>(value).FromMaybe(0));

        wrapper->_instance->Byte2 = winRtValue;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
      }
    }
      
    static void Byte1Getter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiUniqueId^>(info.This())) {
        return;
      }

      MidiUniqueId *wrapper = MidiUniqueId::Unwrap<MidiUniqueId>(info.This());

      try  {
        unsigned char result = wrapper->_instance->Byte1;
        info.GetReturnValue().Set(Nan::New<Integer>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void Byte1Setter(Local<String> property, Local<Value> value, const Nan::PropertyCallbackInfo<void> &info) {
      HandleScope scope;

      if (!value->IsInt32()) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Value to set is of unexpected type")));
        return;
      }

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiUniqueId^>(info.This())) {
        return;
      }

      MidiUniqueId *wrapper = MidiUniqueId::Unwrap<MidiUniqueId>(info.This());

      try {

        unsigned char winRtValue = static_cast<unsigned char>(Nan::To<int32_t>(value).FromMaybe(0));

        wrapper->_instance->Byte1 = winRtValue;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
      }
    }
      


    static void LabelFullGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      try
      {
        Platform::String^ result = ::Windows::Devices::Midi2::MidiUniqueId::LabelFull;
        info.GetReturnValue().Set(NodeRT::Utils::NewString(result->Data()));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      

    static void LabelShortGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      try
      {
        Platform::String^ result = ::Windows::Devices::Midi2::MidiUniqueId::LabelShort;
        info.GetReturnValue().Set(NodeRT::Utils::NewString(result->Data()));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      

    private:
      ::Windows::Devices::Midi2::MidiUniqueId^ _instance;
      static Persistent<FunctionTemplate> s_constructorTemplate;

      friend v8::Local<v8::Value> WrapMidiUniqueId(::Windows::Devices::Midi2::MidiUniqueId^ wintRtInstance);
      friend ::Windows::Devices::Midi2::MidiUniqueId^ UnwrapMidiUniqueId(Local<Value> value);
  };

  Persistent<FunctionTemplate> MidiUniqueId::s_constructorTemplate;

  v8::Local<v8::Value> WrapMidiUniqueId(::Windows::Devices::Midi2::MidiUniqueId^ winRtInstance) {
    EscapableHandleScope scope;

    if (winRtInstance == nullptr) {
      return scope.Escape(Undefined());
    }

    Local<Value> opaqueWrapper = CreateOpaqueWrapper(winRtInstance);
    Local<Value> args[] = {opaqueWrapper};
    Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(MidiUniqueId::s_constructorTemplate);
    return scope.Escape(Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(),_countof(args), args).ToLocalChecked());
  }

  ::Windows::Devices::Midi2::MidiUniqueId^ UnwrapMidiUniqueId(Local<Value> value) {
     return MidiUniqueId::Unwrap<MidiUniqueId>(Nan::To<Object>(value).ToLocalChecked())->_instance;
  }

  void InitMidiUniqueId(Local<Object> exports) {
    MidiUniqueId::Init(exports);
  }

  class MidiVirtualDevice : public WrapperBase {
    public:
      
      static void Init(const Local<Object> exports) {
        HandleScope scope;

        Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(New);
        s_constructorTemplate.Reset(localRef);
        localRef->SetClassName(Nan::New<String>("MidiVirtualDevice").ToLocalChecked());
        localRef->InstanceTemplate()->SetInternalFieldCount(1);


          
            Nan::SetPrototypeMethod(localRef, "addFunctionBlock", AddFunctionBlock);
            Nan::SetPrototypeMethod(localRef, "updateFunctionBlock", UpdateFunctionBlock);
            Nan::SetPrototypeMethod(localRef, "removeFunctionBlock", RemoveFunctionBlock);
            Nan::SetPrototypeMethod(localRef, "initialize", Initialize);
            Nan::SetPrototypeMethod(localRef, "onEndpointConnectionOpened", OnEndpointConnectionOpened);
            Nan::SetPrototypeMethod(localRef, "processIncomingMessage", ProcessIncomingMessage);
            Nan::SetPrototypeMethod(localRef, "cleanup", Cleanup);
          



          
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("tag").ToLocalChecked(), TagGetter, TagSetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("name").ToLocalChecked(), NameGetter, NameSetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("isEnabled").ToLocalChecked(), IsEnabledGetter, IsEnabledSetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("id").ToLocalChecked(), IdGetter, IdSetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("suppressHandledMessages").ToLocalChecked(), SuppressHandledMessagesGetter, SuppressHandledMessagesSetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("endpointProductInstanceId").ToLocalChecked(), EndpointProductInstanceIdGetter, EndpointProductInstanceIdSetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("endpointName").ToLocalChecked(), EndpointNameGetter, EndpointNameSetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("areFunctionBlocksStatic").ToLocalChecked(), AreFunctionBlocksStaticGetter, AreFunctionBlocksStaticSetter);
            Nan::SetAccessor(localRef->PrototypeTemplate(), Nan::New<String>("functionBlocks").ToLocalChecked(), FunctionBlocksGetter);

        Local<Object> constructor = Nan::To<Object>(Nan::GetFunction(localRef).ToLocalChecked()).ToLocalChecked();
        Nan::SetMethod(constructor, "castFrom", CastFrom);



        Nan::Set(exports, Nan::New<String>("MidiVirtualDevice").ToLocalChecked(), constructor);
      }

      virtual ::Platform::Object^ GetObjectInstance() const override {
        return _instance;
      }

    private:

      MidiVirtualDevice(::Windows::Devices::Midi2::MidiVirtualDevice^ instance) {
        _instance = instance;
      }

      
    static void New(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(s_constructorTemplate);

      // in case the constructor was called without the new operator
      if (!localRef->HasInstance(info.This())) {
        if (info.Length() > 0) {
          std::unique_ptr<Local<Value> []> constructorArgs(new Local<Value>[info.Length()]);

          Local<Value> *argsPtr = constructorArgs.get();
          for (int i = 0; i < info.Length(); i++) {
            argsPtr[i] = info[i];
          }

          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), constructorArgs.get());
          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        } else {
          MaybeLocal<Object> res = Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(), info.Length(), nullptr);

          if (res.IsEmpty()) {
            return;
          }

          info.GetReturnValue().Set(res.ToLocalChecked());
          return;
        }
      }

      ::Windows::Devices::Midi2::MidiVirtualDevice^ winRtInstance;


      if (info.Length() == 1 && OpaqueWrapper::IsOpaqueWrapper(info[0]) &&
        NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiVirtualDevice^>(info[0])) {
        try {
          winRtInstance = (::Windows::Devices::Midi2::MidiVirtualDevice^) NodeRT::Utils::GetObjectInstance(info[0]);
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
      else if (info.Length() == 0)
      {
        try {
          winRtInstance = ref new ::Windows::Devices::Midi2::MidiVirtualDevice();
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no suitable constructor found")));
        return;
      }

      NodeRT::Utils::SetHiddenValue(info.This(), Nan::New<String>("__winRtInstance__").ToLocalChecked(), True());

      MidiVirtualDevice *wrapperInstance = new MidiVirtualDevice(winRtInstance);
      wrapperInstance->Wrap(info.This());

      info.GetReturnValue().Set(info.This());
    }


      
    static void CastFrom(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;
      if (info.Length() < 1 || !NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiVirtualDevice^>(info[0])) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Invalid arguments, no object provided, or given object could not be casted to requested type")));
        return;
      }

      ::Windows::Devices::Midi2::MidiVirtualDevice^ winRtInstance;
      try {
        winRtInstance = (::Windows::Devices::Midi2::MidiVirtualDevice^) NodeRT::Utils::GetObjectInstance(info[0]);
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }

      info.GetReturnValue().Set(WrapMidiVirtualDevice(winRtInstance));
    }


    static void AddFunctionBlock(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiVirtualDevice^>(info.This())) {
        return;
      }

      MidiVirtualDevice *wrapper = MidiVirtualDevice::Unwrap<MidiVirtualDevice>(info.This());

      if (info.Length() == 1
        && NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiFunctionBlock^>(info[0]))
      {
        try
        {
          ::Windows::Devices::Midi2::MidiFunctionBlock^ arg0 = UnwrapMidiFunctionBlock(info[0]);
          
          bool result;
          result = wrapper->_instance->AddFunctionBlock(arg0);
          info.GetReturnValue().Set(Nan::New<Boolean>(result));
          return;
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }
    static void UpdateFunctionBlock(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiVirtualDevice^>(info.This())) {
        return;
      }

      MidiVirtualDevice *wrapper = MidiVirtualDevice::Unwrap<MidiVirtualDevice>(info.This());

      if (info.Length() == 1
        && NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiFunctionBlock^>(info[0]))
      {
        try
        {
          ::Windows::Devices::Midi2::MidiFunctionBlock^ arg0 = UnwrapMidiFunctionBlock(info[0]);
          
          wrapper->_instance->UpdateFunctionBlock(arg0);
          return;
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }
    static void RemoveFunctionBlock(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiVirtualDevice^>(info.This())) {
        return;
      }

      MidiVirtualDevice *wrapper = MidiVirtualDevice::Unwrap<MidiVirtualDevice>(info.This());

      if (info.Length() == 1
        && info[0]->IsInt32())
      {
        try
        {
          unsigned char arg0 = static_cast<unsigned char>(Nan::To<int32_t>(info[0]).FromMaybe(0));
          
          wrapper->_instance->RemoveFunctionBlock(arg0);
          return;
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }
    static void Initialize(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiVirtualDevice^>(info.This())) {
        return;
      }

      MidiVirtualDevice *wrapper = MidiVirtualDevice::Unwrap<MidiVirtualDevice>(info.This());

      if (info.Length() == 1
        && NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::IMidiEndpointConnectionSource^>(info[0]))
      {
        try
        {
          ::Windows::Devices::Midi2::IMidiEndpointConnectionSource^ arg0 = UnwrapIMidiEndpointConnectionSource(info[0]);
          
          wrapper->_instance->Initialize(arg0);
          return;
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }
    static void OnEndpointConnectionOpened(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiVirtualDevice^>(info.This())) {
        return;
      }

      MidiVirtualDevice *wrapper = MidiVirtualDevice::Unwrap<MidiVirtualDevice>(info.This());

      if (info.Length() == 0)
      {
        try
        {
          wrapper->_instance->OnEndpointConnectionOpened();
          return;
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }
    static void ProcessIncomingMessage(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiVirtualDevice^>(info.This())) {
        return;
      }

      MidiVirtualDevice *wrapper = MidiVirtualDevice::Unwrap<MidiVirtualDevice>(info.This());

      if (info.Length() == 1
        && NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiMessageReceivedEventArgs^>(info[0]))
      {
        try
        {
          ::Windows::Devices::Midi2::MidiMessageReceivedEventArgs^ arg0 = UnwrapMidiMessageReceivedEventArgs(info[0]);
          bool arg1;
          bool arg2;
          
          wrapper->_instance->ProcessIncomingMessage(arg0, &arg1, &arg2);
          Local<Object> resObj = Nan::New<Object>();
          Nan::Set(resObj, Nan::New<String>("skipFurtherListeners").ToLocalChecked(), Nan::New<Boolean>(arg1));
          Nan::Set(resObj, Nan::New<String>("skipMainMessageReceivedEvent").ToLocalChecked(), Nan::New<Boolean>(arg2));
          info.GetReturnValue().Set(resObj);
          return;
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }
    static void Cleanup(Nan::NAN_METHOD_ARGS_TYPE info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiVirtualDevice^>(info.This())) {
        return;
      }

      MidiVirtualDevice *wrapper = MidiVirtualDevice::Unwrap<MidiVirtualDevice>(info.This());

      if (info.Length() == 0)
      {
        try
        {
          wrapper->_instance->Cleanup();
          return;
        } catch (Platform::Exception ^exception) {
          NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
          return;
        }
      }
 else {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Bad arguments: no suitable overload found")));
        return;
      }
    }



    static void TagGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiVirtualDevice^>(info.This())) {
        return;
      }

      MidiVirtualDevice *wrapper = MidiVirtualDevice::Unwrap<MidiVirtualDevice>(info.This());

      try  {
        ::Platform::Object^ result = wrapper->_instance->Tag;
        info.GetReturnValue().Set(CreateOpaqueWrapper(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void TagSetter(Local<String> property, Local<Value> value, const Nan::PropertyCallbackInfo<void> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Platform::Object^>(value)) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Value to set is of unexpected type")));
        return;
      }

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiVirtualDevice^>(info.This())) {
        return;
      }

      MidiVirtualDevice *wrapper = MidiVirtualDevice::Unwrap<MidiVirtualDevice>(info.This());

      try {

        ::Platform::Object^ winRtValue = dynamic_cast<::Platform::Object^>(NodeRT::Utils::GetObjectInstance(value));

        wrapper->_instance->Tag = winRtValue;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
      }
    }
      
    static void NameGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiVirtualDevice^>(info.This())) {
        return;
      }

      MidiVirtualDevice *wrapper = MidiVirtualDevice::Unwrap<MidiVirtualDevice>(info.This());

      try  {
        Platform::String^ result = wrapper->_instance->Name;
        info.GetReturnValue().Set(NodeRT::Utils::NewString(result->Data()));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void NameSetter(Local<String> property, Local<Value> value, const Nan::PropertyCallbackInfo<void> &info) {
      HandleScope scope;

      if (!value->IsString()) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Value to set is of unexpected type")));
        return;
      }

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiVirtualDevice^>(info.This())) {
        return;
      }

      MidiVirtualDevice *wrapper = MidiVirtualDevice::Unwrap<MidiVirtualDevice>(info.This());

      try {

        Platform::String^ winRtValue = ref new Platform::String(NodeRT::Utils::StringToWchar(v8::String::Value(v8::Isolate::GetCurrent(), value)));

        wrapper->_instance->Name = winRtValue;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
      }
    }
      
    static void IsEnabledGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiVirtualDevice^>(info.This())) {
        return;
      }

      MidiVirtualDevice *wrapper = MidiVirtualDevice::Unwrap<MidiVirtualDevice>(info.This());

      try  {
        bool result = wrapper->_instance->IsEnabled;
        info.GetReturnValue().Set(Nan::New<Boolean>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void IsEnabledSetter(Local<String> property, Local<Value> value, const Nan::PropertyCallbackInfo<void> &info) {
      HandleScope scope;

      if (!value->IsBoolean()) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Value to set is of unexpected type")));
        return;
      }

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiVirtualDevice^>(info.This())) {
        return;
      }

      MidiVirtualDevice *wrapper = MidiVirtualDevice::Unwrap<MidiVirtualDevice>(info.This());

      try {

        bool winRtValue = Nan::To<bool>(value).FromMaybe(false);

        wrapper->_instance->IsEnabled = winRtValue;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
      }
    }
      
    static void IdGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiVirtualDevice^>(info.This())) {
        return;
      }

      MidiVirtualDevice *wrapper = MidiVirtualDevice::Unwrap<MidiVirtualDevice>(info.This());

      try  {
        Platform::String^ result = wrapper->_instance->Id;
        info.GetReturnValue().Set(NodeRT::Utils::NewString(result->Data()));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void IdSetter(Local<String> property, Local<Value> value, const Nan::PropertyCallbackInfo<void> &info) {
      HandleScope scope;

      if (!value->IsString()) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Value to set is of unexpected type")));
        return;
      }

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiVirtualDevice^>(info.This())) {
        return;
      }

      MidiVirtualDevice *wrapper = MidiVirtualDevice::Unwrap<MidiVirtualDevice>(info.This());

      try {

        Platform::String^ winRtValue = ref new Platform::String(NodeRT::Utils::StringToWchar(v8::String::Value(v8::Isolate::GetCurrent(), value)));

        wrapper->_instance->Id = winRtValue;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
      }
    }
      
    static void SuppressHandledMessagesGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiVirtualDevice^>(info.This())) {
        return;
      }

      MidiVirtualDevice *wrapper = MidiVirtualDevice::Unwrap<MidiVirtualDevice>(info.This());

      try  {
        bool result = wrapper->_instance->SuppressHandledMessages;
        info.GetReturnValue().Set(Nan::New<Boolean>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void SuppressHandledMessagesSetter(Local<String> property, Local<Value> value, const Nan::PropertyCallbackInfo<void> &info) {
      HandleScope scope;

      if (!value->IsBoolean()) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Value to set is of unexpected type")));
        return;
      }

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiVirtualDevice^>(info.This())) {
        return;
      }

      MidiVirtualDevice *wrapper = MidiVirtualDevice::Unwrap<MidiVirtualDevice>(info.This());

      try {

        bool winRtValue = Nan::To<bool>(value).FromMaybe(false);

        wrapper->_instance->SuppressHandledMessages = winRtValue;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
      }
    }
      
    static void EndpointProductInstanceIdGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiVirtualDevice^>(info.This())) {
        return;
      }

      MidiVirtualDevice *wrapper = MidiVirtualDevice::Unwrap<MidiVirtualDevice>(info.This());

      try  {
        Platform::String^ result = wrapper->_instance->EndpointProductInstanceId;
        info.GetReturnValue().Set(NodeRT::Utils::NewString(result->Data()));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void EndpointProductInstanceIdSetter(Local<String> property, Local<Value> value, const Nan::PropertyCallbackInfo<void> &info) {
      HandleScope scope;

      if (!value->IsString()) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Value to set is of unexpected type")));
        return;
      }

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiVirtualDevice^>(info.This())) {
        return;
      }

      MidiVirtualDevice *wrapper = MidiVirtualDevice::Unwrap<MidiVirtualDevice>(info.This());

      try {

        Platform::String^ winRtValue = ref new Platform::String(NodeRT::Utils::StringToWchar(v8::String::Value(v8::Isolate::GetCurrent(), value)));

        wrapper->_instance->EndpointProductInstanceId = winRtValue;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
      }
    }
      
    static void EndpointNameGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiVirtualDevice^>(info.This())) {
        return;
      }

      MidiVirtualDevice *wrapper = MidiVirtualDevice::Unwrap<MidiVirtualDevice>(info.This());

      try  {
        Platform::String^ result = wrapper->_instance->EndpointName;
        info.GetReturnValue().Set(NodeRT::Utils::NewString(result->Data()));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void EndpointNameSetter(Local<String> property, Local<Value> value, const Nan::PropertyCallbackInfo<void> &info) {
      HandleScope scope;

      if (!value->IsString()) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Value to set is of unexpected type")));
        return;
      }

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiVirtualDevice^>(info.This())) {
        return;
      }

      MidiVirtualDevice *wrapper = MidiVirtualDevice::Unwrap<MidiVirtualDevice>(info.This());

      try {

        Platform::String^ winRtValue = ref new Platform::String(NodeRT::Utils::StringToWchar(v8::String::Value(v8::Isolate::GetCurrent(), value)));

        wrapper->_instance->EndpointName = winRtValue;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
      }
    }
      
    static void AreFunctionBlocksStaticGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiVirtualDevice^>(info.This())) {
        return;
      }

      MidiVirtualDevice *wrapper = MidiVirtualDevice::Unwrap<MidiVirtualDevice>(info.This());

      try  {
        bool result = wrapper->_instance->AreFunctionBlocksStatic;
        info.GetReturnValue().Set(Nan::New<Boolean>(result));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      
    static void AreFunctionBlocksStaticSetter(Local<String> property, Local<Value> value, const Nan::PropertyCallbackInfo<void> &info) {
      HandleScope scope;

      if (!value->IsBoolean()) {
        Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"Value to set is of unexpected type")));
        return;
      }

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiVirtualDevice^>(info.This())) {
        return;
      }

      MidiVirtualDevice *wrapper = MidiVirtualDevice::Unwrap<MidiVirtualDevice>(info.This());

      try {

        bool winRtValue = Nan::To<bool>(value).FromMaybe(false);

        wrapper->_instance->AreFunctionBlocksStatic = winRtValue;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
      }
    }
      
    static void FunctionBlocksGetter(Local<String> property, const Nan::PropertyCallbackInfo<v8::Value> &info) {
      HandleScope scope;

      if (!NodeRT::Utils::IsWinRtWrapperOf<::Windows::Devices::Midi2::MidiVirtualDevice^>(info.This())) {
        return;
      }

      MidiVirtualDevice *wrapper = MidiVirtualDevice::Unwrap<MidiVirtualDevice>(info.This());

      try  {
        ::Windows::Foundation::Collections::IMapView<unsigned char, ::Windows::Devices::Midi2::MidiFunctionBlock^>^ result = wrapper->_instance->FunctionBlocks;
        info.GetReturnValue().Set(NodeRT::Collections::MapViewWrapper<unsigned char,::Windows::Devices::Midi2::MidiFunctionBlock^>::CreateMapViewWrapper(result, 
            [](unsigned char val) -> Local<Value> {
              return Nan::New<Integer>(val);
            },
            [](Local<Value> value) -> bool {
              return value->IsInt32();
            },
            [](Local<Value> value) -> unsigned char {
              return static_cast<unsigned char>(Nan::To<int32_t>(value).FromMaybe(0));
            },
            [](::Windows::Devices::Midi2::MidiFunctionBlock^ val) -> Local<Value> {
              return WrapMidiFunctionBlock(val);
            }
          ));
        return;
      } catch (Platform::Exception ^exception) {
        NodeRT::Utils::ThrowWinRtExceptionInJs(exception);
        return;
      }
    }
      


    private:
      ::Windows::Devices::Midi2::MidiVirtualDevice^ _instance;
      static Persistent<FunctionTemplate> s_constructorTemplate;

      friend v8::Local<v8::Value> WrapMidiVirtualDevice(::Windows::Devices::Midi2::MidiVirtualDevice^ wintRtInstance);
      friend ::Windows::Devices::Midi2::MidiVirtualDevice^ UnwrapMidiVirtualDevice(Local<Value> value);
  };

  Persistent<FunctionTemplate> MidiVirtualDevice::s_constructorTemplate;

  v8::Local<v8::Value> WrapMidiVirtualDevice(::Windows::Devices::Midi2::MidiVirtualDevice^ winRtInstance) {
    EscapableHandleScope scope;

    if (winRtInstance == nullptr) {
      return scope.Escape(Undefined());
    }

    Local<Value> opaqueWrapper = CreateOpaqueWrapper(winRtInstance);
    Local<Value> args[] = {opaqueWrapper};
    Local<FunctionTemplate> localRef = Nan::New<FunctionTemplate>(MidiVirtualDevice::s_constructorTemplate);
    return scope.Escape(Nan::NewInstance(Nan::GetFunction(localRef).ToLocalChecked(),_countof(args), args).ToLocalChecked());
  }

  ::Windows::Devices::Midi2::MidiVirtualDevice^ UnwrapMidiVirtualDevice(Local<Value> value) {
     return MidiVirtualDevice::Unwrap<MidiVirtualDevice>(Nan::To<Object>(value).ToLocalChecked())->_instance;
  }

  void InitMidiVirtualDevice(Local<Object> exports) {
    MidiVirtualDevice::Init(exports);
  }


} } } } 

NAN_MODULE_INIT(init) {
  // We ignore failures for now since it probably means that
  // the initialization already happened for STA, and that's cool

  CoInitializeEx(nullptr, COINIT_MULTITHREADED);

  /*
  if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED))) {
    Nan::ThrowError(Nan::Error(NodeRT::Utils::NewString(L"error in CoInitializeEx()")));
    return;
  }
  */

      NodeRT::Windows::Devices::Midi2::InitMidi1ChannelVoiceMessageStatusEnum(target);
      NodeRT::Windows::Devices::Midi2::InitMidi2ChannelVoiceMessageStatusEnum(target);
      NodeRT::Windows::Devices::Midi2::InitMidiEndpointConnectionSharingEnum(target);
      NodeRT::Windows::Devices::Midi2::InitMidiEndpointConnectionSharingPreferenceEnum(target);
      NodeRT::Windows::Devices::Midi2::InitMidiEndpointDeviceInformationFilterEnum(target);
      NodeRT::Windows::Devices::Midi2::InitMidiEndpointDeviceInformationSortOrderEnum(target);
      NodeRT::Windows::Devices::Midi2::InitMidiEndpointDevicePurposeEnum(target);
      NodeRT::Windows::Devices::Midi2::InitMidiEndpointDiscoveryFilterFlagsEnum(target);
      NodeRT::Windows::Devices::Midi2::InitMidiEndpointNativeDataFormatEnum(target);
      NodeRT::Windows::Devices::Midi2::InitMidiFunctionBlockDirectionEnum(target);
      NodeRT::Windows::Devices::Midi2::InitMidiFunctionBlockDiscoveryFilterFlagsEnum(target);
      NodeRT::Windows::Devices::Midi2::InitMidiFunctionBlockMidi10Enum(target);
      NodeRT::Windows::Devices::Midi2::InitMidiFunctionBlockUIHintEnum(target);
      NodeRT::Windows::Devices::Midi2::InitMidiGroupTerminalBlockDirectionEnum(target);
      NodeRT::Windows::Devices::Midi2::InitMidiGroupTerminalBlockProtocolEnum(target);
      NodeRT::Windows::Devices::Midi2::InitMidiMessageTypeEnum(target);
      NodeRT::Windows::Devices::Midi2::InitMidiPacketTypeEnum(target);
      NodeRT::Windows::Devices::Midi2::InitMidiProtocolEnum(target);
      NodeRT::Windows::Devices::Midi2::InitMidiSendMessageResultEnum(target);
      NodeRT::Windows::Devices::Midi2::InitMidiSystemExclusive8StatusEnum(target);
      NodeRT::Windows::Devices::Midi2::InitIMidiEndpointConnectionSource(target);
      NodeRT::Windows::Devices::Midi2::InitIMidiEndpointDefinedConnectionSettings(target);
      NodeRT::Windows::Devices::Midi2::InitIMidiEndpointMessageProcessingPlugin(target);
      NodeRT::Windows::Devices::Midi2::InitIMidiMessageReceivedEventSource(target);
      NodeRT::Windows::Devices::Midi2::InitIMidiTransportSettingsData(target);
      NodeRT::Windows::Devices::Midi2::InitIMidiUniversalPacket(target);
      NodeRT::Windows::Devices::Midi2::InitMidiChannel(target);
      NodeRT::Windows::Devices::Midi2::InitMidiChannelEndpointListener(target);
      NodeRT::Windows::Devices::Midi2::InitMidiClock(target);
      NodeRT::Windows::Devices::Midi2::InitMidiEndpointConnection(target);
      NodeRT::Windows::Devices::Midi2::InitMidiEndpointConnectionOptions(target);
      NodeRT::Windows::Devices::Midi2::InitMidiEndpointDeviceInformation(target);
      NodeRT::Windows::Devices::Midi2::InitMidiEndpointDeviceInformationUpdateEventArgs(target);
      NodeRT::Windows::Devices::Midi2::InitMidiEndpointDeviceWatcher(target);
      NodeRT::Windows::Devices::Midi2::InitMidiFunctionBlock(target);
      NodeRT::Windows::Devices::Midi2::InitMidiGroup(target);
      NodeRT::Windows::Devices::Midi2::InitMidiGroupEndpointListener(target);
      NodeRT::Windows::Devices::Midi2::InitMidiGroupTerminalBlock(target);
      NodeRT::Windows::Devices::Midi2::InitMidiMessage128(target);
      NodeRT::Windows::Devices::Midi2::InitMidiMessage32(target);
      NodeRT::Windows::Devices::Midi2::InitMidiMessage64(target);
      NodeRT::Windows::Devices::Midi2::InitMidiMessage96(target);
      NodeRT::Windows::Devices::Midi2::InitMidiMessageBuilder(target);
      NodeRT::Windows::Devices::Midi2::InitMidiMessageConverter(target);
      NodeRT::Windows::Devices::Midi2::InitMidiMessageProcessingPluginInformation(target);
      NodeRT::Windows::Devices::Midi2::InitMidiMessageReceivedEventArgs(target);
      NodeRT::Windows::Devices::Midi2::InitMidiMessageTranslator(target);
      NodeRT::Windows::Devices::Midi2::InitMidiMessageTypeEndpointListener(target);
      NodeRT::Windows::Devices::Midi2::InitMidiMessageUtility(target);
      NodeRT::Windows::Devices::Midi2::InitMidiService(target);
      NodeRT::Windows::Devices::Midi2::InitMidiServicePingResponse(target);
      NodeRT::Windows::Devices::Midi2::InitMidiServicePingResponseSummary(target);
      NodeRT::Windows::Devices::Midi2::InitMidiSession(target);
      NodeRT::Windows::Devices::Midi2::InitMidiSessionSettings(target);
      NodeRT::Windows::Devices::Midi2::InitMidiStreamConfigurationRequestedSettings(target);
      NodeRT::Windows::Devices::Midi2::InitMidiStreamMessageBuilder(target);
      NodeRT::Windows::Devices::Midi2::InitMidiTransportPluginInformation(target);
      NodeRT::Windows::Devices::Midi2::InitMidiUniqueId(target);
      NodeRT::Windows::Devices::Midi2::InitMidiVirtualDevice(target);


  NodeRT::Utils::RegisterNameSpace("Windows.Devices.Midi2", target);
}



NODE_MODULE(binding, init)
