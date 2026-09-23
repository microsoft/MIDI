// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"

#include <random>

using namespace MidiSynth;

#pragma push_macro("SendMessage")
#undef SendMessage

namespace
{
    // How often the worker looks for a parked property request or a lost audio device. Outbound
    // replies do not wait for this: the render thread signals the worker when it queues one.
    constexpr DWORD WorkerPollIntervalMilliseconds = 10;

    // The endpoint can go away while a client is connected, most often because the user changed
    // the default playback device. Reopening immediately on every failure would spin.
    constexpr uint64_t AudioRetryIntervalMilliseconds = 1000;

    // How long the synthesizer stays silent before it lets the audio device go. Long enough that
    // the gaps in a performance never reach it, short enough that a finished song does.
    constexpr uint64_t AudioIdleReleaseMilliseconds = 5000;

    // Long enough for the fade a voice ends with, short enough that a disconnect never hangs.
    constexpr DWORD DrainTimeoutMilliseconds = 100;

    std::wstring SystemSoundSetPath()
    {
        wchar_t systemDirectory[MAX_PATH]{};

        if (GetSystemDirectoryW(systemDirectory, ARRAYSIZE(systemDirectory)) == 0)
        {
            return {};
        }

        return std::wstring{ systemDirectory } + L"\\drivers\\gm.dls";
    }

    uint32_t CreateRandomMuid() noexcept
    {
        try
        {
            // Draw from the whole range below the reserved block so neither a reserved value nor
            // the broadcast value can come out. std::rand cannot do this: it stops at 32767, a
            // small fraction of the 28 bits the collision odds in the specification assume.
            std::random_device generator;

            std::uniform_int_distribution<uint32_t> distribution(
                0, WindowsMidiServicesCapabilityInquiry::MuidReservedStart - 1);

            return distribution(generator);
        }
        catch (...)
        {
            // Without a usable identifier MIDI-CI stays silent, which is correct: answering with
            // one we did not generate properly is worse than not answering.
            return 0;
        }
    }
}


MidiSynthDevice::MidiSynthDevice()
{
    LOG_IF_FAILED(m_workerStop.create(wil::EventOptions::ManualReset));
    LOG_IF_FAILED(m_drainedEvent.create(wil::EventOptions::None));

    m_muid = CreateRandomMuid();
}

MidiSynthDevice::~MidiSynthDevice()
{
    LOG_IF_FAILED(Shutdown());
}


_Use_decl_annotations_
void MidiSynthDevice::QueuedUmpOutput::SendUmp(const uint32_t* words, uint32_t wordCount) noexcept
{
    if (words == nullptr || wordCount == 0 || wordCount > 4)
    {
        return;
    }

    QueuedUmp message{};

    for (uint32_t i = 0; i < wordCount; i++)
    {
        message.Words[i] = words[i];
    }

    message.WordCount = static_cast<uint8_t>(wordCount);

    (void)m_queue.TryPush(message);
}


HRESULT
MidiSynthDevice::EnsureSoundSetLoaded()
{
    if (m_soundSetLoaded)
    {
        return S_OK;
    }

    auto const path = SystemSoundSetPath();

    RETURN_HR_IF(E_UNEXPECTED, path.empty());

    // Only a sound set Windows installed is accepted, checked on the open handle so there is no
    // window between the check and the read. The library supports other paths for offline tools.
    auto const status = DlsCollection::LoadFromFile(
        path, DlsParseLimits{}, SoundSetOrigin::SystemOnly, m_collection);

    if (status != DlsParseStatus::Ok)
    {
        TraceLoggingWrite(
            MidiSynthTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Could not load the system sound set", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(path.c_str(), "path"),
            TraceLoggingInt32(static_cast<int32_t>(status), "status")
        );

        RETURN_HR(HRESULT_FROM_WIN32(ERROR_FILE_CORRUPT));
    }

    m_propertyExchange.Build(m_collection, SynthIdentity{});

    m_soundSetLoaded = true;

    TraceLoggingWrite(
        MidiSynthTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Sound set loaded", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingUInt64(m_collection.Instruments().size(), "instruments"),
        TraceLoggingUInt64(m_collection.Waves().size(), "waves")
    );

    return S_OK;
}


// The synthesizer answers discovery, MIDI-CI, identity and property exchange with no audio device
// open at all. Only a channel voice message needs one, so this is set up as soon as a client
// connects and the device is left alone until there is something to play.
void
MidiSynthDevice::PrimeDispatcher() noexcept
{
    // The engine is not initialized yet, and will not be until a sample rate is known. Its channel
    // map is at power-up defaults from construction and its control entry points all work without
    // a sound set, so the dispatcher can be pointed at it and MIDI-CI answered before there is any
    // audio device. Nothing here opens one.
    m_dispatcher.Initialize(&m_engine, MIDI_SYNTH_GROUP_INDEX, m_muid);
    m_dispatcher.SetOutput(&m_output, SynthIdentity{});

    // What the endpoint answers UMP Stream discovery with. The name is the same resource string the
    // endpoint was created from, so the in-protocol name and the transport supplied name agree.
    auto const discoveryName = internal::Utf8FromWString(
        internal::ResourceGetWString(IDS_ENDPOINT_NAME));

    m_dispatcher.SetEndpointIdentity(discoveryName.c_str(), MIDI_SYNTH_ENDPOINT_UNIQUE_ID_UTF8);
}


// Caller must hold m_audioLock exclusively, and must have checked that there is no sink, because
// the render thread owns the inbound queue whenever there is one.
void
MidiSynthDevice::PumpWithoutAudio() noexcept
{
    QueuedUmp message{};

    // Stops as soon as a channel voice message has been seen, so the note that starts the audio
    // device is still in the queue for the render thread rather than being played to nothing.
    while (!m_audioWanted.load(std::memory_order_acquire) && m_inbound.TryPop(message))
    {
        m_dispatcher.ProcessWords(message.Words, message.WordCount);
    }
}


// Caller must hold m_audioLock exclusively.
HRESULT
MidiSynthDevice::AcquireAudio()
{
    if (m_sink != nullptr)
    {
        return S_OK;
    }

    MidiSynthSettings settings{};
    {
        auto lock = m_settingsLock.lock_shared();
        settings = m_settings;
    }

    if (!settings.Enabled)
    {
        return S_FALSE;
    }

    auto sink = std::make_unique<WasapiAudioSink>();

    if (!sink->Open(settings.WantsLowLatency()))
    {
        TraceLoggingWrite(
            MidiSynthTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_WARNING,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Could not open an audio device. MIDI will flow but nothing will sound.", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );

        return HRESULT_FROM_WIN32(ERROR_DEVICE_NOT_AVAILABLE);
    }

    // Modern mode renders at the device rate, so the audio engine converts nothing.
    auto config = SynthConfig::ForMode(settings.SynthMode, sink->SampleRate());

    config.BankSelect = settings.BankSelect;
    config.EnableEffects = settings.EffectsEnabled && config.EnableEffects;

    // The audio device now comes and goes underneath a connection, and initializing the engine
    // clears every channel: programs, bank, volume, pan and tuning. A song that sets its
    // instruments up and then rests longer than the idle timeout would come back playing pianos.
    // So the engine is only rebuilt when something it was built from actually changed.
    bool const engineMatchesDevice =
        m_engineInitialized &&
        m_engineSynthMode == settings.SynthMode &&
        m_engineSampleRate == sink->SampleRate() &&
        m_engineBankSelect == settings.BankSelect &&
        m_engineEffectsEnabled == config.EnableEffects;

    if (!engineMatchesDevice)
    {
        RETURN_HR_IF(E_FAIL, !m_engine.Initialize(&m_collection, config));

        // The engine keeps its channel map across this, so this only matters for a rhythm channel
        // chosen before the engine had ever been told about it.
        for (uint8_t channel = 0; channel < MidiChannelCount; channel++)
        {
            if ((m_drumChannelMask & (1u << channel)) != 0)
            {
                m_engine.SetDrumChannel(channel, true);
            }
        }

        m_engineInitialized = true;
        m_engineSynthMode = settings.SynthMode;
        m_engineSampleRate = sink->SampleRate();
        m_engineBankSelect = settings.BankSelect;
        m_engineEffectsEnabled = config.EnableEffects;
    }

    m_engine.SetUserVolumeDb(settings.VolumeDecibels);

    auto source = std::make_unique<UmpRenderSource>(
        m_engine, m_dispatcher, m_inbound, sink->SampleRate(), sink->BufferFrames(),
        m_drainedEvent.get());

    if (!sink->Start(source.get()))
    {
        return HRESULT_FROM_WIN32(ERROR_DEVICE_NOT_AVAILABLE);
    }

    // Measured from the message that asked for the device, so a trace answers "how long before the
    // first note sounds" without having to correlate two events by hand.
    auto const requestedAt = m_audioRequestedTimestamp.load(std::memory_order_acquire);
    auto const frequency = internal::GetMidiTimestampFrequency();
    auto const acquiredAt = internal::GetCurrentMidiTimestamp();

    double acquireMilliseconds = 0.0;

    if (requestedAt != 0 && acquiredAt > requestedAt && frequency > 0)
    {
        acquireMilliseconds =
            static_cast<double>(acquiredAt - requestedAt) * 1000.0 / static_cast<double>(frequency);
    }

    TraceLoggingWrite(
        MidiSynthTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Audio acquired", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(sink->DeviceName().c_str(), "device"),
        TraceLoggingUInt32(sink->SampleRate(), "sample rate"),
        TraceLoggingFloat64(sink->PeriodMilliseconds(), "period ms"),
        TraceLoggingBool(sink->UsingLowLatencyPath(), "low latency"),
        TraceLoggingFloat64(acquireMilliseconds, "ms since requested"),
        TraceLoggingBool(!engineMatchesDevice, "engine rebuilt")
    );

    m_source = std::move(source);
    m_sink = std::move(sink);

    return S_OK;
}


// Caller must hold m_audioLock exclusively.
void
MidiSynthDevice::ReleaseAudio() noexcept
{
    if (m_sink == nullptr)
    {
        return;
    }

    // Cutting a ringing voice mid waveform is heard as a click, so everything is faded and the
    // render thread reports that it has gone quiet before the device is released.
    for (uint8_t channel = 0; channel < MidiChannelCount; channel++)
    {
        m_engine.AllSoundOff(channel);
    }

    if (m_source != nullptr && m_drainedEvent)
    {
        m_source->RequestDrain();
        (void)WaitForSingleObject(m_drainedEvent.get(), DrainTimeoutMilliseconds);
    }

    m_sink->Stop();
    m_sink.reset();
    m_source.reset();

    TraceLoggingWrite(
        MidiSynthTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Audio released", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );
}


_Use_decl_annotations_
HRESULT
MidiSynthDevice::ConnectClient(IMidiCallback* callback, LONGLONG context)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, callback);

    TraceLoggingWrite(
        MidiSynthTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    RETURN_IF_FAILED(EnsureSoundSetLoaded());

    // Assigning over a joinable std::thread terminates the process, which here would take MIDI
    // down for the whole machine. The service opens one connection at a time, so this is defense
    // against a surprise rather than an expected path.
    if (m_worker.joinable())
    {
        RETURN_IF_FAILED(DisconnectClient());
    }

    // A previous connection may have left messages nobody consumed. Starting from empty keeps a
    // stale note on from sounding the moment the next client arrives.
    {
        auto lock = m_producerLock.lock();

        QueuedUmp discarded;
        while (m_inbound.TryPop(discarded)) {}
        while (m_outbound.TryPop(discarded)) {}
    }

    m_callback = callback;
    m_callbackContext = context;

    {
        auto lock = m_audioLock.lock_exclusive();

        m_nextAudioRetryTimestamp = 0;
        m_audioWanted.store(false, std::memory_order_release);
        m_lastChannelVoiceTimestamp.store(0, std::memory_order_relaxed);

        // No audio device yet. The service's protocol manager connects to every MIDI 2.0 endpoint
        // and stays connected for its lifetime, so opening the device here would hold it open for
        // as long as the endpoint exists and shut out anything wanting it exclusively.
        PrimeDispatcher();
    }

    m_workerStop.ResetEvent();
    m_worker = std::thread(&MidiSynthDevice::WorkerThread, this);

    return S_OK;
}


HRESULT
MidiSynthDevice::DisconnectClient() noexcept
{
    TraceLoggingWrite(
        MidiSynthTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    // The worker is the only thing that touches the callback, so joining it first is what makes
    // clearing the callback below safe without holding a lock across a call into the service.
    // It is also why nothing here may hold m_audioLock while joining: the worker takes that lock.
    m_workerStop.SetEvent();

    if (m_worker.joinable())
    {
        m_worker.join();
    }

    {
        auto lock = m_audioLock.lock_exclusive();
        ReleaseAudio();
    }

    m_callback.reset();
    m_callbackContext = 0;

    return S_OK;
}


_Use_decl_annotations_
HRESULT
MidiSynthDevice::SendMessage(
    MessageOptionFlags /*optionFlags*/,
    PVOID message,
    UINT size,
    LONGLONG position) noexcept
{
    RETURN_HR_IF_NULL(E_INVALIDARG, message);
    RETURN_HR_IF(E_INVALIDARG, size < sizeof(uint32_t));

    // UMP payloads are 32 bit words, so a size which is not a whole number of them is not the
    // buffer it claims to be.
    RETURN_HR_IF(E_INVALIDARG, (size % sizeof(uint32_t)) != 0);

    auto const* const words = reinterpret_cast<uint32_t const*>(message);
    auto const wordCount = size / sizeof(uint32_t);

    // Zero means deliver now. Anything else is the QPC the service decided this belongs at, which
    // is what lets the render thread place it inside the block instead of at the block boundary.
    int64_t timestamp = position;

    if (timestamp == 0)
    {
        timestamp = static_cast<int64_t>(internal::GetCurrentMidiTimestamp());
    }

    auto lock = m_producerLock.lock();

    for (size_t index = 0; index < wordCount; )
    {
        size_t const length = internal::GetUmpLengthInMidiWordsFromFirstWord(words[index]);

        // A length which does not fit means the rest of the buffer cannot be trusted. Stop rather
        // than walking off the end or spinning on a zero length.
        if (length == 0 || index + length > wordCount)
        {
            RETURN_HR(E_INVALIDARG);
        }

        QueuedUmp queued{};
        queued.WordCount = static_cast<uint8_t>(length);
        queued.Timestamp = timestamp;

        for (size_t word = 0; word < length; word++)
        {
            queued.Words[word] = words[index + word];
        }

        // A channel voice message is the only thing that has to be heard, so it is what calls for
        // the audio device. Flagged before the push, so the worker can never drain a note that
        // arrived before the device was asked for.
        auto const messageType = internal::GetUmpMessageTypeFromFirstWord(queued.Words[0]);

        if (messageType == MIDI_UMP_MESSAGE_TYPE_MIDI1_CHANNEL_VOICE_32 ||
            messageType == MIDI_UMP_MESSAGE_TYPE_MIDI2_CHANNEL_VOICE_64)
        {
            auto const arrivedAt = internal::GetCurrentMidiTimestamp();

            m_lastChannelVoiceTimestamp.store(arrivedAt, std::memory_order_release);

            // Stored after the timestamp, so a worker that is deciding to release always sees the
            // newer timestamp if it sees the flag. Only the transition is traced: this runs for
            // every message, and tracing each one would swamp a capture during a performance.
            if (!m_audioWanted.exchange(true, std::memory_order_acq_rel))
            {
                m_audioRequestedTimestamp.store(arrivedAt, std::memory_order_release);

                TraceLoggingWrite(
                    MidiSynthTransportTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_INFO,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Audio requested by a channel voice message", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingUInt32(static_cast<uint32_t>(messageType), "message type"),
                    TraceLoggingUInt64(arrivedAt, "timestamp")
                );
            }
        }

        if (!m_inbound.TryPush(queued))
        {
            m_droppedInboundCount.fetch_add(1, std::memory_order_relaxed);
        }

        index += length;
    }

    return S_OK;
}


_Use_decl_annotations_
HRESULT
MidiSynthDevice::ApplySettings(MidiSynthSettings const& settings) noexcept
{
    bool enabledChanged{ false };
    bool audioShapeChanged{ false };
    bool liveChanged{ false };

    {
        auto lock = m_settingsLock.lock_exclusive();

        enabledChanged = settings.Enabled != m_settings.Enabled;

        // Only these reshape the stream or reallocate the engine, so only these are worth an
        // audible gap. Volume and bank addressing take effect in place.
        audioShapeChanged =
            settings.SynthMode != m_settings.SynthMode ||
            settings.AudioMode != m_settings.AudioMode ||
            settings.EffectsEnabled != m_settings.EffectsEnabled;

        liveChanged =
            settings.VolumeDecibels != m_settings.VolumeDecibels ||
            settings.BankSelect != m_settings.BankSelect;

        m_settings = settings;
    }

    if (liveChanged && !audioShapeChanged && !enabledChanged)
    {
        auto lock = m_audioLock.lock_exclusive();

        // Applied to the engine rather than to the stream, so this still lands while the audio
        // device is released. Recording the bank mode keeps the next acquisition from deciding the
        // engine is stale and rebuilding it.
        if (m_engineInitialized)
        {
            m_engine.SetUserVolumeDb(settings.VolumeDecibels);
            m_engine.SetBankSelectMode(settings.BankSelect);

            m_engineBankSelect = settings.BankSelect;
        }
    }

    if (enabledChanged)
    {
        // Nothing may be held here. Removing the endpoint makes the service tear the device pipe
        // down, which comes back in through Bidi::Shutdown to DisconnectClient, and that joins the
        // worker and takes the audio lock. Holding either across this call would deadlock.
        auto endpointManager = TransportState::Current().GetEndpointManager();

        if (endpointManager != nullptr && endpointManager->IsInitialized())
        {
            LOG_IF_FAILED(endpointManager->SyncEndpointToSettings());
        }

        // Belt and braces. If the endpoint went away without a client attached, or the service did
        // not close the pipe, the audio device still has to be handed back: releasing it is the
        // whole reason for the off switch.
        if (!settings.Enabled)
        {
            auto lock = m_audioLock.lock_exclusive();
            ReleaseAudio();
        }

        return S_OK;
    }

    if (!audioShapeChanged)
    {
        return S_OK;
    }

    auto lock = m_audioLock.lock_exclusive();

    // Only restart what was already running. A change made while nothing is connected simply
    // applies the next time a client arrives.
    if (m_sink != nullptr)
    {
        ReleaseAudio();
        m_nextAudioRetryTimestamp = 0;

        LOG_IF_FAILED(AcquireAudio());
    }

    return S_OK;
}


MidiSynthSettings
MidiSynthDevice::Settings() const noexcept
{
    auto lock = m_settingsLock.lock_shared();
    return m_settings;
}


_Use_decl_annotations_
HRESULT
MidiSynthDevice::SetDrumChannel(uint8_t channel, bool isDrumChannel) noexcept
{
    RETURN_HR_IF(E_INVALIDARG, channel >= MidiChannelCount);

    auto lock = m_audioLock.lock_exclusive();

    // Remembered whether or not audio is running, because the device is released whenever the
    // synthesizer falls quiet and the engine is rebuilt when it next has something to play.
    if (isDrumChannel)
    {
        m_drumChannelMask |= (1u << channel);
    }
    else
    {
        m_drumChannelMask &= ~(1u << channel);
    }

    // Told to the engine even with no audio device, so a MIDI-CI ChannelList read before anything
    // has played links the channel to the right program list.
    m_engine.SetDrumChannel(channel, isDrumChannel);

    return S_OK;
}


_Use_decl_annotations_
HRESULT
MidiSynthDevice::AddSoundSetInfoToResponse(json::JsonObject& responseObject)
{
    // Answering this must not depend on anything being connected, so the sound set is read here
    // if a client has not already caused it to load.
    RETURN_IF_FAILED(EnsureSoundSetLoaded());

    auto const version = m_collection.Version();

    wchar_t versionText[64]{};
    (void)swprintf_s(versionText, L"%u.%u.%u.%u",
        version.Major, version.Minor, version.Release, version.Build);

    responseObject.SetNamedValue(MIDI_SYNTH_JSON_SOUND_SET_NAME_KEY,
        json::JsonValue::CreateStringValue(m_collection.Name()));

    responseObject.SetNamedValue(MIDI_SYNTH_JSON_SOUND_SET_VERSION_KEY,
        json::JsonValue::CreateStringValue(versionText));

    responseObject.SetNamedValue(MIDI_SYNTH_JSON_SOUND_SET_PATH_KEY,
        json::JsonValue::CreateStringValue(SystemSoundSetPath()));

    responseObject.SetNamedValue(MIDI_SYNTH_JSON_SOUND_SET_INSTRUMENTS_KEY,
        json::JsonValue::CreateNumberValue(static_cast<double>(m_collection.Instruments().size())));

    responseObject.SetNamedValue(MIDI_SYNTH_JSON_SOUND_SET_WAVES_KEY,
        json::JsonValue::CreateNumberValue(static_cast<double>(m_collection.Waves().size())));

    auto kits = json::JsonArray();
    uint32_t melodicCount{ 0 };

    for (auto const& instrument : m_collection.Instruments())
    {
        if (!instrument.IsDrumKit)
        {
            melodicCount++;
            continue;
        }

        auto kit = json::JsonObject();

        kit.SetNamedValue(MIDI_SYNTH_JSON_SOUND_SET_KIT_NAME_KEY,
            json::JsonValue::CreateStringValue(instrument.Name));

        kit.SetNamedValue(MIDI_SYNTH_JSON_SOUND_SET_KIT_PROGRAM_KEY,
            json::JsonValue::CreateNumberValue(static_cast<double>(instrument.Program)));

        kits.Append(kit);
    }

    responseObject.SetNamedValue(MIDI_SYNTH_JSON_SOUND_SET_MELODIC_KEY,
        json::JsonValue::CreateNumberValue(static_cast<double>(melodicCount)));

    responseObject.SetNamedValue(MIDI_SYNTH_JSON_SOUND_SET_KITS_KEY, kits);

    return S_OK;
}


_Use_decl_annotations_
HRESULT
MidiSynthDevice::AddInstrumentListToResponse(json::JsonObject& responseObject)
{
    RETURN_IF_FAILED(EnsureSoundSetLoaded());

    auto instruments = json::JsonArray();

    for (auto const& instrument : m_collection.Instruments())
    {
        // Kits are reported by the sound set command instead: they are selected by a program
        // change on a drum channel, so a bank address for them would be misleading.
        if (instrument.IsDrumKit)
        {
            continue;
        }

        auto entry = json::JsonObject();

        entry.SetNamedValue(MIDI_SYNTH_JSON_INSTRUMENT_NAME_KEY,
            json::JsonValue::CreateStringValue(instrument.Name));

        entry.SetNamedValue(MIDI_SYNTH_JSON_INSTRUMENT_BANK_MSB_KEY,
            json::JsonValue::CreateNumberValue(static_cast<double>(instrument.BankMsb & 0x7F)));

        entry.SetNamedValue(MIDI_SYNTH_JSON_INSTRUMENT_BANK_LSB_KEY,
            json::JsonValue::CreateNumberValue(static_cast<double>(instrument.BankLsb & 0x7F)));

        entry.SetNamedValue(MIDI_SYNTH_JSON_INSTRUMENT_PROGRAM_KEY,
            json::JsonValue::CreateNumberValue(static_cast<double>(instrument.Program & 0x7F)));

        instruments.Append(entry);
    }

    responseObject.SetNamedValue(MIDI_SYNTH_JSON_INSTRUMENTS_KEY, instruments);

    return S_OK;
}


HRESULT
MidiSynthDevice::Shutdown() noexcept
{
    RETURN_IF_FAILED(DisconnectClient());

    return S_OK;
}


void
MidiSynthDevice::WorkerThread() noexcept
{
    // Answering a property request parses JSON through WinRT, which needs an apartment on this
    // thread. Nothing else in the service provides one here. A failure is not fatal: MIDI keeps
    // flowing and only property exchange stops working.
    auto const comResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    auto comCleanup = wil::scope_exit([&]() { if (SUCCEEDED(comResult)) { CoUninitialize(); } });

    LOG_IF_FAILED(comResult);

    LOG_IF_FAILED(SetThreadDescription(GetCurrentThread(), L"MIDI Synth Transport Worker"));

    while (WaitForSingleObject(m_workerStop.get(), WorkerPollIntervalMilliseconds) == WAIT_TIMEOUT)
    {
        // Nothing may escape a thread body: an unhandled exception here takes the whole service
        // down, and MIDI with it, for every application on the machine.
        try
        {
            ServicePropertyRequests();
            ServiceOutbound();

            auto lock = m_audioLock.lock_exclusive();

            if (m_sink != nullptr && m_sink->DeviceLost())
            {
                TraceLoggingWrite(
                    MidiSynthTransportTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_WARNING,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Audio endpoint lost. Reopening.", MIDI_TRACE_EVENT_MESSAGE_FIELD)
                );

                ReleaseAudio();
                m_nextAudioRetryTimestamp = 0;
            }

            if (m_sink == nullptr)
            {
                auto const now = internal::GetCurrentMidiTimestamp();

                if (!m_audioWanted.load(std::memory_order_acquire))
                {
                    // Nothing to play. The endpoint still answers discovery, MIDI-CI, identity and
                    // property exchange from here rather than from the render thread.
                    PumpWithoutAudio();
                }
                else if (now >= m_nextAudioRetryTimestamp && AcquireAudio() != S_OK)
                {
                    // S_FALSE means the synthesizer is switched off rather than broken. Both back
                    // off, because retrying either one every pass would spin.
                    m_nextAudioRetryTimestamp = now +
                        internal::GetMidiTimestampFrequency() * AudioRetryIntervalMilliseconds / 1000;
                }
            }
            else if (m_audioWanted.load(std::memory_order_acquire))
            {
                auto const now = internal::GetCurrentMidiTimestamp();
                auto const last = m_lastChannelVoiceTimestamp.load(std::memory_order_acquire);
                auto const quietFor = (now > last) ? now - last : 0;

                // Let the device go once nothing has been played for a while and every voice has
                // finished. The wait is generous so that a reverb tail, a held pedal or a gap
                // between phrases cannot cut a performance off mid-flight.
                if (quietFor >= internal::GetMidiTimestampFrequency() * AudioIdleReleaseMilliseconds / 1000 &&
                    m_source != nullptr && m_source->LastActiveVoiceCount() == 0)
                {
                    TraceLoggingWrite(
                        MidiSynthTransportTelemetryProvider::Provider(),
                        MIDI_TRACE_EVENT_INFO,
                        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                        TraceLoggingPointer(this, "this"),
                        TraceLoggingWideString(L"Idle. Releasing the audio device.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                        TraceLoggingFloat64(
                            (internal::GetMidiTimestampFrequency() > 0)
                                ? static_cast<double>(quietFor) * 1000.0 /
                                    static_cast<double>(internal::GetMidiTimestampFrequency())
                                : 0.0,
                            "silent for ms")
                    );

                    ReleaseAudio();

                    // A note that arrived while this was being decided leaves the flag set, so the
                    // next pass reopens the device rather than dropping it on a silent engine.
                    if (m_lastChannelVoiceTimestamp.load(std::memory_order_acquire) == last)
                    {
                        m_audioWanted.store(false, std::memory_order_release);
                    }

                    m_nextAudioRetryTimestamp = 0;
                }
            }
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }
    }
}


void
MidiSynthDevice::ServiceOutbound() noexcept
{
    QueuedUmp message;

    while (m_outbound.TryPop(message))
    {
        LOG_IF_FAILED(DeliverToCallback(message));
    }
}


_Use_decl_annotations_
HRESULT
MidiSynthDevice::DeliverToCallback(QueuedUmp const& message) noexcept
{
    auto callback = m_callback;

    if (callback == nullptr || message.WordCount == 0 || message.WordCount > 4)
    {
        return S_FALSE;
    }

    RETURN_IF_FAILED(callback->Callback(
        MessageOptionFlags::MessageOptionFlags_None,
        const_cast<uint32_t*>(message.Words),
        message.WordCount * static_cast<UINT>(sizeof(uint32_t)),
        internal::GetCurrentMidiTimestamp(),
        m_callbackContext));

    return S_OK;
}


void
MidiSynthDevice::ServicePropertyRequests() noexcept
{
    try
    {
        ServicePropertyRequestsInner();
    }
    catch (...)
    {
        LOG_CAUGHT_EXCEPTION();

        // An abandoned reply is recoverable. A half sent one that never restarts is not, because
        // the initiator would wait out a timeout with chunks still outstanding.
        m_propertyExchange.AbandonReply();
    }
}


void
MidiSynthDevice::ServicePropertyRequestsInner()
{
    if (!m_propertyExchange.ReplyInProgress())
    {
        uint32_t withdrawn{ 0 };

        if (m_dispatcher.TakeInvalidatedInitiatorMuid(withdrawn))
        {
            (void)m_propertyExchange.RemoveSubscription(withdrawn, {});
        }

        UmpDispatcher::PendingPropertyRequest request{};

        if (m_dispatcher.TakePendingPropertyRequest(request))
        {
            if (request.IsSubscription)
            {
                HandleSubscriptionRequest(request);
                return;
            }

            const std::vector<char>* blob = nullptr;
            bool cacheable{ true };

            auto const lookup = ResourceForHeader(request.Header, request.HeaderByteCount, &blob, cacheable);

            if (lookup != ResourceLookup::Found || blob == nullptr)
            {
                std::string const asked(
                    reinterpret_cast<const char*>(request.Header), request.HeaderByteCount);

                TraceLoggingWrite(
                    MidiSynthTransportTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_WARNING,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(
                        lookup == ResourceLookup::HeaderNotJson
                            ? L"Property request header was not JSON"
                            : L"Property request for a resource we do not have",
                        MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingString(asked.c_str(), "header")
                );

                m_propertyExchange.SendNotFound(m_output, MIDI_SYNTH_GROUP_INDEX, m_dispatcher.Muid(), request);

                return;
            }

            m_propertyExchange.BeginReply(request, *blob, cacheable);
        }
        else if (m_propertyExchange.HasSubscriptions())
        {
            // Nothing was asked for, so this is the moment to tell subscribers what moved. Both
            // calls are cheap when nothing has: the first compares a snapshot and the second walks
            // a list of at most eight.
            (void)m_propertyExchange.ChannelListChanged(m_engine);

            if (!m_propertyExchange.BeginNextSubscriptionUpdate(m_engine, m_collection))
            {
                return;
            }
        }
        else
        {
            return;
        }
    }

    // One chunk per pass. A full program list is far more system exclusive packets than the
    // outbound queue holds at once.
    (void)m_propertyExchange.SendNextChunk(m_output, MIDI_SYNTH_GROUP_INDEX, m_dispatcher.Muid());
}


void
MidiSynthDevice::HandleSubscriptionRequest(
    MidiSynth::UmpDispatcher::PendingPropertyRequest const& request)
{
    std::string const text(reinterpret_cast<const char*>(request.Header), request.HeaderByteCount);

    json::JsonObject parsed{ nullptr };

    if (!json::JsonObject::TryParse(winrt::to_hstring(text), parsed))
    {
        m_propertyExchange.SendSubscriptionReply(
            m_output, MIDI_SYNTH_GROUP_INDEX, m_dispatcher.Muid(), request, 400, nullptr);

        return;
    }

    auto const readString = [&parsed](std::wstring_view key) -> std::wstring
    {
        winrt::hstring const name{ key };

        if (!parsed.HasKey(name))
        {
            return {};
        }

        auto const found = parsed.Lookup(name);

        if (found == nullptr || found.ValueType() != json::JsonValueType::String)
        {
            return {};
        }

        return std::wstring{ found.GetString() };
    };

    auto const command = readString(L"command");
    auto const resource = readString(L"resource");

    std::string subscribeId;

    for (auto const character : readString(L"subscribeId"))
    {
        subscribeId += (character > 0 && character < 0x80) ? static_cast<char>(character) : '?';
    }

    if (command == L"start")
    {
        // ChannelList is the only resource here that changes while the device is running, so it is
        // the only one the resource list declares as subscribable.
        if (resource != L"ChannelList")
        {
            m_propertyExchange.SendSubscriptionReply(
                m_output, MIDI_SYNTH_GROUP_INDEX, m_dispatcher.Muid(), request, 405, nullptr);

            return;
        }

        auto const* const assigned = m_propertyExchange.AddChannelListSubscription(request.InitiatorMuid);

        // Out of room. 507 is what the specification uses for a responder that cannot take on
        // any more, and it tells the initiator to keep polling instead.
        m_propertyExchange.SendSubscriptionReply(
            m_output, MIDI_SYNTH_GROUP_INDEX, m_dispatcher.Muid(), request,
            (assigned[0] == '\0') ? 507 : 200, assigned);

        return;
    }

    if (command == L"end")
    {
        (void)m_propertyExchange.RemoveSubscription(request.InitiatorMuid, subscribeId);

        m_propertyExchange.SendSubscriptionReply(
            m_output, MIDI_SYNTH_GROUP_INDEX, m_dispatcher.Muid(), request, 200, nullptr);

        return;
    }

    // An initiator does not send full, partial or notify to a responder. Answering rather than
    // ignoring keeps it from waiting out a timeout.
    m_propertyExchange.SendSubscriptionReply(
        m_output, MIDI_SYNTH_GROUP_INDEX, m_dispatcher.Muid(), request, 400, nullptr);
}


_Use_decl_annotations_
MidiSynthDevice::ResourceLookup
MidiSynthDevice::ResourceForHeader(
    const uint8_t* header,
    uint16_t headerBytes,
    const std::vector<char>** blob,
    bool& cacheable)
{
    *blob = nullptr;
    cacheable = true;

    std::string const text(reinterpret_cast<const char*>(header), headerBytes);

    json::JsonObject parsed{ nullptr };

    if (!json::JsonObject::TryParse(winrt::to_hstring(text), parsed))
    {
        return ResourceLookup::HeaderNotJson;
    }

    // The header comes off the wire, so a key present with the wrong type is expected rather than
    // exceptional. GetNamedString(key, default) throws on that, and the catch upstream abandons
    // the reply, which leaves the initiator waiting out a timeout instead of getting an answer.
    auto const readString = [&parsed](std::wstring_view key) -> std::wstring
    {
        winrt::hstring const name{ key };

        if (!parsed.HasKey(name))
        {
            return {};
        }

        auto const found = parsed.Lookup(name);

        if (found == nullptr || found.ValueType() != json::JsonValueType::String)
        {
            return {};
        }

        return std::wstring{ found.GetString() };
    };

    auto const resource = readString(L"resource");

    if (resource == L"ResourceList") { *blob = &m_propertyExchange.ResourceListJson(); return ResourceLookup::Found; }
    if (resource == L"DeviceInfo") { *blob = &m_propertyExchange.DeviceInfoJson(); return ResourceLookup::Found; }

    if (resource == L"ProgramList")
    {
        auto const wide = readString(L"resId");

        std::string resourceId;

        for (auto const character : wide)
        {
            resourceId += (character > 0 && character < 0x80) ? static_cast<char>(character) : '?';
        }

        if (!MidiSynth::PropertyExchangeSource::IsKnownProgramListResourceId(resourceId))
        {
            return ResourceLookup::UnknownResource;
        }

        *blob = &m_propertyExchange.ProgramListJson(resourceId);
        return ResourceLookup::Found;
    }

    // Rebuilt per request: unlike the others this reflects what is selected right now, which is
    // also why it is the one resource sent without a cache time.
    if (resource == L"ChannelList")
    {
        *blob = &m_propertyExchange.RebuildChannelListJson(m_engine, m_collection);
        cacheable = false;
        return ResourceLookup::Found;
    }

    return ResourceLookup::UnknownResource;
}

#pragma pop_macro("SendMessage")
