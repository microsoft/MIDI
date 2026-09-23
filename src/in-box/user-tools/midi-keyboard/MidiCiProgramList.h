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

        // "category" from the entry. M2-107-UM Appendix A suggests names but says a device is not
        // limited to them, so these are shown as they arrive rather than matched against a list.
        std::vector<std::wstring> Categories{};

        // "Factory Presets", "GM2 Programs" and so on, from the ChannelList link that led here.
        // Empty when the device only offers one collection.
        std::wstring CollectionTitle{};
    };

    // The programs that share one category, in the order the device first mentioned it. A program
    // declaring several categories appears under each of them, which is what the resource means.
    struct ProgramCategoryGroup
    {
        std::wstring Name{};
        std::vector<size_t> EntryIndexes{};
    };

    // Groups a program list by category. Comes back empty when no program carried one, which is
    // how a caller knows not to offer the grouped view at all. Programs with no category of their
    // own are collected under otherName, but only when some other program did have one.
    std::vector<ProgramCategoryGroup> GroupProgramsByCategory(
        _In_ std::vector<ProgramListEntry> const& entries,
        _In_ std::wstring const& otherName) noexcept;

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
    // The session is BORROWED, never closed. It belongs to MidiCiPresence and outlives any number
    // of queries, because the identifier it carries is how this app is known on the wire and must
    // not change every time a question is asked.
    //
    // Results arrive on the completion handler, which runs on a background thread; callers needing
    // the UI thread must marshal for themselves.
    //
    // One instance handles one query. Create it, Start it, and let it go once the handler fires.
    class MidiCiProgramListQuery : public std::enable_shared_from_this<MidiCiProgramListQuery>
    {
    public:
        using CompletedHandler = std::function<void(ProgramListResult, std::vector<ProgramListEntry>)>;

        // Raised when the device says the programs available on this channel have changed, which
        // happens on a workstation or a DAW when a different instrument is selected. Runs on a
        // background thread. Only ever called when the device supports being subscribed to.
        using ChangedHandler = std::function<void()>;

        // channel is the 0-15 index the app transmits on; the ChannelList is filtered by it so
        // the programs offered are the ones that channel can actually select
        //
        // Pass a changed handler to be told when the answer stops being true. Doing so keeps the
        // query alive after its result arrives, so the caller must hold on to it and Cancel it.
        static std::shared_ptr<MidiCiProgramListQuery> Start(
            _In_ winrt::Windows::Devices::Midi2::CapabilityInquiry::MidiCapabilityInquirySession const& session,
            _In_ uint8_t channel,
            _In_ CompletedHandler handler,
            _In_ ChangedHandler changed) noexcept;

        // Safe to call from any thread and more than once. The handler will not run afterwards.
        void Cancel() noexcept;

        ~MidiCiProgramListQuery() noexcept;

        // How long the device has to answer each step before the query gives up.
        static constexpr uint32_t StepTimeoutMilliseconds = 2000;

    private:
        void Begin(
            _In_ winrt::Windows::Devices::Midi2::CapabilityInquiry::MidiCapabilityInquirySession const& session,
            _In_ uint8_t channel,
            _In_ CompletedHandler handler,
            _In_ ChangedHandler changed) noexcept;

        // The whole exchange, start to finish, on a background thread. Every step waits for the
        // device's answer, so none of this may run on the user interface thread.
        void Run() noexcept;

        // Which program lists the transmitting channel can select from. Empty when the device does
        // not publish a ChannelList, which is not an error: a device with a single list needs no
        // map from channels to lists.
        std::vector<winrt::Windows::Devices::Midi2::CapabilityInquiry::MidiResourceLink>
            ProgramListLinksForChannel(
                _In_ winrt::Windows::Devices::Midi2::CapabilityInquiry::MidiChannelList const& channelList) noexcept;

        // The same links reduced to one comparable string. A channel list update that leaves this
        // alone changed something the program list does not depend on.
        static std::wstring LinkSignature(
            _In_ std::vector<winrt::Windows::Devices::Midi2::CapabilityInquiry::MidiResourceLink> const& links) noexcept;

        // True when an update changed which collections this channel can select from, and so the
        // fetched list has to be thrown away. An update carrying no data is treated as a change,
        // because there is nothing to compare and guessing wrong loses the customer's list.
        bool ChannelLinksChanged(
            _In_ winrt::Windows::Devices::Midi2::CapabilityInquiry::MidiPropertySubscriptionUpdatedEventArgs const& args) noexcept;

        // Returns how many usable rows this list added.
        int32_t CollectPrograms(
            _In_ winrt::Windows::Devices::Midi2::CapabilityInquiry::MidiProgramList const& programList,
            _In_ std::wstring const& collectionTitle,
            _In_ bool const labelWithCollection) noexcept;

        void Complete(_In_ ProgramListResult result) noexcept;

        // Asks the device to tell us when the channel list changes. Does nothing unless the
        // caller wants to know and the device says it can.
        void WatchChannelList(
            _In_ winrt::Windows::Devices::Midi2::CapabilityInquiry::MidiCapabilityInquirySession const& session,
            _In_ winrt::Windows::Devices::Midi2::CapabilityInquiry::MidiUniqueId const& muid) noexcept;

        std::mutex m_lock{};

        winrt::Windows::Devices::Midi2::CapabilityInquiry::MidiCapabilityInquirySession m_session{ nullptr };

        uint8_t m_group{ 0 };
        uint8_t m_channel{ 0 };

        CompletedHandler m_handler{};
        ChangedHandler m_changedHandler{};

        // Set once the device has accepted a subscription. The session is borrowed, so this only
        // records that there is a subscription on it to be ended in Cancel.
        winrt::event_token m_subscriptionToken{};
        std::atomic<bool> m_watching{ false };

        // What the fetched list was built from, so an update can be ignored when it does not
        // change it. A device sends one for every bank or program change on any channel.
        std::wstring m_linkSignature{};

        std::atomic<bool> m_canceled{ false };
        std::atomic<bool> m_completed{ false };

        std::vector<ProgramListEntry> m_entries{};

        // Keeps the object alive for the length of the exchange. Released by Complete.
        std::shared_ptr<MidiCiProgramListQuery> m_self{};
    };
}
