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
    // The whole exchange is driven from the connection's message callback, so this never blocks.
    // Results arrive on the completion handler, which runs on the MIDI callback thread; callers
    // needing the UI thread must marshal for themselves.
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
        enum class Stage : int32_t
        {
            Idle = 0,
            AwaitingDiscoveryReply,
            AwaitingChannelList,
            AwaitingProgramList,
            Finished
        };

        void Begin(
            _In_ winrt::Windows::Devices::Midi2::MidiEndpointConnection const& connection,
            _In_ uint8_t group,
            _In_ uint8_t channel,
            _In_ CompletedHandler handler) noexcept;

        void OnMessageReceived(
            _In_ winrt::Windows::Foundation::IInspectable const& sender,
            _In_ winrt::Windows::Devices::Midi2::MidiMessageReceivedEventArgs const& args) noexcept;

        void OnDiscoveryReply(_In_ uint32_t remoteMuid) noexcept;
        void OnPropertyReply(_In_ std::string const& headerJson, _In_ std::vector<uint8_t> const& body) noexcept;

        void CollectCollectionsFromChannelList(_In_ winrt::Windows::Data::Json::JsonArray const& array) noexcept;

        // returns how many usable rows this reply added, which is what drives paging
        int32_t CollectProgramsFromList(_In_ winrt::Windows::Data::Json::JsonArray const& array) noexcept;

        void RequestChannelList() noexcept;
        void RequestProgramList() noexcept;

        // true when another collection was queued, false when the list is complete
        bool RequestNextCollection() noexcept;

        void SendCiMessage(_In_reads_bytes_(length) uint8_t const* bytes, _In_ uint16_t length) noexcept;
        void SendPropertyGet(_In_ std::string const& headerJson) noexcept;

        void Complete(_In_ ProgramListResult result) noexcept;
        void ArmTimeout() noexcept;

        std::recursive_mutex m_lock{};

        winrt::Windows::Devices::Midi2::MidiEndpointConnection m_connection{ nullptr };
        winrt::event_token m_messageToken{};

        uint8_t m_group{ 0 };
        uint8_t m_channel{ 0 };
        CompletedHandler m_handler{};

        Stage m_stage{ Stage::Idle };
        uint32_t m_localMuid{ 0 };
        uint32_t m_remoteMuid{ 0 };
        uint8_t m_requestId{ 0 };

        // reassembly across Property Exchange chunks
        std::string m_replyHeader{};
        std::vector<uint8_t> m_replyBody{};

        // one entry per ProgramList collection still to be fetched
        struct PendingCollection
        {
            std::string ResourceId;
            std::wstring Title;
        };
        std::vector<PendingCollection> m_pendingCollections{};
        std::wstring m_currentCollectionTitle{};
        std::string m_currentResourceId{};

        // paging within the collection being fetched
        int32_t m_offset{ 0 };
        int32_t m_totalCount{ 0 };

        std::vector<ProgramListEntry> m_entries{};

        winrt::Windows::System::Threading::ThreadPoolTimer m_timeoutTimer{ nullptr };
        uint32_t m_timeoutGeneration{ 0 };

        // Keeps the object alive for the length of the exchange. Released by Complete.
        std::shared_ptr<MidiCiProgramListQuery> m_self{};
    };
}
