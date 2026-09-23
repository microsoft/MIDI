// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"

#include "MidiCiPresence.h"
#include "Telemetry.h"

#include <MidiDefs.h>

using namespace winrt::Windows::Devices::Midi2;
using namespace winrt::Windows::Devices::Midi2::CapabilityInquiry;
using namespace winrt::Windows::Devices::Midi2::Enumeration;

namespace midikeyboard
{
    namespace
    {
        // What this app says it is, to anything that asks and to anything it asks. Model numbers
        // are registered in MidiDefs.h so no two Microsoft endpoints claim the same one.
        MidiDeclaredDeviceIdentity KeyboardIdentity() noexcept
        {
            try
            {
                return MidiDeclaredDeviceIdentity{
                    MIDI_MANUFACTURER_SYSEX_ID_MICROSOFT_BYTE1,
                    MIDI_MANUFACTURER_SYSEX_ID_MICROSOFT_BYTE2,
                    MIDI_MANUFACTURER_SYSEX_ID_MICROSOFT_BYTE3,
                    static_cast<uint8_t>(MIDI_DEVICE_FAMILY_WINDOWS_11 & 0x7F),
                    static_cast<uint8_t>((MIDI_DEVICE_FAMILY_WINDOWS_11 >> 7) & 0x7F),
                    static_cast<uint8_t>(MIDI_DEVICE_FAMILY_MODEL_NUMBER_MIDI_KEYBOARD & 0x7F),
                    static_cast<uint8_t>((MIDI_DEVICE_FAMILY_MODEL_NUMBER_MIDI_KEYBOARD >> 7) & 0x7F),
                    1, 0, 0, 0 };
            }
            catch (...)
            {
                return nullptr;
            }
        }
    }

    _Use_decl_annotations_
    void MidiCiPresence::Open(
        MidiEndpointConnection const& connection,
        uint8_t group,
        ResponderAppearedHandler responderAppeared) noexcept
    {
        Close();

        try
        {
            if (connection == nullptr)
            {
                return;
            }

            auto session = MidiCapabilityInquirySession::Create(connection);

            if (session == nullptr)
            {
                return;
            }

            session.Group(MidiGroup(group));
            session.Identity(KeyboardIdentity());

            // This app asks questions but answers none of them, and now that a session can say so
            // it should. Declaring property exchange would invite requests it cannot serve.
            session.SupportedCategories(MidiCapabilityInquiryCategories::None);

            {
                std::lock_guard<std::mutex> guard(m_lock);

                m_connection = connection;
                m_session = session;
                m_responderAppeared = responderAppeared;
                m_knownResponders.clear();
            }

            AnswerDiscovery();
            WatchForNewResponders();
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to start answering capability inquiry.")
    }

    void MidiCiPresence::Close() noexcept
    {
        try
        {
            MidiCapabilityInquirySession session{ nullptr };
            winrt::event_token discoveryToken{};
            winrt::event_token responderToken{};

            {
                std::lock_guard<std::mutex> guard(m_lock);

                session = m_session;
                discoveryToken = m_discoveryToken;
                responderToken = m_responderFoundToken;

                m_session = nullptr;
                m_connection = nullptr;
                m_discoveryToken = {};
                m_responderFoundToken = {};
                m_responderAppeared = nullptr;
                m_knownResponders.clear();
            }

            if (session != nullptr)
            {
                if (discoveryToken.value != 0)
                {
                    session.MessageReceived(discoveryToken);
                }

                if (responderToken.value != 0)
                {
                    session.ResponderFound(responderToken);
                }

                // Withdraws the identifier, which is what tells everything else this app has gone.
                session.Close();
            }
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to stop answering capability inquiry.")
    }

    MidiCapabilityInquirySession MidiCiPresence::Session() const noexcept
    {
        try
        {
            std::lock_guard<std::mutex> guard(m_lock);
            return m_session;
        }
        catch (...)
        {
            return nullptr;
        }
    }

    _Use_decl_annotations_
    void MidiCiPresence::Group(uint8_t group) noexcept
    {
        try
        {
            auto const session = Session();

            if (session != nullptr)
            {
                session.Group(MidiGroup(group));
            }
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to change the capability inquiry group.")
    }

    _Use_decl_annotations_
    void MidiCiPresence::RememberResponders(
        winrt::Windows::Foundation::Collections::IVectorView<MidiCapabilityInquiryResponder> const& responders) noexcept
    {
        try
        {
            if (responders == nullptr)
            {
                return;
            }

            std::lock_guard<std::mutex> guard(m_lock);

            for (auto const& responder : responders)
            {
                if (responder != nullptr && responder.Muid() != nullptr)
                {
                    m_knownResponders.insert(responder.Muid().AsCombined28BitValue());
                }
            }
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to record which devices answered.")
    }

    _Use_decl_annotations_
    void MidiCiPresence::SuppressAppeared(bool value) noexcept
    {
        m_suppressAppeared = value;
    }

    void MidiCiPresence::AnswerDiscovery() noexcept
    {
        try
        {
            auto const session = Session();
            auto const connection = [this]()
            {
                std::lock_guard<std::mutex> guard(m_lock);
                return m_connection;
            }();

            if (session == nullptr || connection == nullptr)
            {
                return;
            }

            auto const token = session.MessageReceived(
                [connection](MidiCapabilityInquirySession const& sender,
                             MidiCapabilityInquiryMessageReceivedEventArgs const& args)
                {
                    try
                    {
                        if (args == nullptr || connection == nullptr)
                        {
                            return;
                        }

                        auto const message = args.Message();

                        if (message == nullptr || !message.IsValid() ||
                            message.MessageType() != MidiCapabilityInquiryMessageType::Discovery)
                        {
                            return;
                        }

                        // The same identifier and the same claim the session makes when it
                        // announces itself, so one device does not describe itself two ways.
                        auto const messages = MidiCapabilityInquiryMessageBuilder::BuildDiscoveryReply(
                            0,
                            args.Group(),
                            sender.SourceMuid(),
                            message.SourceMuid(),
                            sender.Identity(),
                            sender.SupportedCategories(),
                            sender.ReceivableMaximumSystemExclusiveSize(),
                            message.OutputPathId(),
                            0);

                        if (messages == nullptr || messages.Size() == 0)
                        {
                            return;
                        }

                        auto packets = winrt::single_threaded_vector<IMidiUniversalPacket>();

                        for (auto const& reply : messages)
                        {
                            packets.Append(reply);
                        }

                        (void)connection.SendMultipleMessagesPacketList(packets);
                    }
                    catch (...)
                    {
                    }
                });

            std::lock_guard<std::mutex> guard(m_lock);
            m_discoveryToken = token;
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to answer a capability inquiry discovery.")
    }

    void MidiCiPresence::WatchForNewResponders() noexcept
    {
        try
        {
            auto const session = Session();

            if (session == nullptr)
            {
                return;
            }

            auto const token = session.ResponderFound(
                [this](auto&&, MidiCapabilityInquiryResponder const& responder)
                {
                    try
                    {
                        if (responder == nullptr || responder.Muid() == nullptr)
                        {
                            return;
                        }

                        ResponderAppearedHandler handler{};

                        {
                            std::lock_guard<std::mutex> guard(m_lock);

                            // A device that was already here is not news, and acting on it would
                            // start a query again for nothing.
                            if (!m_knownResponders.insert(
                                    responder.Muid().AsCombined28BitValue()).second)
                            {
                                return;
                            }

                            handler = m_responderAppeared;
                        }

                        // Recorded either way above, so that when the query finishes this device
                        // is already known and does not look like an arrival later.
                        if (handler != nullptr && !m_suppressAppeared)
                        {
                            handler();
                        }
                    }
                    catch (...)
                    {
                    }
                });

            std::lock_guard<std::mutex> guard(m_lock);
            m_responderFoundToken = token;
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Unable to watch for devices arriving.")
    }
}
