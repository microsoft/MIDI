// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiPanic.h"
#include "StringResources.h"

namespace midisettings
{
    namespace
    {
        // The three universal "stop everything" controllers, in the order a receiver is most
        // likely to act on them usefully.
        constexpr uint8_t ControllerAllSoundOff = 120;
        constexpr uint8_t ControllerResetAllControllers = 121;
        constexpr uint8_t ControllerAllNotesOff = 123;

        std::mutex g_sessionLock{};
        midi2::MidiSession g_session{ nullptr };

        std::wstring DescribeSendResult(midi2::MidiSendMessageResults const result) noexcept
        {
            auto const has = [result](midi2::MidiSendMessageResults const flag)
                {
                    return (static_cast<uint32_t>(result) & static_cast<uint32_t>(flag)) != 0;
                };

            if (has(midi2::MidiSendMessageResults::BufferFull))
            {
                return L"The endpoint's send buffer is full.";
            }

            if (has(midi2::MidiSendMessageResults::TransmissionWordCountExceeded))
            {
                return L"Too many messages were sent at once.";
            }

            if (has(midi2::MidiSendMessageResults::EndpointConnectionClosedOrInvalid))
            {
                return L"The connection to the endpoint was closed.";
            }

            return std::format(L"The MIDI service reported 0x{:08X}.", static_cast<uint32_t>(result));
        }

        midi2::MidiSession EnsureSession() noexcept
        {
            try
            {
                std::scoped_lock lock{ g_sessionLock };

                if (g_session == nullptr)
                {
                    g_session = midi2::MidiSession::Create(resources::GetString(L"AppTitle"));
                }

                return g_session;
            }
            MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to create the MIDI session.")

            return nullptr;
        }

        void DropSession() noexcept
        {
            try
            {
                std::scoped_lock lock{ g_sessionLock };

                if (g_session != nullptr)
                {
                    g_session.Close();
                    g_session = nullptr;
                }
            }
            catch (...)
            {
            }
        }
    }

    _Use_decl_annotations_
    bool SendMidiPanic(
        winrt::hstring const& endpointDeviceId,
        std::vector<uint8_t> const& groupIndexes,
        std::wstring& errorMessage) noexcept
    {
        errorMessage.clear();

        midi2::MidiEndpointConnection connection{ nullptr };
        midi2::MidiSession session{ nullptr };

        try
        {
            if (endpointDeviceId.empty() || groupIndexes.empty())
            {
                errorMessage = L"No endpoint was selected.";
                return false;
            }

            if (!midi2::MidiApi::EnsureServiceAvailable())
            {
                errorMessage = L"The MIDI service is not available.";
                return false;
            }

            // A session cached from before a service restart is still an object but no longer
            // talks to anything, so one retry on a fresh session is worth more than an error.
            for (int attempt = 0; attempt < 2 && connection == nullptr; attempt++)
            {
                if (attempt > 0)
                {
                    DropSession();
                }

                session = EnsureSession();

                if (session == nullptr)
                {
                    continue;
                }

                auto candidate = session.CreateEndpointConnection(endpointDeviceId);

                if (candidate != nullptr && candidate.Open())
                {
                    connection = candidate;
                }
            }

            if (connection == nullptr)
            {
                errorMessage = session == nullptr ?
                    L"A MIDI session could not be started." : L"The endpoint could not be opened.";

                return false;
            }

            bool succeeded{ true };

            // One group per send. The service rejects an oversized list outright with
            // TransmissionWordCountExceeded, and 16 channels x 3 controllers is well inside
            // what it will take.
            std::vector<uint32_t> words{};
            words.reserve(16 * 3);

            for (auto const group : groupIndexes)
            {
                words.clear();

                for (uint8_t channel = 0; channel < 16; channel++)
                {
                    for (auto const controller :
                        { ControllerAllSoundOff, ControllerResetAllControllers, ControllerAllNotesOff })
                    {
                        words.push_back(midi2msg::MidiMessageBuilder::BuildMidi1ChannelVoiceMessage(
                            midi2::MidiClock::TimestampConstantSendImmediately(),
                            midi2::MidiGroup{ group },
                            midi2msg::Midi1ChannelVoiceMessageStatus::ControlChange,
                            midi2::MidiChannel{ channel },
                            controller,
                            0).Word0());
                    }
                }

                auto const result = connection.SendMultipleMessagesWordList(
                    midi2::MidiClock::TimestampConstantSendImmediately(), words);

                if (!midi2::MidiEndpointConnection::SendMessageSucceeded(result))
                {
                    errorMessage = DescribeSendResult(result);
                    succeeded = false;
                    break;
                }
            }

            session.DisconnectEndpointConnection(connection.ConnectionId());
            connection = nullptr;

            return succeeded;
        }
        catch (winrt::hresult_error const& ex)
        {
            errorMessage = ex.message();
            MIDI_SETTINGS_LOG_HRESULT_EXCEPTION(ex, L"Unable to send MIDI panic.");
        }
        catch (...)
        {
            errorMessage = L"An unexpected error occurred.";
            MIDI_SETTINGS_LOG_GENERAL_EXCEPTION(L"Unable to send MIDI panic.");
        }

        try
        {
            if (connection != nullptr && session != nullptr)
            {
                session.DisconnectEndpointConnection(connection.ConnectionId());
            }
        }
        catch (...)
        {
        }

        return false;
    }

    void ShutDownPanicSession() noexcept
    {
        try
        {
            std::scoped_lock lock{ g_sessionLock };

            if (g_session != nullptr)
            {
                g_session.Close();
                g_session = nullptr;
            }
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to close the MIDI session.")
    }
}
