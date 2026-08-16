// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

// this class is 1:1 with a socket pair
// it's responsible for keeping track of packet numbers, 
// FEC, and more. It can be used both by active sessions
// and out-of-band messages
class MidiNetworkDataWriter
{
public:
    HRESULT Initialize(_In_ winrt::Windows::Storage::Streams::IOutputStream stream);

    HRESULT WriteUdpPacketHeader();
    HRESULT Send();


    HRESULT WriteCommandPing(_In_ uint32_t pingId);
    HRESULT WriteCommandPingReply(_In_ uint32_t pingId);
    HRESULT WriteCommandNAK(_In_ uint32_t originalCommandHeader, _In_ MidiNetworkCommandNAKReason reason, _In_ std::wstring textMessage);
    HRESULT WriteCommandBye(_In_ MidiNetworkCommandByeReason reason, _In_ std::wstring textMessage);
    HRESULT WriteCommandByeReply();

    HRESULT WriteCommandSessionReset();
    HRESULT WriteCommandSessionResetReply();

    HRESULT WriteCommandRetransmitRequest(_In_ MidiSequenceNumber sequenceNumber, _In_ uint16_t numberOfUmpCommands);
    HRESULT WriteCommandRetransmitError(_In_ MidiSequenceNumber sequenceNumber, _In_ MidiNetworkCommandRetransmitErrorReason errorReason);

    // todo: change the capabilities to a bitmap enum
    HRESULT WriteCommandInvitation(_In_ MidiNetworkCommandInvitationCapabilities capabilities, _In_ std::wstring clientUmpEndpointName, _In_ std::wstring clientProductInstanceId);
    HRESULT WriteCommandInvitationWithAuthentication(_In_ std::string cryptoNonce, _In_ std::string sharedSecret);
    HRESULT WriteCommandInvitationWithUserAuthentication(_In_ std::string cryptoNonce, _In_ std::string userName, _In_ std::string password);

    HRESULT WriteCommandInvitationReplyAccepted(_In_ std::wstring hostUmpEndpointName, _In_ std::wstring hostProductInstanceId);
    HRESULT WriteCommandInvitationReplyPending(_In_ std::wstring hostUmpEndpointName, _In_ std::wstring hostProductInstanceId);

    // todo: change authenticationState to an enum (see spec page 31)
    HRESULT WriteCommandInvitationReplyAuthenticationRequired(_In_ std::string cryptoNonce, _In_ byte authenticationState, _In_ std::wstring hostUmpEndpointName, _In_ std::wstring hostProductInstanceId);

    // todo: change authenticationState to an enum (different from other enum) See page 33
    HRESULT WriteCommandInvitationReplyUserAuthenticationRequired(_In_ std::string cryptoNonce, _In_ byte authenticationState, _In_ std::wstring hostUmpEndpointName, _In_ std::wstring hostProductInstanceId);

    HRESULT WriteCommandUmpMessages(_In_ MidiSequenceNumber sequenceNumber, _In_ std::vector<uint32_t> words);

    HRESULT WriteCommandUmpMessages(
        _In_ MidiSequenceNumber sequenceNumber,
        _In_reads_(wordCount) uint32_t const* words,
        _In_ uint8_t const wordCount);

    // Throws away any bytes buffered but not yet stored, so a failed or abandoned packet
    // is never prepended to the next one.
    HRESULT DiscardPendingData();

    HRESULT Shutdown();


    uint64_t GetCountNetworkPacketsSent() { return m_countNetworkPacketsSent; }

private:
    uint64_t m_countNetworkPacketsSent{ 0 };

    inline std::string ConvertWStringToUTF8(_In_ std::wstring s, _In_ size_t maxByteCount)
    {
        // The cap is rounded down to whole words first, so the truncation and the declared
        // length always agree.
        return internal::Utf8FromWString(
            internal::TruncateToUtf8ByteCount(s, EffectiveMaxByteCount(maxByteCount)));
    }

    // The declared length and the bytes actually written must always agree, otherwise every
    // field after this one in the datagram is misaligned. Caps are therefore rounded down to a
    // whole number of 32-bit words.
    static inline size_t EffectiveMaxByteCount(_In_ size_t maxByteCount)
    {
        if (maxByteCount > 255 * sizeof(uint32_t))
        {
            maxByteCount = 255 * sizeof(uint32_t);
        }

        return (maxByteCount / sizeof(uint32_t)) * sizeof(uint32_t);
    }

    inline void WritePaddedString(_In_ std::string s, _In_ size_t maxByteCount)
    {
        // don't lock here. Calling method will lock

        auto byteCount = min(s.size(), EffectiveMaxByteCount(maxByteCount));

        for (size_t i = 0; i < byteCount; i++)
        {
            m_dataWriter.WriteByte(s[i]);
        }

        auto paddingByteCount = (sizeof(uint32_t) - (byteCount % sizeof(uint32_t))) % sizeof(uint32_t);

        for (size_t i = 0; i < paddingByteCount; i++)
        {
            m_dataWriter.WriteByte(0);
        }
    }

    // 255 is largest this can be
    inline byte CalculatePaddedStringSizeIn32BitWords(_In_ std::string s, _In_ size_t maxByteCount)
    {
        auto byteCount = min(s.size(), EffectiveMaxByteCount(maxByteCount));

        return static_cast<byte>((byteCount + sizeof(uint32_t) - 1) / sizeof(uint32_t));
    }

    HRESULT InternalWriteCommandHeader(_In_ MidiNetworkCommandCode commandCode, _In_ byte payloadLength, _In_ uint16_t commandSpecificData);
    HRESULT InternalWriteCommandHeader(_In_ MidiNetworkCommandCode commandCode, _In_ byte payloadLength, _In_ byte commandSpecificData1, _In_ byte commandSpecificData2);


    // this handles wrapping the numbers around. Could just store a uint64_t and use % instead

    winrt::Windows::Storage::Streams::IOutputStream m_stream{ nullptr };
    winrt::Windows::Storage::Streams::DataWriter m_dataWriter{ nullptr };

    // data writer doesn't support concurrent writes and will throw exceptions if you try
    wil::critical_section m_dataWriterLock;
};