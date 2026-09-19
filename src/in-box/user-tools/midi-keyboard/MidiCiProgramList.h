// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace midikeyboard
{
    // One selectable program on the device, as declared in a MIDI-CI ProgramList Resource.
    struct ProgramListEntry
    {
        std::wstring Title{};

        // Straight from the "bankPC" array. M2-107-UM section 2.3 says these three values go on
        // the wire exactly as they appear here, so nothing is adjusted for display.
        uint8_t BankMsb{ 0 };
        uint8_t BankLsb{ 0 };
        uint8_t ProgramChange{ 0 };

        // "tags" from the entry, joined for display. A sound set commonly gives a program and
        // its bank variation the same title, so this is often the only thing telling them apart.
        std::wstring Tags{};

        // "Factory Presets", "GM2 Programs" and so on, from the ChannelList link that led here.
        // Empty when the device only offers one collection.
        std::wstring CollectionTitle{};
    };

    enum class ProgramListResult : int32_t
    {
        Success = 0,

        // the device never answered Discovery, so it is not a MIDI-CI device at all
        NoResponse = 1,

        // it answered, but does not offer Property Exchange or has no ProgramList
        NotSupported = 2,

        // it answered and offered a list, but nothing usable came back
        Empty = 3
    };

    // Asks an endpoint for its program list over MIDI-CI Property Exchange.
    //
    // The exchange itself belongs to Windows.Devices.Midi2.CapabilityInquiry: this walks a device's
    // ChannelList to find which program lists the transmitting channel can actually select from,
    // then fetches each of them. Everything underneath, the identifier, the request numbering, the
    // chunk reassembly, the paging and the timeouts, is the API's business.
    //
    // Results arrive on the completion handler, which runs on a background thread; callers needing
    // the UI thread must marshal for themselves.
    //
    // One instance handles one query. Create it, Start it, and let it go once the handler fires.
    class MidiCiProgramListQuery : public std::enable_shared_from_this<MidiCiProgramListQuery>
    {
    public:
        using CompletedHandler = std::function<void(ProgramListResult, std::vector<ProgramListEntry>)>;

        // channel is the 0-15 index the app transmits on; the ChannelList is filtered by it so
        // the programs offered are the ones that channel can actually select
        static std::shared_ptr<MidiCiProgramListQuery> Start(
            _In_ winrt::Windows::Devices::Midi2::MidiEndpointConnection const& connection,
            _In_ uint8_t group,
            _In_ uint8_t channel,
            _In_ CompletedHandler handler) noexcept;

        // Safe to call from any thread and more than once. The handler will not run afterwards.
        void Cancel() noexcept;

        ~MidiCiProgramListQuery() noexcept;

        // How long the device has to answer each step before the query gives up.
        static constexpr uint32_t StepTimeoutMilliseconds = 2000;

    private:
        void Begin(
            _In_ winrt::Windows::Devices::Midi2::MidiEndpointConnection const& connection,
            _In_ uint8_t group,
            _In_ uint8_t channel,
            _In_ CompletedHandler handler) noexcept;

        // The whole exchange, start to finish, on a background thread. Every step waits for the
        // device's answer, so none of this may run on the user interface thread.
        void Run() noexcept;

        // Which program lists the transmitting channel can select from. Empty when the device does
        // not publish a ChannelList, which is not an error: a device with a single list needs no
        // map from channels to lists.
        std::vector<winrt::Windows::Devices::Midi2::CapabilityInquiry::MidiResourceLink>
            ProgramListLinksForChannel(
                _In_ winrt::Windows::Devices::Midi2::CapabilityInquiry::MidiChannelList const& channelList) noexcept;

        // Returns how many usable rows this list added.
        int32_t CollectPrograms(
            _In_ winrt::Windows::Devices::Midi2::CapabilityInquiry::MidiProgramList const& programList,
            _In_ std::wstring const& collectionTitle,
            _In_ bool const labelWithCollection) noexcept;

        void Complete(_In_ ProgramListResult result) noexcept;

        std::mutex m_lock{};

        winrt::Windows::Devices::Midi2::CapabilityInquiry::MidiCapabilityInquirySession m_session{ nullptr };

        uint8_t m_group{ 0 };
        uint8_t m_channel{ 0 };

        CompletedHandler m_handler{};

        std::atomic<bool> m_canceled{ false };
        std::atomic<bool> m_completed{ false };

        std::vector<ProgramListEntry> m_entries{};

        // Keeps the object alive for the length of the exchange. Released by Complete.
        std::shared_ptr<MidiCiProgramListQuery> m_self{};
    };
}
