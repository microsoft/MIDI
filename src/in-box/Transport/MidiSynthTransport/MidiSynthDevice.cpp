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

    RETURN_HR_IF(E_FAIL, !m_engine.Initialize(&m_collection, config));

    m_engine.SetUserVolumeDb(settings.VolumeDecibels);

    m_dispatcher.Initialize(&m_engine, MIDI_SYNTH_GROUP_INDEX, m_muid);
    m_dispatcher.SetOutput(&m_output, SynthIdentity{});

    auto source = std::make_unique<UmpRenderSource>(
        m_engine, m_dispatcher, m_inbound, sink->SampleRate(), sink->BufferFrames(),
        m_drainedEvent.get());

    if (!sink->Start(source.get()))
    {
        return HRESULT_FROM_WIN32(ERROR_DEVICE_NOT_AVAILABLE);
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
        TraceLoggingBool(sink->UsingLowLatencyPath(), "low latency")
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

        auto const audioResult = AcquireAudio();

        // A machine with no usable audio device still gets a working endpoint which accepts
        // messages, and the worker keeps retrying. Note that nothing is answered while audio is
        // down, MIDI-CI included: the dispatcher is driven by the render thread, so with no stream
        // there is nothing pumping it.
        LOG_IF_FAILED(audioResult);
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

        if (m_sink != nullptr)
        {
            m_engine.SetUserVolumeDb(settings.VolumeDecibels);
            m_engine.SetBankSelectMode(settings.BankSelect);
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

    // The engine only exists while audio is running, and it is the render thread which reads this.
    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_DEVICE_NOT_AVAILABLE), m_sink == nullptr);

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

                // S_FALSE means the synthesizer is switched off rather than broken. Both back off,
                // because retrying either one every pass would spin.
                if (now >= m_nextAudioRetryTimestamp && AcquireAudio() != S_OK)
                {
                    m_nextAudioRetryTimestamp = now +
                        internal::GetMidiTimestampFrequency() * AudioRetryIntervalMilliseconds / 1000;
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
        UmpDispatcher::PendingPropertyRequest request{};

        if (!m_dispatcher.TakePendingPropertyRequest(request))
        {
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

    // One chunk per pass. A full program list is far more system exclusive packets than the
    // outbound queue holds at once.
    (void)m_propertyExchange.SendNextChunk(m_output, MIDI_SYNTH_GROUP_INDEX, m_dispatcher.Muid());
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

    auto const resource = parsed.GetNamedString(L"resource", L"");

    if (resource == L"ResourceList") { *blob = &m_propertyExchange.ResourceListJson(); return ResourceLookup::Found; }
    if (resource == L"DeviceInfo") { *blob = &m_propertyExchange.DeviceInfoJson(); return ResourceLookup::Found; }
    if (resource == L"ProgramList") { *blob = &m_propertyExchange.ProgramListJson(); return ResourceLookup::Found; }

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
