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

    HRESULT Shutdown();

private:
    inline std::string ConvertWStringToUTF8(_In_ std::wstring s, _In_ size_t maxByteCount)
    {
        // this is deprecated, and removed in C++ 23, but there's currently no replacement
        // there's also apparently a memory leak in this in the stdlib

#pragma warning (push)
#pragma warning (disable: 4996)
        std::wstring_convert<std::codecvt_utf8<wchar_t>> convert;
        auto utf8 = convert.to_bytes(s).substr(0, maxByteCount);
#pragma warning (pop)

        return utf8;
    }

    inline void WritePaddedString(_In_ std::string s, _In_ size_t maxByteCount)
    {
        size_t count{ 0 };

        for (auto const& ch : s)
        {
            m_dataWriter.WriteByte(ch);
            count++;

            // we assume maxByteCount is a multiple of sizeof(uint32_t)
            if (count == maxByteCount) return;
        }

        if (count % sizeof(uint32_t) != 0)
        {
            for (int i = 0; i < sizeof(uint32_t) - (count % sizeof(uint32_t)); i++)
            {
                m_dataWriter.WriteByte(0);
            }
        }
    }

    // 255 is largest this can be
    inline byte CalculatePaddedStringSizeIn32BitWords(_In_ std::string s, _In_ size_t maxByteCount)
    {
        if (maxByteCount > 255 * 4)
        {
            maxByteCount = 255 * 4;
        }

        if (s.size() >= maxByteCount)
        {
            return static_cast<byte>(maxByteCount / sizeof(uint32_t));
        }

        if (s.size() % sizeof(uint32_t) != 0)
        {
            // round up to next 32-bit word
            return static_cast<byte>(s.size() / sizeof(uint32_t) + 1);
        }
        else
        {
            return static_cast<byte>(s.size() / sizeof(uint32_t));
        }
    }

    HRESULT InternalWriteCommandHeader(_In_ MidiNetworkCommandCode commandCode, _In_ byte payloadLength, _In_ uint16_t commandSpecificData);
    HRESULT InternalWriteCommandHeader(_In_ MidiNetworkCommandCode commandCode, _In_ byte payloadLength, _In_ byte commandSpecificData1, _In_ byte commandSpecificData2);


    // this handles wrapping the numbers around. Could just store a uint64_t and use % instead

    winrt::Windows::Storage::Streams::IOutputStream m_stream{ nullptr };
    winrt::Windows::Storage::Streams::DataWriter m_dataWriter{ nullptr };


};