// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace midikeyboard
{
    // This app's MIDI-CI presence on one connection.
    //
    // It owns exactly one session, and the session owns the identifier this app is known by. That
    // is the whole point of this class: an identifier belongs to a device for as long as it is
    // there, so it cannot be created and thrown away every time the app wants to ask a question.
    // Everything that asks questions borrows the session from here.
    //
    // It also answers Discovery. An app that only ever asks is invisible to anything that looks,
    // and the specification requires a reply even from a device with nothing to offer.
    class MidiCiPresence
    {
    public:
        // Raised when a device that was not here before answers or announces itself, which is the
        // signal that anything previously asked and not answered is worth asking again. Runs on a
        // background thread.
        using ResponderAppearedHandler = std::function<void()>;

        // Safe to call more than once; an existing session is closed first. Does nothing useful
        // without a connection, which is the case when the app has no destination.
        void Open(
            _In_ winrt::Windows::Devices::Midi2::MidiEndpointConnection const& connection,
            _In_ uint8_t group,
            _In_ ResponderAppearedHandler responderAppeared) noexcept;

        // Withdraws the identifier and stops answering. Safe to call when never opened.
        void Close() noexcept;

        // The session to ask questions with, or null when there is no connection. Borrowers must
        // not close it.
        winrt::Windows::Devices::Midi2::CapabilityInquiry::MidiCapabilityInquirySession
            Session() const noexcept;

        // Which group capability inquiry goes out on. Changing it does not disturb the identifier.
        void Group(_In_ uint8_t group) noexcept;

        // Records who was already here, so that answers to a query this app made itself are not
        // mistaken for a device arriving.
        void RememberResponders(
            _In_ winrt::Windows::Foundation::Collections::IVectorView<
                winrt::Windows::Devices::Midi2::CapabilityInquiry::MidiCapabilityInquiryResponder> const& responders) noexcept;

        // Set around a query of this app's own. Devices answering it are still recorded, but they
        // are not reported as arriving, because they are only answering what was just asked.
        void SuppressAppeared(_In_ bool value) noexcept;

    private:
        void AnswerDiscovery() noexcept;
        void WatchForNewResponders() noexcept;

        mutable std::mutex m_lock{};

        winrt::Windows::Devices::Midi2::MidiEndpointConnection m_connection{ nullptr };
        winrt::Windows::Devices::Midi2::CapabilityInquiry::MidiCapabilityInquirySession m_session{ nullptr };

        winrt::event_token m_discoveryToken{};
        winrt::event_token m_responderFoundToken{};

        ResponderAppearedHandler m_responderAppeared{};
        std::atomic<bool> m_suppressAppeared{ false };

        // Identifiers already seen on this session. Anything outside it is a device that was not
        // there before.
        std::set<uint32_t> m_knownResponders{};
    };
}
