// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"

HRESULT
MidiNetworkDataWriter::Send()
{
    RETURN_HR_IF_NULL(E_UNEXPECTED, m_dataWriter);

    m_dataWriter.StoreAsync().get();

    return S_OK;
}



HRESULT 
MidiNetworkDataWriter::WriteUdpPacketHeader()
{
    RETURN_HR_IF_NULL(E_UNEXPECTED, m_dataWriter);

    m_dataWriter.WriteUInt32(MIDI_UDP_PAYLOAD_HEADER);

    return S_OK;
}

_Use_decl_annotations_
HRESULT 
MidiNetworkDataWriter::InternalWriteCommandHeader(
    MidiNetworkCommandCode commandCode, 
    byte payloadLengthIn32BitWords,
    uint16_t commandSpecificData
)
{
    RETURN_HR_IF_NULL(E_UNEXPECTED, m_dataWriter);

    m_dataWriter.WriteByte(commandCode);
    m_dataWriter.WriteByte(payloadLengthIn32BitWords);
    m_dataWriter.WriteUInt16(commandSpecificData);

    return S_OK;
}

_Use_decl_annotations_
HRESULT
MidiNetworkDataWriter::InternalWriteCommandHeader(
    MidiNetworkCommandCode commandCode,
    byte payloadLengthIn32BitWords,
    byte commandSpecificData1,
    byte commandSpecificData2
)
{
    RETURN_HR_IF_NULL(E_UNEXPECTED, m_dataWriter);

    m_dataWriter.WriteByte(commandCode);
    m_dataWriter.WriteByte(payloadLengthIn32BitWords);
    m_dataWriter.WriteByte(commandSpecificData1);
    m_dataWriter.WriteByte(commandSpecificData2);

    return S_OK;
}



_Use_decl_annotations_
HRESULT 
MidiNetworkDataWriter::Initialize(
    winrt::Windows::Storage::Streams::IOutputStream stream
)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, stream);
    m_stream = stream;

    winrt::Windows::Storage::Streams::DataWriter writer(m_stream);

    m_dataWriter = std::move(writer);
    RETURN_HR_IF_NULL(E_UNEXPECTED, m_dataWriter);

    m_dataWriter.ByteOrder(winrt::Windows::Storage::Streams::ByteOrder::BigEndian);

    return S_OK;
}

_Use_decl_annotations_
HRESULT
MidiNetworkDataWriter::WriteCommandPing(uint32_t pingId)
{
    RETURN_HR_IF_NULL(E_UNEXPECTED, m_dataWriter);

    byte payloadLengthIn32BitWords{ 1 };

    RETURN_IF_FAILED(InternalWriteCommandHeader(MidiNetworkCommandCode::CommandCommon_Ping, payloadLengthIn32BitWords, 0));

    m_dataWriter.WriteUInt32(pingId);

    return S_OK;
}

_Use_decl_annotations_
HRESULT
MidiNetworkDataWriter::WriteCommandPingReply(uint32_t pingId)
{
    RETURN_HR_IF_NULL(E_UNEXPECTED, m_dataWriter);
    byte payloadLengthIn32BitWords{ 1 };

    RETURN_IF_FAILED(InternalWriteCommandHeader(MidiNetworkCommandCode::CommandCommon_PingReply, payloadLengthIn32BitWords, 0));

    m_dataWriter.WriteUInt32(pingId);

    return S_OK;
}




_Use_decl_annotations_
HRESULT
MidiNetworkDataWriter::WriteCommandNAK(
    uint32_t originalCommandHeader,
    MidiNetworkCommandNAKReason reason,
    std::wstring textMessage
)
{
    RETURN_HR_IF_NULL(E_UNEXPECTED, m_dataWriter);

    byte payloadLengthIn32BitWords{ 1 };    // 1 here to account for the original command header

    std::string utf8{};

    if (!textMessage.empty())
    {
        utf8 = ConvertWStringToUTF8(textMessage, MIDI_MAX_NAK_MESSAGE_BYTE_COUNT);
    }

    payloadLengthIn32BitWords += CalculatePaddedStringSizeIn32BitWords(utf8, MIDI_MAX_NAK_MESSAGE_BYTE_COUNT);

    RETURN_IF_FAILED(InternalWriteCommandHeader(MidiNetworkCommandCode::CommandCommon_NAK, payloadLengthIn32BitWords, reason, 0));

    m_dataWriter.WriteUInt32(originalCommandHeader);

    // write the text
    WritePaddedString(utf8, MIDI_MAX_NAK_MESSAGE_BYTE_COUNT);

    return S_OK;
}


_Use_decl_annotations_
HRESULT
MidiNetworkDataWriter::WriteCommandBye(
    MidiNetworkCommandByeReason reason, 
    std::wstring textMessage
)
{
    RETURN_HR_IF_NULL(E_UNEXPECTED, m_dataWriter);

    byte payloadLengthIn32BitWords{ 0 };    // 1 here to account for the original command header

    std::string utf8{};

    if (!textMessage.empty())
    {
        utf8 = ConvertWStringToUTF8(textMessage, MIDI_MAX_BYE_MESSAGE_BYTE_COUNT);
    }

    payloadLengthIn32BitWords += CalculatePaddedStringSizeIn32BitWords(utf8, MIDI_MAX_BYE_MESSAGE_BYTE_COUNT);

    RETURN_IF_FAILED(InternalWriteCommandHeader(MidiNetworkCommandCode::CommandCommon_Bye, payloadLengthIn32BitWords, reason, 0));

    // write the text
    WritePaddedString(utf8, MIDI_MAX_BYE_MESSAGE_BYTE_COUNT);

    return S_OK;
}

HRESULT 
MidiNetworkDataWriter::WriteCommandByeReply()
{
    RETURN_HR_IF_NULL(E_UNEXPECTED, m_dataWriter);

    RETURN_IF_FAILED(InternalWriteCommandHeader(MidiNetworkCommandCode::CommandCommon_ByeReply, MIDI_COMMAND_PAYLOAD_LENGTH_NO_PAYLOAD, 0));

    return S_OK;
}

HRESULT 
MidiNetworkDataWriter::WriteCommandSessionReset()
{
    RETURN_HR_IF_NULL(E_UNEXPECTED, m_dataWriter);

    RETURN_IF_FAILED(InternalWriteCommandHeader(MidiNetworkCommandCode::CommandCommon_SessionReset, MIDI_COMMAND_PAYLOAD_LENGTH_NO_PAYLOAD, 0));

    return S_OK;
}

HRESULT 
MidiNetworkDataWriter::WriteCommandSessionResetReply()
{
    RETURN_HR_IF_NULL(E_UNEXPECTED, m_dataWriter);

    RETURN_IF_FAILED(InternalWriteCommandHeader(MidiNetworkCommandCode::CommandCommon_SessionResetReply, MIDI_COMMAND_PAYLOAD_LENGTH_NO_PAYLOAD, 0));

    return S_OK;
}

_Use_decl_annotations_
HRESULT
MidiNetworkDataWriter::WriteCommandRetransmitRequest(
    uint16_t sequenceNumber, 
    uint16_t numberOfUmpCommands
)
{
    RETURN_HR_IF_NULL(E_UNEXPECTED, m_dataWriter);

    byte payloadLengthIn32BitWords{ 1 };    // 1 here to account for number of UMP commands

    RETURN_IF_FAILED(InternalWriteCommandHeader(MidiNetworkCommandCode::CommandCommon_RetransmitRequest, payloadLengthIn32BitWords, sequenceNumber));

    m_dataWriter.WriteUInt16(numberOfUmpCommands);
    m_dataWriter.WriteUInt16(0);

    return S_OK;
}

// todo: change errorReason to an enum. See page 47 - 7.2.4
_Use_decl_annotations_
HRESULT
MidiNetworkDataWriter::WriteCommandRetransmitError(
    uint16_t sequenceNumber, 
    MidiNetworkCommandRetransmitErrorReason errorReason
)
{
    RETURN_HR_IF_NULL(E_UNEXPECTED, m_dataWriter);

    byte payloadLengthIn32BitWords{ 1 };    // 1 here to account for sequence number

    RETURN_IF_FAILED(InternalWriteCommandHeader(MidiNetworkCommandCode::CommandCommon_RetransmitError, payloadLengthIn32BitWords, errorReason, 0));

    m_dataWriter.WriteUInt16(sequenceNumber);
    m_dataWriter.WriteUInt16(0);

    return S_OK;
}

// todo: change the capabilities to a bitmap enum
_Use_decl_annotations_
HRESULT
MidiNetworkDataWriter::WriteCommandInvitation(
    MidiNetworkCommandInvitationCapabilities capabilities,
    std::wstring clientUmpEndpointName, 
    std::wstring clientProductInstanceId
)
{
    RETURN_HR_IF_NULL(E_UNEXPECTED, m_dataWriter);

    byte payloadLengthIn32BitWords{ 0 };

    auto endpointNameUtf8 = ConvertWStringToUTF8(clientUmpEndpointName, MIDI_MAX_UMP_ENDPOINT_NAME_BYTE_COUNT);
    auto productInstanceIdUtf8 = ConvertWStringToUTF8(clientProductInstanceId, MIDI_MAX_UMP_PRODUCT_INSTANCE_ID_BYTE_COUNT);

    byte umpEndpointNameLengthIn32BitWords{ CalculatePaddedStringSizeIn32BitWords(endpointNameUtf8, MIDI_MAX_UMP_ENDPOINT_NAME_BYTE_COUNT) };
    byte productInstanceIdLengthIn32BitWords{ CalculatePaddedStringSizeIn32BitWords(productInstanceIdUtf8, MIDI_MAX_UMP_PRODUCT_INSTANCE_ID_BYTE_COUNT) };

    payloadLengthIn32BitWords += umpEndpointNameLengthIn32BitWords;
    payloadLengthIn32BitWords += productInstanceIdLengthIn32BitWords;

    RETURN_IF_FAILED(InternalWriteCommandHeader(MidiNetworkCommandCode::CommandClientToHost_Invitation, payloadLengthIn32BitWords, umpEndpointNameLengthIn32BitWords, capabilities));

    WritePaddedString(endpointNameUtf8, MIDI_MAX_UMP_ENDPOINT_NAME_BYTE_COUNT);
    WritePaddedString(productInstanceIdUtf8, MIDI_MAX_UMP_PRODUCT_INSTANCE_ID_BYTE_COUNT);

    return S_OK;
}

_Use_decl_annotations_
HRESULT
MidiNetworkDataWriter::WriteCommandInvitationWithAuthentication(
    std::string cryptoNonce, 
    std::string sharedSecret
)
{
    RETURN_HR_IF_NULL(E_UNEXPECTED, m_dataWriter);

    UNREFERENCED_PARAMETER(cryptoNonce);
    UNREFERENCED_PARAMETER(sharedSecret);

    return E_NOTIMPL;
}

_Use_decl_annotations_
HRESULT
MidiNetworkDataWriter::WriteCommandInvitationWithUserAuthentication(
    std::string cryptoNonce, 
    std::string userName, 
    std::string password
)
{
    RETURN_HR_IF_NULL(E_UNEXPECTED, m_dataWriter);

    UNREFERENCED_PARAMETER(cryptoNonce);
    UNREFERENCED_PARAMETER(userName);
    UNREFERENCED_PARAMETER(password);

    return E_NOTIMPL;
}

_Use_decl_annotations_
HRESULT
MidiNetworkDataWriter::WriteCommandInvitationReplyAccepted(
    std::wstring hostUmpEndpointName, 
    std::wstring hostProductInstanceId
)
{
    RETURN_HR_IF_NULL(E_UNEXPECTED, m_dataWriter);

    byte payloadLengthIn32BitWords{ 0 };

    auto endpointNameUtf8 = ConvertWStringToUTF8(hostUmpEndpointName, MIDI_MAX_UMP_ENDPOINT_NAME_BYTE_COUNT);
    byte umpEndpointNameLengthIn32BitWords{ CalculatePaddedStringSizeIn32BitWords(endpointNameUtf8, MIDI_MAX_UMP_ENDPOINT_NAME_BYTE_COUNT) };
    payloadLengthIn32BitWords += umpEndpointNameLengthIn32BitWords;

    auto productInstanceIdUtf8 = ConvertWStringToUTF8(hostProductInstanceId, MIDI_MAX_UMP_PRODUCT_INSTANCE_ID_BYTE_COUNT);
    byte productInstanceIdLengthIn32BitWords{ CalculatePaddedStringSizeIn32BitWords(productInstanceIdUtf8, MIDI_MAX_UMP_PRODUCT_INSTANCE_ID_BYTE_COUNT) };
    payloadLengthIn32BitWords += productInstanceIdLengthIn32BitWords;


    RETURN_IF_FAILED(InternalWriteCommandHeader(MidiNetworkCommandCode::CommandHostToClient_InvitationReplyAccepted, payloadLengthIn32BitWords, umpEndpointNameLengthIn32BitWords, 0));

    WritePaddedString(endpointNameUtf8, MIDI_MAX_UMP_ENDPOINT_NAME_BYTE_COUNT);
    WritePaddedString(productInstanceIdUtf8, MIDI_MAX_UMP_PRODUCT_INSTANCE_ID_BYTE_COUNT);

    return S_OK;
}

_Use_decl_annotations_
HRESULT 
MidiNetworkDataWriter::WriteCommandInvitationReplyPending(
    std::wstring hostUmpEndpointName, 
    std::wstring hostProductInstanceId
)
{
    RETURN_HR_IF_NULL(E_UNEXPECTED, m_dataWriter);

    UNREFERENCED_PARAMETER(hostUmpEndpointName);
    UNREFERENCED_PARAMETER(hostProductInstanceId);

    return E_NOTIMPL;
}

// todo: change authenticationState to an enum (see spec page 31)
_Use_decl_annotations_
HRESULT 
MidiNetworkDataWriter::WriteCommandInvitationReplyAuthenticationRequired(
    std::string cryptoNonce, 
    byte authenticationState, 
    std::wstring hostUmpEndpointName, 
    std::wstring hostProductInstanceId
)
{
    RETURN_HR_IF_NULL(E_UNEXPECTED, m_dataWriter);

    UNREFERENCED_PARAMETER(cryptoNonce);
    UNREFERENCED_PARAMETER(authenticationState);
    UNREFERENCED_PARAMETER(hostUmpEndpointName);
    UNREFERENCED_PARAMETER(hostProductInstanceId);

    return E_NOTIMPL;
}

// todo: change authenticationState to an enum (different from other enum) See page 33
_Use_decl_annotations_
HRESULT 
MidiNetworkDataWriter::WriteCommandInvitationReplyUserAuthenticationRequired(
    std::string cryptoNonce, 
    byte authenticationState, 
    std::wstring hostUmpEndpointName, 
    std::wstring hostProductInstanceId
)
{
    RETURN_HR_IF_NULL(E_UNEXPECTED, m_dataWriter);

    UNREFERENCED_PARAMETER(cryptoNonce);
    UNREFERENCED_PARAMETER(authenticationState);
    UNREFERENCED_PARAMETER(hostUmpEndpointName);
    UNREFERENCED_PARAMETER(hostProductInstanceId);

    return E_NOTIMPL;
}

// can send multiple UMP messages. If more than the max
// words are provided, will fail. Can be  zero words, in 
// which case a command is still sent out as a keep-alive 
// idle type of message (7.2.1)
_Use_decl_annotations_
HRESULT
MidiNetworkDataWriter::WriteCommandUmpMessages(
    uint16_t sequenceNumber,
    std::vector<uint32_t> words
)
{
    RETURN_HR_IF_NULL(E_UNEXPECTED, m_dataWriter);

    RETURN_HR_IF(E_INVALIDARG, words.size() > MIDI_MAX_UMP_WORDS_PER_PACKET);

    RETURN_IF_FAILED(InternalWriteCommandHeader(MidiNetworkCommandCode::CommandCommon_UmpData, static_cast<byte>(words.size()), sequenceNumber));

    // we make the assumption that the calling code has already validated that the words
    // are complete messages, or zero-length. We don't check again here.

    for (auto const& word : words)
    {
        m_dataWriter.WriteUInt32(word);
    }

    return S_OK;
}

HRESULT
MidiNetworkDataWriter::Shutdown()
{
    m_dataWriter.Close();
    m_dataWriter = nullptr;

    m_stream = nullptr;

    return S_OK;
}