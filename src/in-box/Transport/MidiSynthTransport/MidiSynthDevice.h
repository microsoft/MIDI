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

    // The service opens exactly one bidirectional connection per endpoint and fans clients out
    // above it, so this pair is also the "something is listening" signal.
    HRESULT ConnectClient(_In_ IMidiCallback* callback, _In_ LONGLONG context);
    HRESULT DisconnectClient() noexcept;

    HRESULT SendMessage(
        _In_ MessageOptionFlags optionFlags,
        _In_ PVOID message,
        _In_ UINT size,
        _In_ LONGLONG position) noexcept;

    // Safe at any time. Restarts audio only when a setting that shapes the stream changed.
    HRESULT ApplySettings(_In_ MidiSynthSettings const& settings) noexcept;

    MidiSynthSettings Settings() const noexcept;

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
    using OutboundQueue = MidiSynth::SpscRingBuffer<MidiSynth::QueuedUmp, 256>;

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

    // The sound set is 3.4 MB and cannot change while the service runs, so it is read once on the
    // first connection and kept. A machine that never plays a note never reads it at all.
    HRESULT EnsureSoundSetLoaded();

    // Everything the synthesizer can do without making a sound: discovery, MIDI-CI, identity and
    // property exchange. Separate from AcquireAudio because the protocol manager is connected for
    // the life of the endpoint, so "a client is connected" cannot mean "hold the audio device".
    void PrimeDispatcher() noexcept;

    HRESULT AcquireAudio();
    void ReleaseAudio() noexcept;

    // Drains messages through the dispatcher when no audio is running, so the endpoint still
    // answers. Caller must hold m_audioLock exclusively and must have checked there is no sink.
    void PumpWithoutAudio() noexcept;

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

    ResourceLookup ResourceForHeader(
        _In_reads_(headerBytes) const uint8_t* header,
        _In_ uint16_t headerBytes,
        _Outptr_result_maybenull_ const std::vector<char>** blob,
        _Out_ bool& cacheable);

    HRESULT DeliverToCallback(_In_ MidiSynth::QueuedUmp const& message) noexcept;

    MidiSynth::DlsCollection m_collection;
    bool m_soundSetLoaded{ false };

    MidiSynth::SynthEngine m_engine;
    MidiSynth::UmpDispatcher m_dispatcher;
    MidiSynth::PropertyExchangeSource m_propertyExchange;

    MidiSynth::UmpInboundQueue m_inbound;
    OutboundQueue m_outbound;
    QueuedUmpOutput m_output{ m_outbound };

    std::unique_ptr<MidiSynth::WasapiAudioSink> m_sink;
    std::unique_ptr<MidiSynth::UmpRenderSource> m_source;

    // Set when a channel voice message arrives, which is the only thing that needs the audio
    // device. Cleared when the device is let go again after a quiet spell.
    std::atomic<bool> m_audioWanted{ false };
    std::atomic<uint64_t> m_lastChannelVoiceTimestamp{ 0 };

    // Guarded by m_audioLock. One bit per channel, so the assignment outlives the engine.
    uint16_t m_drumChannelMask{ 0 };

    // Guards the sink and render source against a settings change or a device loss arriving while
    // a connection is being made or broken.
    mutable wil::srwlock m_audioLock;

    // The inbound ring is single producer. The service is not documented to deliver on one thread,
    // so the producer side is serialized here; the render thread still pops without a lock.
    wil::critical_section m_producerLock;

    mutable wil::srwlock m_settingsLock;
    MidiSynthSettings m_settings{};

    wil::com_ptr_nothrow<IMidiCallback> m_callback{ nullptr };
    LONGLONG m_callbackContext{ 0 };

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
