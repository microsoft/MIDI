// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#pragma push_macro("SendMessage")
#undef SendMessage

// Hosts the synthesizer inside the service.
//
// Audio is held only while a client is connected, so an idle machine owns no audio device at all.
// A session 0 service renders to the same endpoint the logged on user hears; that was measured
// rather than assumed, which is why there is no separate render host process here.
class MidiSynthDevice
{
public:
    MidiSynthDevice();
    ~MidiSynthDevice();

    MidiSynthDevice(const MidiSynthDevice&) = delete;
    MidiSynthDevice& operator=(const MidiSynthDevice&) = delete;

    // The service opens one bidirectional connection per endpoint and fans clients out above it,
    // so this pair is also the "something is listening" signal. It can open a second one before
    // the first has closed, though: a client that holds the endpoint open while the synthesizer is
    // turned off keeps the old connection alive, and turning it back on opens a new one. So a
    // connection replaces whatever was connected before and gets an identifier, and a disconnect
    // does nothing unless its identifier is still the current connection.
    HRESULT ConnectClient(_In_ IMidiCallback* callback, _In_ LONGLONG context, _Out_ uint64_t& connectionId);
    HRESULT DisconnectClient(_In_ uint64_t connectionId) noexcept;

    HRESULT SendMessage(
        _In_ MessageOptionFlags optionFlags,
        _In_ PVOID message,
        _In_ UINT size,
        _In_ LONGLONG position) noexcept;

    // Safe at any time. Restarts audio only when a setting that shapes the stream changed.
    HRESULT ApplySettings(_In_ MidiSynthSettings const& settings) noexcept;

    MidiSynthSettings Settings() const noexcept;

    // Held across reading the settings, changing them and applying the result. The service doesn't
    // serialize configuration requests, so two arriving together would otherwise both start from
    // the same settings, and the second would undo the first. ApplySettings takes it too, and the
    // thread holding it can take it again.
    [[nodiscard]] wil::cs_leave_scope_exit LockSettingsChanges() noexcept;

    // Reports what the active sound set holds. The full melodic list is deliberately left out:
    // MIDI-CI Property Exchange already carries it as ProgramList, and duplicating 235 entries
    // into every configuration response would make a cheap query expensive.
    HRESULT AddSoundSetInfoToResponse(_Inout_ json::JsonObject& responseObject);
    HRESULT AddInstrumentListToResponse(_Inout_ json::JsonObject& responseObject);

    // Live only, and not persisted: a System Reset in a file puts channel 10 back as the only
    // drum channel, so remembering an override would promise something content can overrule.
    HRESULT SetDrumChannel(_In_ uint8_t channel, _In_ bool isDrumChannel) noexcept;

    HRESULT Shutdown() noexcept;

private:
    // Replies are produced on whichever thread dispatched the message, so they are queued here and
    // put on the wire by the worker.
    static constexpr size_t OutboundQueueCapacity = 256;
    using OutboundQueue = MidiSynth::SpscRingBuffer<MidiSynth::QueuedUmp, OutboundQueueCapacity>;

    // The worker queues a MIDI Message Report whole, in one pass. The ring keeps one slot empty.
    static_assert(MidiSynth::UmpDispatcher::MidiMessageReportMaximumPackets <= OutboundQueueCapacity - 1,
        "the largest MIDI Message Report must fit the property queue in one pass");

    class QueuedUmpOutput final : public MidiSynth::IUmpOutput
    {
    public:
        explicit QueuedUmpOutput(_In_ OutboundQueue& queue) noexcept : m_queue(queue) {}

        void SendUmp(
            _In_reads_(wordCount) const uint32_t* words,
            _In_ uint32_t wordCount) noexcept override;

    private:
        OutboundQueue& m_queue;
    };

    // Queues a system exclusive message only once its last packet arrives, so it is never seen in part.
    class WholeMessageUmpOutput final : public MidiSynth::IUmpOutput
    {
    public:
        explicit WholeMessageUmpOutput(_In_ OutboundQueue& queue) noexcept : m_queue(queue) {}

        void SendUmp(
            _In_reads_(wordCount) const uint32_t* words,
            _In_ uint32_t wordCount) noexcept override;

    private:
        // The longest reply the dispatcher sends is ten packets.
        static constexpr size_t MaxHeldPackets = 64;

        OutboundQueue& m_queue;
        std::array<MidiSynth::QueuedUmp, MaxHeldPackets> m_held{};
        size_t m_heldCount{ 0 };
        bool m_overflowed{ false };
    };

    // The sound set is 3.4 MB and cannot change while the service runs, so it is read once on the
    // first connection and kept. A machine that never plays a note never reads it at all.
    HRESULT EnsureSoundSetLoaded();

    // Ends the current connection, whichever it is. Caller must hold m_connectionLock.
    HRESULT DisconnectCurrentClient() noexcept;

    // Everything the synthesizer can do without making a sound: discovery, MIDI-CI, identity and
    // property exchange. Separate from AcquireAudio because the protocol manager is connected for
    // the life of the endpoint, so "a client is connected" cannot mean "hold the audio device".
    void PrimeDispatcher() noexcept;

    HRESULT AcquireAudio();
    void ReleaseAudio() noexcept;

    // Drains messages through the dispatcher when no audio is running, so the endpoint still
    // answers. A channel voice message is left queued, and audio asked for, unless notes are being
    // discarded because no audio device can be had. Caller must hold m_audioLock exclusively and
    // must have checked there is no sink.
    void PumpWithoutAudio(_In_ bool discardNotes) noexcept;

    void WorkerThread() noexcept;
    void ServiceOutbound() noexcept;
    void ServicePropertyRequests() noexcept;
    void ServicePropertyRequestsInner();

    enum class ResourceLookup
    {
        Found = 0,
        HeaderNotJson,
        UnknownResource,
    };

    // What one Get Property Data header asked for. A program list is answered by serializing a
    // page rather than by handing back a stored blob, so it is told apart here.
    struct ResourceRequest
    {
        const std::vector<char>* Blob{ nullptr };
        bool Cacheable{ true };

        bool IsProgramList{ false };
        std::string ResourceId{};
        size_t Offset{ 0 };
        size_t Limit{ SIZE_MAX };
    };

    ResourceLookup ResourceForHeader(
        _In_reads_(headerBytes) const uint8_t* header,
        _In_ uint16_t headerBytes,
        _Out_ ResourceRequest& result);

    // Starting and ending a subscription. The command is in the header JSON, which is why this
    // does not live in the library alongside the subscription table itself.
    void HandleSubscriptionRequest(
        _In_ MidiSynth::UmpDispatcher::PendingPropertyRequest const& request);

    HRESULT DeliverToCallback(_In_ MidiSynth::QueuedUmp const& message) noexcept;

    MidiSynth::DlsCollection m_collection;

    // A client connecting and a configuration request for the sound set can both be first, on
    // different threads. Guards m_soundSetLoaded, and the collection until it is loaded.
    wil::critical_section m_soundSetLock;
    bool m_soundSetLoaded{ false };

    MidiSynth::SynthEngine m_engine;
    MidiSynth::UmpDispatcher m_dispatcher;
    MidiSynth::PropertyExchangeSource m_propertyExchange;

    MidiSynth::UmpInboundQueue m_inbound;

    // Single producer: the render thread while audio runs, the worker while it does not.
    OutboundQueue m_outbound;
    WholeMessageUmpOutput m_output{ m_outbound };

    // Property exchange replies. Only the worker writes and reads this one.
    OutboundQueue m_propertyOutbound;
    QueuedUmpOutput m_propertyOutput{ m_propertyOutbound };

    std::unique_ptr<MidiSynth::WasapiAudioSink> m_sink;
    std::unique_ptr<MidiSynth::UmpRenderSource> m_source;

    // Set when a channel voice message arrives, which is the only thing that needs the audio
    // device. Cleared when the device is let go again after a quiet spell.
    std::atomic<bool> m_audioWanted{ false };
    std::atomic<uint64_t> m_lastChannelVoiceTimestamp{ 0 };

    // When the device was asked for, so a trace can report how long acquiring it took.
    std::atomic<uint64_t> m_audioRequestedTimestamp{ 0 };

    // Guarded by m_audioLock. One bit per channel, so the assignment outlives the engine.
    uint16_t m_drumChannelMask{ 0 };

    // Guarded by m_audioLock. What the engine was last built from, so that reopening the audio
    // device does not rebuild it and throw away everything the song has set up.
    bool m_engineInitialized{ false };
    MidiSynth::SynthMode m_engineSynthMode{};
    MidiSynth::BankSelectMode m_engineBankSelect{};
    uint32_t m_engineSampleRate{ 0 };
    bool m_engineEffectsEnabled{ false };

    // Guards the sink and render source against a settings change or a device loss arriving while
    // a connection is being made or broken.
    mutable wil::srwlock m_audioLock;

    // The inbound ring is single producer. The service is not documented to deliver on one thread,
    // so the producer side is serialized here; the render thread still pops without a lock.
    wil::critical_section m_producerLock;

    // Taken before every other lock here, and held for a whole settings change, including creating
    // or removing the endpoint. Nothing the service calls back into while that happens takes it,
    // which is what makes holding it across those calls safe.
    wil::critical_section m_settingsChangeLock;

    mutable wil::srwlock m_settingsLock;
    MidiSynthSettings m_settings{};

    wil::com_ptr_nothrow<IMidiCallback> m_callback{ nullptr };
    LONGLONG m_callbackContext{ 0 };

    // Connecting and disconnecting arrive on service threads, and can overlap when the service
    // replaces a connection. Taken before m_producerLock and m_audioLock, and never by the worker,
    // which is joined while it is held.
    wil::critical_section m_connectionLock;

    // Guarded by m_connectionLock. Counts connections, so zero is never a live one.
    uint64_t m_connectionId{ 0 };

    std::thread m_worker;
    wil::unique_event_nothrow m_workerStop;
    wil::unique_event_nothrow m_drainedEvent;

    std::atomic<uint64_t> m_droppedInboundCount{ 0 };
    std::atomic<uint64_t> m_droppedOutboundCount{ 0 };

    // Set by the worker when the endpoint went away, for example because the user changed the
    // default playback device. Cleared when audio is next acquired.
    uint64_t m_nextAudioRetryTimestamp{ 0 };

    // The API's generator keeps clear of the reserved MUID range, so do not roll our own.
    uint32_t m_muid{ 0 };
};

#pragma pop_macro("SendMessage")
