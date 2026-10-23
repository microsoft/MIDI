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

    // Exclusive mode asks for the device's shortest period, but no shorter than this. Every period
    // is a deadline for the render thread, and a missed one is heard as a dropout.
    constexpr uint32_t ExclusivePeriodFloorMilliseconds = 3;

    // Long enough for the fade a voice ends with, short enough that a disconnect never hangs.
    constexpr DWORD DrainTimeoutMilliseconds = 100;

    // How long notes are held for an audio device that is still being opened. Opening one takes
    // well under a second, even over Bluetooth. Past this the audio stack is in trouble, and notes
    // are dropped as they would be with no device at all.
    constexpr uint64_t AudioOpenPatienceMilliseconds = 3000;

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


_Use_decl_annotations_
void MidiSynthDevice::WholeMessageUmpOutput::SendUmp(const uint32_t* words, uint32_t wordCount) noexcept
{
    if (words == nullptr || wordCount == 0 || wordCount > 4)
    {
        return;
    }

    if (m_heldCount < m_held.size())
    {
        auto& held = m_held[m_heldCount++];

        held = QueuedUmp{};

        for (uint32_t i = 0; i < wordCount; i++)
        {
            held.Words[i] = words[i];
        }

        held.WordCount = static_cast<uint8_t>(wordCount);
    }
    else
    {
        m_overflowed = true;
    }

    // Status 1 is start and 2 is continue: more of this message is still to come.
    auto const status = internal::GetStatusFromDataMessage64FirstWord(words[0]);

    if (internal::GetUmpMessageTypeFromFirstWord(words[0]) == MIDI_UMP_MESSAGE_TYPE_DATA_MESSAGE_64 &&
        (status == 1 || status == 2))
    {
        return;
    }

    // A message too long to hold, or one the queue has no room for, is dropped whole.
    if (!m_overflowed)
    {
        (void)m_queue.TryPushAll(m_held.data(), m_heldCount);
    }

    m_heldCount = 0;
    m_overflowed = false;
}


HRESULT
MidiSynthDevice::EnsureSoundSetLoaded()
{
    // Held for the whole load: a second caller must neither parse alongside the first nor load
    // again into a collection the engine already plays from.
    auto soundSetLock = m_soundSetLock.lock();

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

    PropertyExchangeText text{};

    const std::pair<std::string*, UINT> localized[]
    {
        { &text.Manufacturer, IDS_DEVICE_INFO_MANUFACTURER },
        { &text.Family, IDS_DEVICE_INFO_FAMILY },
        { &text.Model, IDS_DEVICE_INFO_MODEL },
        { &text.MelodicChannel, IDS_CHANNEL_TITLE_MELODIC },
        { &text.PercussionChannel, IDS_CHANNEL_TITLE_PERCUSSION },
        { &text.MelodicProgramList, IDS_LINK_TITLE_MELODIC_PROGRAMS },
        { &text.DrumKitProgramList, IDS_LINK_TITLE_DRUM_KITS },
        { &text.ControllerList, IDS_LINK_TITLE_CONTROLLERS },
        { &text.Volume, IDS_CONTROLLER_VOLUME },
        { &text.Modulation, IDS_CONTROLLER_MODULATION },
        { &text.PitchBend, IDS_CONTROLLER_PITCH_BEND },
        { &text.SustainPedal, IDS_CONTROLLER_SUSTAIN_PEDAL },
        { &text.Pan, IDS_CONTROLLER_PAN },
        { &text.Expression, IDS_CONTROLLER_EXPRESSION },
        { &text.ReverbSend, IDS_CONTROLLER_REVERB_SEND },
        { &text.ChorusSend, IDS_CONTROLLER_CHORUS_SEND },
        { &text.NotePitchBend, IDS_CONTROLLER_NOTE_PITCH_BEND },
        { &text.PitchBendSensitivity, IDS_CONTROLLER_PITCH_BEND_SENSITIVITY },
        { &text.FineTuning, IDS_CONTROLLER_FINE_TUNING },
        { &text.CoarseTuning, IDS_CONTROLLER_COARSE_TUNING },
        { &text.NoteVolume, IDS_CONTROLLER_NOTE_VOLUME },
        { &text.NotePan, IDS_CONTROLLER_NOTE_PAN },
        { &text.NotePitch, IDS_CONTROLLER_NOTE_PITCH },
    };

    for (auto const& [target, id] : localized)
    {
        *target = internal::Utf8FromWString(internal::ResourceGetWString(id));
    }

    m_propertyExchange.Build(m_collection, SynthIdentity{}, text);

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
    // Loading it allocates, and nothing may leave this function.
    std::string discoveryName{};

    try
    {
        discoveryName = internal::Utf8FromWString(internal::ResourceGetWString(IDS_ENDPOINT_NAME));
    }
    CATCH_LOG();

    m_dispatcher.SetEndpointIdentity(discoveryName.c_str(), MIDI_SYNTH_ENDPOINT_UNIQUE_ID_UTF8);
}


// Caller must hold m_audioLock exclusively, and must have checked that there is no sink, because
// the render thread owns the inbound queue whenever there is one.
_Use_decl_annotations_
void
MidiSynthDevice::PumpWithoutAudio(bool discardNotes) noexcept
{
    if (DispatchWithoutAudio(m_inbound, m_dispatcher, discardNotes) != WithoutAudioResult::NeedsAudio)
    {
        return;
    }

    // Normally set already, because SendMessage flags a channel voice message before queuing it.
    // Letting the device go clears the flag, though, and a note that arrives just as that is
    // decided would otherwise wait in the queue, unheard, until another one came along.
    if (!m_audioWanted.exchange(true, std::memory_order_acq_rel))
    {
        m_audioRequestedTimestamp.store(internal::GetCurrentMidiTimestamp(), std::memory_order_release);
    }
}


// Only the worker calls this. Caller must hold m_audioLock exclusively.
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

    if (m_pendingAudioOpen != nullptr)
    {
        if (!m_pendingAudioOpen->Done.load(std::memory_order_acquire))
        {
            return E_PENDING;
        }

        auto const pending = std::move(m_pendingAudioOpen);

        // Switched off while the device was opening. Letting go of it is the point of the switch.
        if (!settings.Enabled)
        {
            return S_FALSE;
        }

        // The audio mode changed while the device was opening, so it was opened the wrong way. It
        // is let go first, or the new open could find it still busy.
        if (pending->Settings.AudioMode != settings.AudioMode)
        {
            pending->Sink.reset();

            RETURN_IF_FAILED(BeginAudioOpen(settings));
            return E_PENDING;
        }

        if (pending->Sink == nullptr)
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

        return InstallAudio(settings, pending->Sink);
    }

    if (!settings.Enabled)
    {
        return S_FALSE;
    }

    RETURN_IF_FAILED(BeginAudioOpen(settings));

    return E_PENDING;
}


namespace
{
    struct AudioOpenWork
    {
        std::shared_ptr<MidiSynthPendingAudioOpen> Pending{};
        HMODULE Module{ nullptr };
    };

    VOID CALLBACK OpenAudioOnThreadpool(_Inout_ PTP_CALLBACK_INSTANCE instance, _Inout_opt_ PVOID parameter) noexcept
    {
        std::unique_ptr<AudioOpenWork> work{ static_cast<AudioOpenWork*>(parameter) };

        if (work == nullptr)
        {
            return;
        }

        auto const module = work->Module;

        {
            auto const comResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

            if (work->Pending != nullptr)
            {
                work->Pending->Sink = MidiSynthDevice::OpenAudioSink(work->Pending->Settings);
                work->Pending->Done.store(true, std::memory_order_release);
            }

            // When the worker has given up on this open, the last reference is this one, and the
            // device is closed here, while COM is still initialized on this thread.
            work.reset();

            if (SUCCEEDED(comResult))
            {
                CoUninitialize();
            }
        }

        if (module != nullptr)
        {
            // keeps this code loaded until the callback has returned
            ::FreeLibraryWhenCallbackReturns(instance, module);
        }
    }
}


// Caller must hold m_audioLock exclusively.
_Use_decl_annotations_
HRESULT
MidiSynthDevice::BeginAudioOpen(MidiSynthSettings const& settings)
{
    auto pending = std::make_shared<MidiSynthPendingAudioOpen>();
    pending->Settings = settings;
    pending->StartedTimestamp = internal::GetCurrentMidiTimestamp();

    auto work = std::make_unique<AudioOpenWork>();
    work->Pending = pending;

    // The opener can outlast everything else in this transport, so it holds the DLL loaded
    if (!::GetModuleHandleExW(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
        reinterpret_cast<LPCWSTR>(&OpenAudioOnThreadpool),
        &work->Module))
    {
        work->Module = nullptr;
    }

    if (!::TrySubmitThreadpoolCallback(&OpenAudioOnThreadpool, work.get(), nullptr))
    {
        auto const error = HRESULT_FROM_WIN32(::GetLastError());

        if (work->Module != nullptr)
        {
            ::FreeLibrary(work->Module);
        }

        RETURN_HR(FAILED(error) ? error : E_OUTOFMEMORY);
    }

    // the callback owns it now
    work.release();

    m_pendingAudioOpen = std::move(pending);

    return S_OK;
}


// Runs on the opener's threadpool thread, so it touches nothing but its arguments.
_Use_decl_annotations_
std::unique_ptr<WasapiAudioSink>
MidiSynthDevice::OpenAudioSink(MidiSynthSettings const& settings) noexcept
try
{
    auto sink = std::make_unique<WasapiAudioSink>();
    bool opened{ false };

    if (settings.WantsExclusive())
    {
        auto const exclusiveResult = sink->OpenExclusive(ExclusivePeriodFloorMilliseconds);

        if (SUCCEEDED(exclusiveResult))
        {
            opened = true;
        }
        else
        {
            // Another application holds the device, exclusive use is turned off for it in Sound
            // settings, or it takes no format the synthesizer can write. Sound in shared mode beats
            // silence, and exclusive is tried again the next time the device is opened.
            TraceLoggingWrite(
                MidiSynthTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_WARNING,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
                TraceLoggingWideString(L"Could not take the audio device in exclusive mode. Playing in shared mode.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingHResult(exclusiveResult, MIDI_TRACE_EVENT_HRESULT_FIELD)
            );

            sink = std::make_unique<WasapiAudioSink>();
        }
    }

    // A failed exclusive open falls back to the lowest latency shared mode the device offers.
    if (!opened && !sink->Open(settings.WantsLowLatency() || settings.WantsExclusive()))
    {
        return nullptr;
    }

    return sink;
}
catch (...)
{
    LOG_CAUGHT_EXCEPTION();
    return nullptr;
}


// Caller must hold m_audioLock exclusively.
_Use_decl_annotations_
HRESULT
MidiSynthDevice::InstallAudio(MidiSynthSettings const& settings, std::unique_ptr<WasapiAudioSink>& sink)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, sink);

    // Modern mode renders at the device rate, so the audio engine converts nothing.
    auto config = SynthConfig::ForMode(settings.SynthMode, sink->SampleRate());

    config.BankSelect = settings.BankSelect;
    config.EnableEffects = settings.EffectsEnabled && config.EnableEffects;

    // The audio device now comes and goes underneath a connection. Initializing the engine keeps
    // every channel's program, controllers and tuning, and the bank addressing a System On chose,
    // but it reallocates the voices and effects, so the engine is only rebuilt when something it
    // was built from actually changed.
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

    // Kept before anything below can throw. Unwinding destroys the locals in reverse order, which
    // would free the render source while the audio thread is still rendering from it.
    m_source = std::move(source);
    m_sink = std::move(sink);

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
        TraceLoggingWideString(m_sink->DeviceName().c_str(), "device"),
        TraceLoggingUInt32(m_sink->SampleRate(), "sample rate"),
        TraceLoggingFloat64(m_sink->PeriodMilliseconds(), "period ms"),
        TraceLoggingBool(m_sink->UsingLowLatencyPath(), "low latency"),
        TraceLoggingBool(m_sink->UsingExclusiveMode(), "exclusive"),
        TraceLoggingFloat64(acquireMilliseconds, "ms since requested"),
        TraceLoggingBool(!engineMatchesDevice, "engine rebuilt")
    );

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
    // render thread reports that it has gone quiet before the device is released. The render
    // thread does the fading itself, because only it may touch the voices while it runs.
    if (m_source != nullptr)
    {
        m_source->RequestSilence();
    }

    if (m_source != nullptr && m_drainedEvent)
    {
        // The render thread signals on every quiet block until it stops, so the event is usually
        // left set by the previous release. Without this the wait would return at once and cut
        // the fade short, which is heard as a click.
        m_drainedEvent.ResetEvent();

        m_source->RequestDrain();
        (void)WaitForSingleObject(m_drainedEvent.get(), DrainTimeoutMilliseconds);
    }

    m_sink->Stop();

    // The render thread has stopped. Whatever it was asked to do and didn't get to, because the
    // stream stalled or stopped first, is done here, so the engine ends up as it was told to be.
    if (m_source != nullptr)
    {
        m_source->ApplyRequests();
    }

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
MidiSynthDevice::ConnectClient(IMidiCallback* callback, LONGLONG context, uint64_t& connectionId)
{
    connectionId = 0;

    RETURN_HR_IF_NULL(E_INVALIDARG, callback);

    TraceLoggingWrite(
        MidiSynthTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    auto connectionLock = m_connectionLock.lock();

    RETURN_IF_FAILED(EnsureSoundSetLoaded());

    // Assigning over a joinable std::thread terminates the process, which here would take MIDI
    // down for the whole machine. The service replaces a connection this way when it opens a new
    // one while the old one is still held by a client.
    if (m_worker.joinable())
    {
        RETURN_IF_FAILED(DisconnectCurrentClient());
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

    try
    {
        m_worker = std::thread(&MidiSynthDevice::WorkerThread, this);
    }
    catch (...)
    {
        // With no worker, nothing may keep the service's callback.
        m_callback.reset();
        m_callbackContext = 0;

        RETURN_CAUGHT_EXCEPTION();
    }

    connectionId = ++m_connectionId;

    return S_OK;
}


_Use_decl_annotations_
HRESULT
MidiSynthDevice::DisconnectClient(uint64_t connectionId) noexcept
{
    auto connectionLock = m_connectionLock.lock();

    // The service already opened a newer connection, which replaced this one. Ending it here would
    // leave the endpoint silent until the synthesizer is turned off and on again.
    if (connectionId != m_connectionId)
    {
        TraceLoggingWrite(
            MidiSynthTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Connection was already replaced. Nothing to end.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingUInt64(connectionId, "connection"),
            TraceLoggingUInt64(m_connectionId, "current connection")
        );

        return S_OK;
    }

    return DisconnectCurrentClient();
}


HRESULT
MidiSynthDevice::DisconnectCurrentClient() noexcept
{
    TraceLoggingWrite(
        MidiSynthTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingUInt64(m_connectionId, "connection")
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

        // A device still opening is left to its opener, which closes it once nobody wants it
        m_pendingAudioOpen.reset();
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
    // Otherwise a second change could record its settings between this one recording its own and
    // applying them, and the engine and endpoint would end up following the older of the two.
    auto settingsChange = m_settingsChangeLock.lock();

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

            // Re-resolving every channel is done only when the convention really changed. A volume
            // slider sends a stream of these. While audio runs, the render thread does it, because
            // only it may change the engine then.
            if (m_engineBankSelect != settings.BankSelect)
            {
                if (m_source != nullptr)
                {
                    m_source->RequestBankSelectMode(settings.BankSelect);
                }
                else
                {
                    m_engine.SetBankSelectMode(settings.BankSelect);
                }

                m_engineBankSelect = settings.BankSelect;
            }
        }
    }

    if (enabledChanged)
    {
        // Only the settings change lock may be held here. Removing the endpoint makes the service
        // tear the device pipe down, which comes back in through Bidi::Shutdown to DisconnectClient,
        // and that joins the worker and takes the audio lock. Holding the settings or audio lock
        // across this call would deadlock.
        auto endpointManager = TransportState::Current().GetEndpointManager();

        if (endpointManager != nullptr && endpointManager->IsInitialized())
        {
            // Creating the endpoint allocates and starts a thread, and nothing may leave here.
            try
            {
                LOG_IF_FAILED(endpointManager->SyncEndpointToSettings());
            }
            CATCH_LOG();
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

        // The worker opens the device again with the new settings. Opening it here would make a
        // configuration request, and everything waiting on this lock, wait on the audio stack.
        m_nextAudioRetryTimestamp = 0;

        if (!m_audioWanted.exchange(true, std::memory_order_acq_rel))
        {
            m_audioRequestedTimestamp.store(internal::GetCurrentMidiTimestamp(), std::memory_order_release);
        }
    }

    return S_OK;
}


MidiSynthSettings
MidiSynthDevice::Settings() const noexcept
{
    auto lock = m_settingsLock.lock_shared();
    return m_settings;
}


wil::cs_leave_scope_exit
MidiSynthDevice::LockSettingsChanges() noexcept
{
    return m_settingsChangeLock.lock();
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
    // has played links the channel to the right program list. While audio runs, the render thread
    // makes the change, because only it may change the engine then.
    if (m_source != nullptr)
    {
        m_source->RequestDrumChannel(channel, isDrumChannel);
    }
    else
    {
        m_engine.SetDrumChannel(channel, isDrumChannel);
    }

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
    // Whatever is connected ends, current or not: the transport itself is going away.
    auto connectionLock = m_connectionLock.lock();

    RETURN_IF_FAILED(DisconnectCurrentClient());

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
                auto const frequency = internal::GetMidiTimestampFrequency();
                bool const wanted = m_audioWanted.load(std::memory_order_acquire);
                bool opening{ false };

                if (wanted && now >= m_nextAudioRetryTimestamp)
                {
                    auto const acquireResult = AcquireAudio();

                    if (acquireResult == E_PENDING)
                    {
                        opening = true;
                    }
                    else if (acquireResult != S_OK)
                    {
                        // S_FALSE means the synthesizer is switched off rather than broken. Both
                        // back off, because retrying either one every pass would spin.
                        m_nextAudioRetryTimestamp = now + frequency * AudioRetryIntervalMilliseconds / 1000;
                    }
                }
                else if (!wanted && m_pendingAudioOpen != nullptr && m_pendingAudioOpen->Done.load(std::memory_order_acquire))
                {
                    // Opened after nobody wanted it any more. Holding it would keep the device
                    // from an application that wants it exclusively.
                    m_pendingAudioOpen.reset();
                }

                if (m_sink == nullptr)
                {
                    // Notes wait for a device that is being opened, so the first one isn't lost,
                    // but not for ever: an audio stack that has stalled would otherwise hold up
                    // MIDI-CI behind them too.
                    bool const worthWaitingFor =
                        opening &&
                        m_pendingAudioOpen != nullptr &&
                        now - m_pendingAudioOpen->StartedTimestamp < frequency * AudioOpenPatienceMilliseconds / 1000;

                    // Nothing renders, so the endpoint answers discovery, MIDI-CI, identity and
                    // property exchange from here. When audio is wanted and can't be had, notes are
                    // dropped, so they neither hold up everything queued behind them nor all sound
                    // at once when the device comes back.
                    PumpWithoutAudio(wanted && !worthWaitingFor);

                    // Nobody has played anything for a while, so stop asking for a device that can't
                    // be had. The next note asks again. One arriving just as this is decided is still
                    // in the queue, and asks again from there.
                    auto const last = m_lastChannelVoiceTimestamp.load(std::memory_order_acquire);

                    if (wanted && now > last && now - last >= frequency * AudioIdleReleaseMilliseconds / 1000)
                    {
                        m_audioWanted.store(false, std::memory_order_release);
                    }
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

    // Dispatcher replies go first: they answer messages that arrived before the property request.
    while (m_outbound.TryPop(message))
    {
        LOG_IF_FAILED(DeliverToCallback(message));
    }

    while (m_propertyOutbound.TryPop(message))
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
        // A device whose identifier was withdrawn, whether by another device or by a collision it
        // detected itself, has to make a new one or it stops answering MIDI-CI for good. The
        // dispatcher deliberately has no random source, so the replacement is made here.
        if (m_dispatcher.MuidNeedsReplacement())
        {
            const auto replacement = CreateRandomMuid();

            // Zero means the draw failed. Leaving the flag set retries on the next pass, which is
            // better than latching an identifier we did not generate properly.
            if (replacement != 0)
            {
                m_muid = replacement;
                m_dispatcher.SetMuid(m_muid);
            }
        }

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

        // A MIDI Message Report goes out whole, in a pass that sends nothing else, so no property
        // reply can land between its reply and its end. The queue is empty at this point, because
        // every pass drains it, and the header checks that the largest report fits.
        UmpDispatcher::PendingMessageReport report{};

        if (m_dispatcher.TakePendingMessageReport(report))
        {
            UmpDispatcher::WriteMidiMessageReport(report, MIDI_SYNTH_GROUP_INDEX, m_dispatcher.Muid(), m_propertyOutput);
            return;
        }

        UmpDispatcher::PendingPropertyRequest request{};

        if (m_dispatcher.TakePendingPropertyRequest(request))
        {
            if (request.IsSubscription)
            {
                HandleSubscriptionRequest(request);
                return;
            }

            ResourceRequest resourceRequest{};

            auto const lookup = ResourceForHeader(request.Header, request.HeaderByteCount, resourceRequest);

            if (lookup != ResourceLookup::Found ||
                (resourceRequest.Blob == nullptr && !resourceRequest.IsProgramList))
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

                m_propertyExchange.SendNotFound(m_propertyOutput, MIDI_SYNTH_GROUP_INDEX, m_dispatcher.Muid(), request);

                return;
            }

            if (resourceRequest.IsProgramList)
            {
                m_propertyExchange.BeginProgramListReply(
                    m_collection,
                    request,
                    resourceRequest.ResourceId,
                    resourceRequest.Offset,
                    resourceRequest.Limit);
            }
            else
            {
                m_propertyExchange.BeginReply(request, *resourceRequest.Blob, resourceRequest.Cacheable);
            }
        }
        else if (m_propertyExchange.HasSubscriptions())
        {
            // Nothing was asked for, so this is the moment to tell subscribers what moved. Both
            // calls are cheap when nothing has: the first compares a snapshot and the second walks
            // a list of at most eight.
            (void)m_propertyExchange.ChannelListChanged(m_engine);

            if (!m_propertyExchange.BeginNextSubscriptionNotification())
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
    (void)m_propertyExchange.SendNextChunk(m_propertyOutput, MIDI_SYNTH_GROUP_INDEX, m_dispatcher.Muid());
}


_Use_decl_annotations_
void
MidiSynthDevice::HandleSubscriptionRequest(
    MidiSynth::UmpDispatcher::PendingPropertyRequest const& request)
{
    std::string const text(reinterpret_cast<const char*>(request.Header), request.HeaderByteCount);

    json::JsonObject parsed{ nullptr };

    if (!json::JsonObject::TryParse(winrt::to_hstring(text), parsed))
    {
        m_propertyExchange.SendSubscriptionReply(
            m_propertyOutput, MIDI_SYNTH_GROUP_INDEX, m_dispatcher.Muid(), request, 400, nullptr);

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
                m_propertyOutput, MIDI_SYNTH_GROUP_INDEX, m_dispatcher.Muid(), request, 405, nullptr);

            return;
        }

        auto const* const assigned = m_propertyExchange.AddChannelListSubscription(request.InitiatorMuid);

        // Out of room. 507 is what the specification uses for a responder that cannot take on
        // any more, and it tells the initiator to keep polling instead.
        m_propertyExchange.SendSubscriptionReply(
            m_propertyOutput, MIDI_SYNTH_GROUP_INDEX, m_dispatcher.Muid(), request,
            (assigned[0] == '\0') ? 507 : 200, assigned);

        return;
    }

    if (command == L"end")
    {
        (void)m_propertyExchange.RemoveSubscription(request.InitiatorMuid, subscribeId);

        m_propertyExchange.SendSubscriptionReply(
            m_propertyOutput, MIDI_SYNTH_GROUP_INDEX, m_dispatcher.Muid(), request, 200, nullptr);

        return;
    }

    // An initiator does not send full, partial or notify to a responder. Answering rather than
    // ignoring keeps it from waiting out a timeout.
    m_propertyExchange.SendSubscriptionReply(
        m_propertyOutput, MIDI_SYNTH_GROUP_INDEX, m_dispatcher.Muid(), request, 400, nullptr);
}


_Use_decl_annotations_
MidiSynthDevice::ResourceLookup
MidiSynthDevice::ResourceForHeader(
    const uint8_t* header,
    uint16_t headerBytes,
    ResourceRequest& result)
{
    result = {};

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

    // Same discipline for numbers, plus a guard on what a double can be: an initiator is free to
    // send a negative or an absurd offset and neither may become a huge size_t.
    auto const readCount = [&parsed](std::wstring_view key, size_t fallback) -> size_t
    {
        winrt::hstring const name{ key };

        if (!parsed.HasKey(name))
        {
            return fallback;
        }

        auto const found = parsed.Lookup(name);

        if (found == nullptr || found.ValueType() != json::JsonValueType::Number)
        {
            return fallback;
        }

        auto const value = found.GetNumber();

        // Negative and not-a-number are out of spec, so the default stands. An absurd but positive
        // value is clamped rather than ignored: an offset past the end has to stay past the end,
        // which is how an initiator paging forward learns it is done.
        if (!(value >= 0.0))
        {
            return fallback;
        }

        constexpr double ceiling = 1000000.0;

        return static_cast<size_t>((value > ceiling) ? ceiling : value);
    };

    auto const resource = readString(L"resource");

    if (resource == L"ResourceList") { result.Blob = &m_propertyExchange.ResourceListJson(); return ResourceLookup::Found; }
    if (resource == L"DeviceInfo") { result.Blob = &m_propertyExchange.DeviceInfoJson(); return ResourceLookup::Found; }

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

        result.IsProgramList = true;
        result.ResourceId = std::move(resourceId);
        result.Offset = readCount(L"offset", 0);
        result.Limit = readCount(L"limit", SIZE_MAX);

        return ResourceLookup::Found;
    }

    if (resource == L"ChCtrlList")
    {
        std::string resourceId;

        for (auto const character : readString(L"resId"))
        {
            resourceId += (character > 0 && character < 0x80) ? static_cast<char>(character) : '?';
        }

        if (!MidiSynth::PropertyExchangeSource::IsKnownControllerListResourceId(resourceId))
        {
            return ResourceLookup::UnknownResource;
        }

        result.Blob = &m_propertyExchange.ControllerListJson(resourceId);
        return ResourceLookup::Found;
    }

    // Rebuilt per request: unlike the others this reflects what is selected right now, which is
    // also why it is the one resource sent without a cache time.
    if (resource == L"ChannelList")
    {
        result.Blob = &m_propertyExchange.RebuildChannelListJson(m_engine, m_collection);
        result.Cacheable = false;
        return ResourceLookup::Found;
    }

    return ResourceLookup::UnknownResource;
}

#pragma pop_macro("SendMessage")
