// WARNING: Please don't edit this file. It was generated by C++/WinRT v2.0.240405.15

#pragma once
#ifndef WINRT_Microsoft_Windows_Devices_Midi2_Messages_2_H
#define WINRT_Microsoft_Windows_Devices_Midi2_Messages_2_H
#include "winrt/impl/Microsoft.Windows.Devices.Midi2.1.h"
#include "winrt/impl/Windows.Devices.Midi.1.h"
#include "winrt/impl/Windows.Foundation.Collections.1.h"
#include "winrt/impl/Microsoft.Windows.Devices.Midi2.Messages.1.h"
WINRT_EXPORT namespace winrt::Microsoft::Windows::Devices::Midi2::Messages
{
    struct MidiMessageBuilder
    {
        MidiMessageBuilder() = delete;
        static auto BuildUtilityMessage(uint64_t timestamp, uint8_t status, uint32_t dataOrReserved);
        static auto BuildSystemMessage(uint64_t timestamp, winrt::Microsoft::Windows::Devices::Midi2::MidiGroup const& group, uint8_t status, uint8_t midi1Byte2, uint8_t midi1Byte3);
        static auto BuildMidi1ChannelVoiceMessage(uint64_t timestamp, winrt::Microsoft::Windows::Devices::Midi2::MidiGroup const& group, winrt::Microsoft::Windows::Devices::Midi2::Messages::Midi1ChannelVoiceMessageStatus const& status, winrt::Microsoft::Windows::Devices::Midi2::MidiChannel const& channel, uint8_t byte3, uint8_t byte4);
        static auto BuildSystemExclusive7Message(uint64_t timestamp, winrt::Microsoft::Windows::Devices::Midi2::MidiGroup const& group, uint8_t status, uint8_t numberOfBytes, uint8_t dataByte0, uint8_t dataByte1, uint8_t dataByte2, uint8_t dataByte3, uint8_t dataByte4, uint8_t dataByte5);
        static auto BuildMidi2ChannelVoiceMessage(uint64_t timestamp, winrt::Microsoft::Windows::Devices::Midi2::MidiGroup const& group, winrt::Microsoft::Windows::Devices::Midi2::Messages::Midi2ChannelVoiceMessageStatus const& status, winrt::Microsoft::Windows::Devices::Midi2::MidiChannel const& channel, uint16_t index, uint32_t data);
        static auto BuildSystemExclusive8Message(uint64_t timestamp, winrt::Microsoft::Windows::Devices::Midi2::MidiGroup const& group, winrt::Microsoft::Windows::Devices::Midi2::Messages::MidiSystemExclusive8Status const& status, uint8_t numberOfValidDataBytesThisMessage, uint8_t streamId, uint8_t dataByte00, uint8_t dataByte01, uint8_t dataByte02, uint8_t dataByte03, uint8_t dataByte04, uint8_t dataByte05, uint8_t dataByte06, uint8_t dataByte07, uint8_t dataByte08, uint8_t dataByte09, uint8_t dataByte10, uint8_t dataByte11, uint8_t dataByte12);
        static auto BuildMixedDataSetChunkHeaderMessage(uint64_t timestamp, winrt::Microsoft::Windows::Devices::Midi2::MidiGroup const& group, uint8_t mdsId, uint16_t numberValidDataBytesInThisChunk, uint16_t numberChunksInMixedDataSet, uint16_t numberOfThisChunk, uint16_t manufacturerId, uint16_t deviceId, uint16_t subId1, uint16_t subId2);
        static auto BuildMixedDataSetChunkDataMessage(uint64_t timestamp, winrt::Microsoft::Windows::Devices::Midi2::MidiGroup const& group, uint8_t mdsId, uint8_t dataByte00, uint8_t dataByte01, uint8_t dataByte02, uint8_t dataByte03, uint8_t dataByte04, uint8_t dataByte05, uint8_t dataByte06, uint8_t dataByte07, uint8_t dataByte08, uint8_t dataByte09, uint8_t dataByte10, uint8_t dataByte11, uint8_t dataByte12, uint8_t dataByte13);
        static auto BuildFlexDataMessage(uint64_t timestamp, winrt::Microsoft::Windows::Devices::Midi2::MidiGroup const& group, uint8_t form, uint8_t address, winrt::Microsoft::Windows::Devices::Midi2::MidiChannel const& channel, uint8_t statusBank, uint8_t status, uint32_t word1Data, uint32_t word2Data, uint32_t word3Data);
        static auto BuildStreamMessage(uint64_t timestamp, uint8_t form, uint16_t status, uint16_t word0RemainingData, uint32_t word1Data, uint32_t word2Data, uint32_t word3Data);
    };
    struct MidiMessageConverter
    {
        MidiMessageConverter() = delete;
        static auto ConvertMidi1Message(uint64_t timestamp, winrt::Microsoft::Windows::Devices::Midi2::MidiGroup const& group, uint8_t statusByte);
        static auto ConvertMidi1Message(uint64_t timestamp, winrt::Microsoft::Windows::Devices::Midi2::MidiGroup const& group, uint8_t statusByte, uint8_t dataByte1);
        static auto ConvertMidi1Message(uint64_t timestamp, winrt::Microsoft::Windows::Devices::Midi2::MidiGroup const& group, uint8_t statusByte, uint8_t dataByte1, uint8_t dataByte2);
        static auto ConvertMidi1ChannelPressureMessage(uint64_t timestamp, winrt::Microsoft::Windows::Devices::Midi2::MidiGroup const& group, winrt::Windows::Devices::Midi::MidiChannelPressureMessage const& originalMessage);
        static auto ConvertMidi1NoteOffMessage(uint64_t timestamp, winrt::Microsoft::Windows::Devices::Midi2::MidiGroup const& group, winrt::Windows::Devices::Midi::MidiNoteOffMessage const& originalMessage);
        static auto ConvertMidi1NoteOnMessage(uint64_t timestamp, winrt::Microsoft::Windows::Devices::Midi2::MidiGroup const& group, winrt::Windows::Devices::Midi::MidiNoteOnMessage const& originalMessage);
        static auto ConvertMidi1PitchBendChangeMessage(uint64_t timestamp, winrt::Microsoft::Windows::Devices::Midi2::MidiGroup const& group, winrt::Windows::Devices::Midi::MidiPitchBendChangeMessage const& originalMessage);
        static auto ConvertMidi1PolyphonicKeyPressureMessage(uint64_t timestamp, winrt::Microsoft::Windows::Devices::Midi2::MidiGroup const& group, winrt::Windows::Devices::Midi::MidiPolyphonicKeyPressureMessage const& originalMessage);
        static auto ConvertMidi1ProgramChangeMessage(uint64_t timestamp, winrt::Microsoft::Windows::Devices::Midi2::MidiGroup const& group, winrt::Windows::Devices::Midi::MidiProgramChangeMessage const& originalMessage);
        static auto ConvertMidi1TimeCodeMessage(uint64_t timestamp, winrt::Microsoft::Windows::Devices::Midi2::MidiGroup const& group, winrt::Windows::Devices::Midi::MidiTimeCodeMessage const& originalMessage);
        static auto ConvertMidi1SongPositionPointerMessage(uint64_t timestamp, winrt::Microsoft::Windows::Devices::Midi2::MidiGroup const& group, winrt::Windows::Devices::Midi::MidiSongPositionPointerMessage const& originalMessage);
        static auto ConvertMidi1SongSelectMessage(uint64_t timestamp, winrt::Microsoft::Windows::Devices::Midi2::MidiGroup const& group, winrt::Windows::Devices::Midi::MidiSongSelectMessage const& originalMessage);
        static auto ConvertMidi1TuneRequestMessage(uint64_t timestamp, winrt::Microsoft::Windows::Devices::Midi2::MidiGroup const& group, winrt::Windows::Devices::Midi::MidiTuneRequestMessage const& originalMessage);
        static auto ConvertMidi1TimingClockMessage(uint64_t timestamp, winrt::Microsoft::Windows::Devices::Midi2::MidiGroup const& group, winrt::Windows::Devices::Midi::MidiTimingClockMessage const& originalMessage);
        static auto ConvertMidi1StartMessage(uint64_t timestamp, winrt::Microsoft::Windows::Devices::Midi2::MidiGroup const& group, winrt::Windows::Devices::Midi::MidiStartMessage const& originalMessage);
        static auto ConvertMidi1ContinueMessage(uint64_t timestamp, winrt::Microsoft::Windows::Devices::Midi2::MidiGroup const& group, winrt::Windows::Devices::Midi::MidiContinueMessage const& originalMessage);
        static auto ConvertMidi1StopMessage(uint64_t timestamp, winrt::Microsoft::Windows::Devices::Midi2::MidiGroup const& group, winrt::Windows::Devices::Midi::MidiStopMessage const& originalMessage);
        static auto ConvertMidi1ActiveSensingMessage(uint64_t timestamp, winrt::Microsoft::Windows::Devices::Midi2::MidiGroup const& group, winrt::Windows::Devices::Midi::MidiActiveSensingMessage const& originalMessage);
        static auto ConvertMidi1SystemResetMessage(uint64_t timestamp, winrt::Microsoft::Windows::Devices::Midi2::MidiGroup const& group, winrt::Windows::Devices::Midi::MidiSystemResetMessage const& originalMessage);
    };
    struct MidiMessageHelper
    {
        MidiMessageHelper() = delete;
        static auto ValidateMessage32MessageType(uint32_t word0);
        static auto ValidateMessage64MessageType(uint32_t word0);
        static auto ValidateMessage96MessageType(uint32_t word0);
        static auto ValidateMessage128MessageType(uint32_t word0);
        static auto GetMessageTypeFromMessageFirstWord(uint32_t word0);
        static auto GetPacketTypeFromMessageFirstWord(uint32_t word0);
        static auto MessageTypeHasGroupField(winrt::Microsoft::Windows::Devices::Midi2::MidiMessageType const& messageType);
        static auto ReplaceGroupInMessageFirstWord(uint32_t word0, winrt::Microsoft::Windows::Devices::Midi2::MidiGroup const& newGroup);
        static auto GetGroupFromMessageFirstWord(uint32_t word0);
        static auto GetStatusFromUtilityMessage(uint32_t word0);
        static auto GetStatusFromMidi1ChannelVoiceMessage(uint32_t word0);
        static auto GetStatusFromMidi2ChannelVoiceMessageFirstWord(uint32_t word0);
        static auto GetStatusBankFromFlexDataMessageFirstWord(uint32_t word0);
        static auto GetStatusFromFlexDataMessageFirstWord(uint32_t word0);
        static auto GetStatusFromSystemCommonMessage(uint32_t word0);
        static auto GetStatusFromDataMessage64FirstWord(uint32_t word0);
        static auto GetNumberOfBytesFromDataMessage64FirstWord(uint32_t word0);
        static auto GetStatusFromDataMessage128FirstWord(uint32_t word0);
        static auto GetNumberOfBytesFromDataMessage128FirstWord(uint32_t word0);
        static auto MessageTypeHasChannelField(winrt::Microsoft::Windows::Devices::Midi2::MidiMessageType const& messageType);
        static auto ReplaceChannelInMessageFirstWord(uint32_t word0, winrt::Microsoft::Windows::Devices::Midi2::MidiChannel const& newChannel);
        static auto GetChannelFromMessageFirstWord(uint32_t word0);
        static auto GetFormFromStreamMessageFirstWord(uint32_t word0);
        static auto GetStatusFromStreamMessageFirstWord(uint32_t word0);
        static auto GetMessageDisplayNameFromFirstWord(uint32_t word0);
        static auto GetPacketListFromWordList(uint64_t timestamp, param::iterable<uint32_t> const& words);
        static auto GetWordListFromPacketList(param::iterable<winrt::Microsoft::Windows::Devices::Midi2::IMidiUniversalPacket> const& words);
    };
    struct MidiStreamMessageBuilder
    {
        MidiStreamMessageBuilder() = delete;
        static auto BuildEndpointDiscoveryMessage(uint64_t timestamp, uint8_t umpVersionMajor, uint8_t umpVersionMinor, winrt::Microsoft::Windows::Devices::Midi2::Messages::MidiEndpointDiscoveryRequests const& request);
        static auto BuildEndpointInfoNotificationMessage(uint64_t timestamp, uint8_t umpVersionMajor, uint8_t umpVersionMinor, bool hasStaticFunctionBlocks, uint8_t numberOfFunctionBlocks, bool supportsMidi20Protocol, bool supportsMidi10Protocol, bool supportsReceivingJitterReductionTimestamps, bool supportsSendingJitterReductionTimestamps);
        static auto BuildDeviceIdentityNotificationMessage(uint64_t timestamp, uint8_t deviceManufacturerSysExIdByte1, uint8_t deviceManufacturerSysExIdByte2, uint8_t deviceManufacturerSysExIdByte3, uint8_t deviceFamilyLsb, uint8_t deviceFamilyMsb, uint8_t deviceFamilyModelNumberLsb, uint8_t deviceFamilyModelNumberMsb, uint8_t softwareRevisionLevelByte1, uint8_t softwareRevisionLevelByte2, uint8_t softwareRevisionLevelByte3, uint8_t softwareRevisionLevelByte4);
        static auto BuildEndpointNameNotificationMessages(uint64_t timestamp, param::hstring const& name);
        static auto BuildProductInstanceIdNotificationMessages(uint64_t timestamp, param::hstring const& productInstanceId);
        static auto ParseEndpointNameNotificationMessages(param::iterable<winrt::Microsoft::Windows::Devices::Midi2::IMidiUniversalPacket> const& messages);
        static auto ParseProductInstanceIdNotificationMessages(param::iterable<winrt::Microsoft::Windows::Devices::Midi2::IMidiUniversalPacket> const& messages);
        static auto BuildStreamConfigurationRequestMessage(uint64_t timestamp, uint8_t protocol, bool expectToReceiveJRTimestamps, bool requestToSendJRTimestamps);
        static auto BuildStreamConfigurationNotificationMessage(uint64_t timestamp, uint8_t protocol, bool confirmationWillReceiveJRTimestamps, bool confirmationSendJRTimestamps);
        static auto BuildFunctionBlockDiscoveryMessage(uint64_t timestamp, uint8_t functionBlockNumber, winrt::Microsoft::Windows::Devices::Midi2::Messages::MidiFunctionBlockDiscoveryRequests const& requestFlags);
        static auto BuildFunctionBlockInfoNotificationMessage(uint64_t timestamp, bool active, uint8_t functionBlockNumber, winrt::Microsoft::Windows::Devices::Midi2::MidiFunctionBlockUIHint const& uiHint, winrt::Microsoft::Windows::Devices::Midi2::MidiFunctionBlockRepresentsMidi10Connection const& midi10, winrt::Microsoft::Windows::Devices::Midi2::MidiFunctionBlockDirection const& direction, uint8_t firstGroup, uint8_t numberOfGroups, uint8_t midiCIVersionFormat, uint8_t maxNumberSysEx8Streams);
        static auto BuildFunctionBlockNameNotificationMessages(uint64_t timestamp, uint8_t functionBlockNumber, param::hstring const& name);
        static auto ParseFunctionBlockNameNotificationMessages(param::iterable<winrt::Microsoft::Windows::Devices::Midi2::IMidiUniversalPacket> const& messages);
    };
}
#endif
