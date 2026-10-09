// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "SynthHost.h"
#include "StringResources.h"

#include "SynthCore.h"
#include "SynthPropertyRequests.h"
#include "SysEx7.h"

#include "MidiDefs.h"

namespace sf = ::SoundFontSynth;
namespace resources = ::midisoundfontsynth::resources;

namespace midisoundfontsynth
{
    namespace
    {
        // Microsoft's MMA id, the Windows family and the model number registered for this synth.
        // The same values reach the UMP device identity, the MIDI 1.0 identity reply, MIDI-CI
        // Discovery and the DeviceInfo resource, so they cannot disagree.
        constexpr uint8_t ManufacturerSysExId[3]
        {
            MIDI_MANUFACTURER_SYSEX_ID_MICROSOFT_BYTE1,
            MIDI_MANUFACTURER_SYSEX_ID_MICROSOFT_BYTE2,
            MIDI_MANUFACTURER_SYSEX_ID_MICROSOFT_BYTE3,
        };

        constexpr uint16_t DeviceFamily = MIDI_DEVICE_FAMILY_WINDOWS_11;
        constexpr uint16_t DeviceFamilyModelNumber = MIDI_DEVICE_FAMILY_MODEL_NUMBER_SOUNDFONT_SYNTH;
        constexpr uint8_t SoftwareRevision[4]{ 0, 1, 0, 0 };

        // The largest message the synth builds is a Property Exchange chunk of 640 bytes.
        constexpr size_t MaximumOutboundWords = 256;

        constexpr uint32_t WorkerLetGoTimeoutMilliseconds = 2000;

        sf::SynthIdentity MakeIdentity() noexcept
        {
            sf::SynthIdentity identity{};

            identity.ManufacturerSysExId[0] = ManufacturerSysExId[0];
            identity.ManufacturerSysExId[1] = ManufacturerSysExId[1];
            identity.ManufacturerSysExId[2] = ManufacturerSysExId[2];
            identity.FamilyCode = DeviceFamily;
            identity.FamilyMemberCode = DeviceFamilyModelNumber;

            for (size_t i = 0; i < 4; i++)
            {
                identity.SoftwareRevision[i] = SoftwareRevision[i];
            }

            return identity;
        }

        int64_t QpcNow() noexcept
        {
            LARGE_INTEGER now{};
            ::QueryPerformanceCounter(&now);
            return now.QuadPart;
        }

        // Whole System Exclusive messages go out as SysEx7 packets on the synth's group, all in
        // one call, so the packets of two messages can never interleave.
        class WireSink final : public sf::ISysExSink
        {
        public:
            explicit WireSink(_In_ midi2::MidiEndpointConnection const& connection) :
                m_connection(connection)
            {
            }

            void SendSysEx(_In_reads_(count) uint8_t const* payload, _In_ size_t count) noexcept override
            {
                try
                {
                    std::array<uint32_t, MaximumOutboundWords> words{};

                    auto const written = sf::BuildSysEx7Packets(
                        sf::SynthEndpointShape::FirstGroupIndex, payload, count, words.data(), words.size());

                    if (written == 0)
                    {
                        return;
                    }

                    (void)m_connection.SendMultipleMessagesWordArray(
                        midi2::MidiClock::TimestampConstantSendImmediately(),
                        0,
                        static_cast<uint32_t>(written),
                        words);
                }
                catch (...)
                {
                    // A connection going away while a reply is out is not worth more than this.
                }
            }

        private:
            midi2::MidiEndpointConnection m_connection{ nullptr };
        };

        SynthFailure FailureFromLoadStatus(_In_ sf::Sf2LoadStatus status) noexcept
        {
            switch (status)
            {
            case sf::Sf2LoadStatus::Ok:
                return SynthFailure::None;

            case sf::Sf2LoadStatus::CannotOpen:
                return SynthFailure::FileNotFound;

            case sf::Sf2LoadStatus::ReadFailed:
                return SynthFailure::FileUnreadable;

            case sf::Sf2LoadStatus::TooLarge:
                return SynthFailure::FileTooLarge;

            case sf::Sf2LoadStatus::NoPresets:
                return SynthFailure::NothingPlayable;

            case sf::Sf2LoadStatus::CompressedSamples:
                return SynthFailure::CompressedSamples;

            case sf::Sf2LoadStatus::OutOfMemory:
                return SynthFailure::OutOfMemory;

            default:
                return SynthFailure::NotASoundFont;
            }
        }

        // The path, its size and when it was written, so an edited file is loaded again rather
        // than served from memory.
        bool MakeFontCacheKey(_In_ std::wstring const& path, _Out_ std::wstring& key)
        {
            key.clear();

            WIN32_FILE_ATTRIBUTE_DATA attributes{};

            if (!::GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &attributes) ||
                (attributes.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
            {
                return false;
            }

            std::wstring lowered(path.size(), L'\0');

            if (::LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_LOWERCASE,
                path.c_str(), static_cast<int>(path.size()),
                lowered.data(), static_cast<int>(lowered.size()), nullptr, nullptr, 0) == 0)
            {
                lowered = path;
            }

            key = std::format(L"{}|{}|{}|{}",
                lowered,
                (static_cast<uint64_t>(attributes.nFileSizeHigh) << 32) | attributes.nFileSizeLow,
                attributes.ftLastWriteTime.dwHighDateTime,
                attributes.ftLastWriteTime.dwLowDateTime);

            return true;
        }

        midi2enum::MidiFunctionBlock MakeFunctionBlock(_In_ winrt::hstring const& name)
        {
            midi2enum::MidiFunctionBlock block{};

            block.Number(sf::SynthEndpointShape::FunctionBlockNumber);
            block.IsActive(true);
            block.Name(name);
            block.FirstGroup(midi2::MidiGroup(sf::SynthEndpointShape::FirstGroupIndex));
            block.GroupCount(sf::SynthEndpointShape::GroupCount);

            // Notes go in; MIDI-CI replies come out, so both directions are needed.
            block.Direction(midi2enum::MidiFunctionBlockDirection::Bidirectional);
            block.UIHint(midi2enum::MidiFunctionBlockUIHint::Receiver);
            block.RepresentsMidi10Connection(midi2enum::MidiFunctionBlockRepresentsMidi10Connection::Not10);

            // MIDI-CI 1.2, which is what the responder implements.
            block.MidiCIMessageVersionFormat(0x02);
            block.MaxSystemExclusive8Streams(0);

            return block;
        }
    }

    struct SynthHost::Instance
    {
        SynthDefinition Definition{};

        std::shared_ptr<sf::SoundFont const> Font{};
        std::shared_ptr<sf::SynthCore> Core{};

        midi2virt::MidiVirtualDevice Device{ nullptr };
        midi2::MidiEndpointConnection Connection{ nullptr };
        std::unique_ptr<WireSink> Wire{};

        winrt::event_token MessageToken{};
        winrt::event_token InUseToken{};
        winrt::event_token StreamConfigToken{};
    };

    SynthHost& SynthHost::Current() noexcept
    {
        static SynthHost instance{};
        return instance;
    }

    _Use_decl_annotations_
    void SynthHost::Start(std::function<void()> changed) noexcept
    {
        try
        {
            if (m_started)
            {
                return;
            }

            m_changed = std::move(changed);
            m_workerWake.create(wil::EventOptions::None);

            m_control = std::thread([this]() { ControlThread(); });
            m_worker = std::thread([this]() { WorkerThread(); });

            m_started = true;
        }
        MIDI_SF2SYNTH_CATCH_AND_LOG(L"Unable to start the synth host.")
    }

    void SynthHost::Shutdown() noexcept
    {
        try
        {
            if (!m_started)
            {
                return;
            }

            // Every synth is removed while the worker still runs, because removing one waits for
            // the worker to let go of it.
            PostCommand([this]()
                {
                    std::vector<std::wstring> ids;

                    for (auto const& [id, instance] : m_instances)
                    {
                        ids.push_back(id);
                    }

                    for (auto const& id : ids)
                    {
                        StopInstance(id);
                    }

                    try
                    {
                        if (m_session != nullptr)
                        {
                            m_session.Close();
                            m_session = nullptr;
                        }
                    }
                    MIDI_SF2SYNTH_CATCH_AND_LOG(L"Unable to close the MIDI session.")
                });

            {
                std::lock_guard<std::mutex> guard(m_commandLock);
                m_stopControl = true;
            }

            m_commandReady.notify_all();

            if (m_control.joinable())
            {
                m_control.join();
            }

            m_stopWorker.store(true, std::memory_order_release);
            m_workerWake.SetEvent();

            if (m_worker.joinable())
            {
                m_worker.join();
            }

            m_started = false;
        }
        MIDI_SF2SYNTH_CATCH_AND_LOG(L"Unable to shut the synth host down cleanly.")
    }

    _Use_decl_annotations_
    void SynthHost::PostCommand(std::function<void()> command) noexcept
    {
        try
        {
            {
                std::lock_guard<std::mutex> guard(m_commandLock);

                if (m_stopControl)
                {
                    return;
                }

                m_commands.push_back(std::move(command));
            }

            m_commandReady.notify_one();
        }
        MIDI_SF2SYNTH_CATCH_AND_LOG(L"Unable to queue synth work.")
    }

    void SynthHost::ControlThread() noexcept
    {
        auto const comResult = ::CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        auto comCleanup = wil::scope_exit([&]() { if (SUCCEEDED(comResult)) { ::CoUninitialize(); } });

        (void)::SetThreadDescription(::GetCurrentThread(), L"SoundFont Synth Control");

        for (;;)
        {
            std::function<void()> command{};

            {
                std::unique_lock<std::mutex> lock(m_commandLock);

                m_commandReady.wait(lock, [this]() { return m_stopControl || !m_commands.empty(); });

                // Whatever was queued before the stop still runs, which is how shutdown gets to
                // remove every synth.
                if (m_commands.empty())
                {
                    return;
                }

                command = std::move(m_commands.front());
                m_commands.pop_front();
            }

            try
            {
                command();
            }
            MIDI_SF2SYNTH_CATCH_AND_LOG(L"Synth work failed.")
        }
    }

    void SynthHost::WorkerThread() noexcept
    {
        // Property Exchange parses JSON through WinRT, which needs an apartment on this thread.
        auto const comResult = ::CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        auto comCleanup = wil::scope_exit([&]() { if (SUCCEEDED(comResult)) { ::CoUninitialize(); } });

        (void)::SetThreadDescription(::GetCurrentThread(), L"SoundFont Synth Worker");

        std::vector<std::shared_ptr<Instance>> running{};
        std::vector<std::shared_ptr<sf::SynthCore>> cores{};

        while (!m_stopWorker.load(std::memory_order_acquire))
        {
            (void)::WaitForSingleObject(m_workerWake.get(), WorkerIntervalMilliseconds);

            // Nothing may escape a thread body. One bad pass costs a few milliseconds of MIDI-CI,
            // where an escaping exception would cost every synth at once.
            try
            {
                {
                    std::lock_guard<std::mutex> guard(m_runningLock);
                    running = m_running;
                }

                cores.clear();

                for (auto const& instance : running)
                {
                    cores.push_back(instance->Core);
                }

                m_audio.Service(cores);

                for (auto const& instance : running)
                {
                    sf::ServicePropertyRequests(*instance->Core, *instance->Wire);
                    instance->Core->ServiceOutbound(*instance->Wire);
                }
            }
            MIDI_SF2SYNTH_CATCH_AND_LOG(L"The worker pass failed.")

            running.clear();
            m_workerPasses.fetch_add(1, std::memory_order_acq_rel);
        }

        m_audio.Shutdown();
    }

    void SynthHost::WaitForWorkerToLetGo() noexcept
    {
        // Two whole passes, so the one that may have been running when the synth was taken off
        // the list has finished and the next one has not seen it.
        auto const start = m_workerPasses.load(std::memory_order_acquire);
        auto const deadline = ::GetTickCount64() + WorkerLetGoTimeoutMilliseconds;

        m_workerWake.SetEvent();

        while (m_workerPasses.load(std::memory_order_acquire) < start + 2 && ::GetTickCount64() < deadline)
        {
            ::Sleep(1);
        }
    }

    bool SynthHost::EnsureSession() noexcept
    {
        try
        {
            if (m_session != nullptr)
            {
                return true;
            }

            if (!midi2::MidiApi::EnsureServiceAvailable())
            {
                MIDI_SF2SYNTH_LOG_WARNING(L"The MIDI service is not available.");
                return false;
            }

            m_session = midi2::MidiSession::Create(resources::GetString(L"AppDisplayName"));

            return m_session != nullptr;
        }
        MIDI_SF2SYNTH_CATCH_AND_LOG(L"Unable to open a MIDI session.")

        return false;
    }

    _Use_decl_annotations_
    std::shared_ptr<sf::SoundFont const> SynthHost::LoadSoundFont(
        std::wstring const& path,
        SynthFailure& failure,
        sf::Sf2LoadStatistics& statistics) noexcept
    {
        failure = SynthFailure::None;
        statistics = {};

        try
        {
            std::wstring key;

            if (!MakeFontCacheKey(path, key))
            {
                failure = SynthFailure::FileNotFound;
                return nullptr;
            }

            {
                std::lock_guard<std::mutex> guard(m_fontCacheLock);

                auto const found = m_fontCache.find(key);

                if (found != m_fontCache.end())
                {
                    if (auto font = found->second.lock())
                    {
                        return font;
                    }

                    m_fontCache.erase(found);
                }
            }

            auto font = std::make_shared<sf::SoundFont>();

            auto const status = sf::SoundFont::LoadFromFile(path, sf::Sf2LoadLimits{}, *font, &statistics);

            if (status != sf::Sf2LoadStatus::Ok)
            {
                failure = FailureFromLoadStatus(status);
                return nullptr;
            }

            std::shared_ptr<sf::SoundFont const> loaded = std::move(font);

            {
                std::lock_guard<std::mutex> guard(m_fontCacheLock);
                m_fontCache[key] = loaded;
            }

            return loaded;
        }
        catch (std::bad_alloc const&)
        {
            failure = SynthFailure::OutOfMemory;
        }
        catch (...)
        {
            failure = SynthFailure::FileUnreadable;
        }

        return nullptr;
    }

    _Use_decl_annotations_
    void SynthHost::Enable(SynthDefinition const& definition) noexcept
    {
        try
        {
            UpdateStatus(definition.Id, [](SynthStatus& status)
                {
                    status.State = SynthState::Loading;
                    status.Failure = SynthFailure::None;
                });

            RaiseChanged();

            PostCommand([this, definition]() { StartInstance(definition); });
        }
        MIDI_SF2SYNTH_CATCH_AND_LOG(L"Unable to start a synth.")
    }

    _Use_decl_annotations_
    void SynthHost::Disable(std::wstring const& id) noexcept
    {
        try
        {
            PostCommand([this, id]() { StopInstance(id); });
        }
        MIDI_SF2SYNTH_CATCH_AND_LOG(L"Unable to stop a synth.")
    }

    _Use_decl_annotations_
    void SynthHost::Rename(std::wstring const& id, std::wstring const& name) noexcept
    {
        try
        {
            PostCommand([this, id, name]() { RenameInstance(id, name); });
        }
        MIDI_SF2SYNTH_CATCH_AND_LOG(L"Unable to rename a synth.")
    }

    _Use_decl_annotations_
    void SynthHost::Forget(std::wstring const& id) noexcept
    {
        try
        {
            PostCommand([this, id]()
                {
                    StopInstance(id);
                    RemoveStatus(id);
                    RaiseChanged();
                });
        }
        MIDI_SF2SYNTH_CATCH_AND_LOG(L"Unable to remove a synth.")
    }

    _Use_decl_annotations_
    void SynthHost::SetVolume(std::wstring const& id, double decibels) noexcept
    {
        try
        {
            std::lock_guard<std::mutex> guard(m_runningLock);

            for (auto const& instance : m_running)
            {
                if (instance->Definition.Id == id)
                {
                    instance->Core->SetUserVolumeDb(decibels);
                }
            }
        }
        MIDI_SF2SYNTH_CATCH_AND_LOG(L"Unable to change a synth's volume.")
    }

    void SynthHost::SilenceAll() noexcept
    {
        try
        {
            std::lock_guard<std::mutex> guard(m_runningLock);

            for (auto const& instance : m_running)
            {
                instance->Core->RequestSilence();
            }
        }
        MIDI_SF2SYNTH_CATCH_AND_LOG(L"Unable to silence the synths.")
    }

    _Use_decl_annotations_
    void SynthHost::ApplyAudioSettings(sf::AudioOutputSettings const& settings) noexcept
    {
        try
        {
            m_audio.SetSettings(settings);
            m_workerWake.SetEvent();
        }
        MIDI_SF2SYNTH_CATCH_AND_LOG(L"Unable to change the audio settings.")
    }

    sf::AudioEngineStatus SynthHost::AudioStatus() const
    {
        return m_audio.Status();
    }

    std::vector<SynthStatus> SynthHost::Snapshot() const
    {
        std::vector<SynthStatus> snapshot;

        {
            std::lock_guard<std::mutex> guard(m_statusLock);

            for (auto const& [id, status] : m_status)
            {
                snapshot.push_back(status);
            }
        }

        // The live numbers come straight from the engines rather than through a status update.
        std::lock_guard<std::mutex> guard(m_runningLock);

        for (auto const& instance : m_running)
        {
            for (auto& status : snapshot)
            {
                if (status.Id == instance->Definition.Id)
                {
                    status.ActiveVoices = instance->Core->LastActiveVoiceCount();
                    status.MessagesReceived = instance->Core->ReceivedMessageCount();
                }
            }
        }

        return snapshot;
    }

    _Use_decl_annotations_
    void SynthHost::UpdateStatus(std::wstring const& id, std::function<void(SynthStatus&)> const& update) noexcept
    {
        try
        {
            std::lock_guard<std::mutex> guard(m_statusLock);

            auto& status = m_status[id];
            status.Id = id;

            update(status);
        }
        MIDI_SF2SYNTH_CATCH_AND_LOG(L"Unable to update a synth's status.")
    }

    _Use_decl_annotations_
    void SynthHost::RemoveStatus(std::wstring const& id) noexcept
    {
        try
        {
            std::lock_guard<std::mutex> guard(m_statusLock);
            m_status.erase(id);
        }
        MIDI_SF2SYNTH_CATCH_AND_LOG(L"Unable to remove a synth's status.")
    }

    void SynthHost::RaiseChanged() noexcept
    {
        try
        {
            if (m_changed)
            {
                m_changed();
            }
        }
        MIDI_SF2SYNTH_CATCH_AND_LOG(L"The status change handler failed.")
    }

    _Use_decl_annotations_
    void SynthHost::DiscardPartialInstance(Instance& instance) noexcept
    {
        try
        {
            if (instance.Connection != nullptr && m_session != nullptr)
            {
                m_session.DisconnectEndpointConnection(instance.Connection.ConnectionId());
            }
        }
        MIDI_SF2SYNTH_CATCH_AND_LOG(L"Unable to discard a half created endpoint.")

        instance.Wire.reset();
        instance.Connection = nullptr;
        instance.Device = nullptr;
    }

    _Use_decl_annotations_
    void SynthHost::StartInstance(SynthDefinition definition) noexcept
    {
        auto const id = definition.Id;

        std::shared_ptr<Instance> instance{};

        auto fail = [this, &id, &instance](SynthFailure failure)
            {
                if (instance != nullptr)
                {
                    DiscardPartialInstance(*instance);
                }

                UpdateStatus(id, [failure](SynthStatus& status)
                    {
                        status.State = SynthState::Failed;
                        status.Failure = failure;
                        status.InUse = false;
                        status.ActiveVoices = 0;
                    });

                RaiseChanged();
            };

        try
        {
            if (m_instances.find(id) != m_instances.end())
            {
                UpdateStatus(id, [](SynthStatus& status) { status.State = SynthState::Running; });
                RaiseChanged();
                return;
            }

            SynthFailure failure{ SynthFailure::None };
            sf::Sf2LoadStatistics statistics{};

            auto font = LoadSoundFont(definition.SoundFontPath, failure, statistics);

            if (font == nullptr)
            {
                fail(failure);
                return;
            }

            if (!EnsureSession())
            {
                fail(SynthFailure::ServiceUnavailable);
                return;
            }

            instance = std::make_shared<Instance>();
            instance->Definition = definition;
            instance->Font = font;

            auto const productName = sf::ToUtf8(std::wstring{ resources::GetString(L"AppDisplayName") });

            instance->Core = std::make_shared<sf::SynthCore>(font, MakeIdentity(), definition.ProductInstanceId, productName);
            instance->Core->SetUserVolumeDb(definition.VolumeDb);

            winrt::hstring const name{ definition.Name };

            midi2enum::MidiDeclaredEndpointInfo endpointInfo{};
            endpointInfo.Name(name);
            endpointInfo.ProductInstanceId(winrt::to_hstring(definition.ProductInstanceId));
            endpointInfo.SpecificationVersionMajor(1);
            endpointInfo.SpecificationVersionMinor(1);
            endpointInfo.SupportsMidi10Protocol(true);
            endpointInfo.SupportsMidi20Protocol(true);
            endpointInfo.SupportsReceivingJitterReductionTimestamps(false);
            endpointInfo.SupportsSendingJitterReductionTimestamps(false);

            // Not static, so a rename can reach the function block name too.
            endpointInfo.HasStaticFunctionBlocks(false);
            endpointInfo.DeclaredFunctionBlockCount(1);

            midi2enum::MidiDeclaredDeviceIdentity deviceIdentity(
                ManufacturerSysExId[0], ManufacturerSysExId[1], ManufacturerSysExId[2],
                static_cast<uint8_t>(DeviceFamily & 0x7F), static_cast<uint8_t>((DeviceFamily >> 7) & 0x7F),
                static_cast<uint8_t>(DeviceFamilyModelNumber & 0x7F), static_cast<uint8_t>((DeviceFamilyModelNumber >> 7) & 0x7F),
                SoftwareRevision[0], SoftwareRevision[1], SoftwareRevision[2], SoftwareRevision[3]);

            midi2enum::MidiEndpointUserSuppliedInfo userSuppliedInfo{};

            auto const fileName = std::wstring{ ::PathFindFileNameW(definition.SoundFontPath.c_str()) };

            midi2virt::MidiVirtualDeviceCreationConfig config(
                name,
                resources::FormatString(L"VirtualDeviceDescriptionFormat", fileName),
                resources::GetString(L"VirtualDeviceManufacturer"),
                endpointInfo,
                deviceIdentity,
                userSuppliedInfo);

            config.FunctionBlocks().Append(MakeFunctionBlock(name));

            instance->Device = midi2virt::MidiVirtualDeviceManager::CreateVirtualDevice(config);

            if (instance->Device == nullptr)
            {
                fail(SynthFailure::EndpointFailed);
                return;
            }

            // Set before the connection opens, or the first discovery messages slip past.
            instance->Device.SuppressHandledMessages(true);

            instance->Connection = m_session.CreateEndpointConnection(instance->Device.DeviceEndpointDeviceId());

            if (instance->Connection == nullptr ||
                instance->Connection.AddMessageProcessingPlugin(instance->Device) != midi2::MidiMessageProcessingPluginAddResult::Succeeded)
            {
                fail(SynthFailure::EndpointFailed);
                return;
            }

            instance->Wire = std::make_unique<WireSink>(instance->Connection);

            std::weak_ptr<sf::SynthCore> weakCore = instance->Core;

            instance->MessageToken = instance->Connection.MessageReceived(
                [weakCore](midi2::IMidiMessageReceivedEventSource const&, midi2::MidiMessageReceivedEventArgs const& args)
                {
                    try
                    {
                        auto const core = weakCore.lock();

                        if (core == nullptr)
                        {
                            return;
                        }

                        uint32_t words[4]{};
                        auto const count = args.FillWords(words[0], words[1], words[2], words[3]);

                        // A timestamp from the future or none at all means arrival now.
                        auto const now = QpcNow();
                        auto timestamp = static_cast<int64_t>(args.Timestamp());

                        if (timestamp <= 0 || timestamp > now)
                        {
                            timestamp = now;
                        }

                        core->QueueInbound(words, count, timestamp);
                    }
                    catch (...)
                    {
                    }
                });

            instance->InUseToken = instance->Device.ClientEndpointInUseChanged(
                [this, id](midi2virt::MidiVirtualDevice const&, midi2virt::MidiVirtualDeviceClientEndpointInUseChangedEventArgs const& args)
                {
                    try
                    {
                        auto const inUse = args.IsClientEndpointInUse();

                        UpdateStatus(id, [inUse](SynthStatus& status) { status.InUse = inUse; });
                        RaiseChanged();
                    }
                    catch (...)
                    {
                    }
                });

            auto weakConnection = winrt::make_weak(instance->Connection);

            // Both protocols are fine: the synth plays MIDI 1.0 and MIDI 2.0 channel voice
            // messages whichever was agreed. So whatever was asked for is confirmed.
            instance->StreamConfigToken = instance->Device.StreamConfigRequestReceived(
                [weakConnection](midi2virt::MidiVirtualDevice const&, midi2virt::MidiStreamConfigRequestReceivedEventArgs const& args)
                {
                    try
                    {
                        auto const connection = weakConnection.get();

                        if (connection == nullptr)
                        {
                            return;
                        }

                        auto const protocol = (args.PreferredMidiProtocol() == midi2enum::MidiProtocol::Midi1)
                            ? midi2enum::MidiProtocol::Midi1
                            : midi2enum::MidiProtocol::Midi2;

                        auto const notification = midi2msg::MidiStreamMessageBuilder::BuildStreamConfigurationNotificationMessage(
                            midi2::MidiClock::TimestampConstantSendImmediately(),
                            static_cast<uint8_t>(protocol),
                            false,
                            false);

                        (void)connection.SendSingleMessagePacket(notification);
                    }
                    catch (...)
                    {
                    }
                });

            if (!instance->Connection.Open())
            {
                fail(SynthFailure::EndpointFailed);
                return;
            }

            std::wstring clientEndpointDeviceId{};

            try
            {
                clientEndpointDeviceId = midi2virt::MidiVirtualDeviceManager::GetAssociatedClientEndpointDeviceId(
                    instance->Device.AssociationId());
            }
            catch (...)
            {
            }

            auto const inUse = instance->Device.IsClientEndpointInUse();

            m_instances[id] = instance;

            {
                std::lock_guard<std::mutex> guard(m_runningLock);
                m_running.push_back(instance);
            }

            m_workerWake.SetEvent();

            uint32_t melodic{ 0 };
            uint32_t drums{ 0 };

            for (auto const& preset : font->Presets())
            {
                (preset.Bank == sf::SoundFont::PercussionBank) ? drums++ : melodic++;
            }

            auto const partly = statistics.PresetsDropped > 0 || statistics.ZonesDropped > 0 || statistics.SamplesUnusable > 0;
            auto const bankName = font->Info().Name;

            UpdateStatus(id, [&](SynthStatus& status)
                {
                    status.State = SynthState::Running;
                    status.Failure = SynthFailure::None;
                    status.InUse = inUse;
                    status.MelodicPresetCount = melodic;
                    status.DrumKitCount = drums;
                    status.BankName = bankName;
                    status.PartlyLoaded = partly;
                    status.ClientEndpointDeviceId = clientEndpointDeviceId;
                });

            RaiseChanged();

            MIDI_SF2SYNTH_LOG_INFO(L"Synth started.");
        }
        catch (std::bad_alloc const&)
        {
            fail(SynthFailure::OutOfMemory);
        }
        catch (winrt::hresult_error const& ex)
        {
            MIDI_SF2SYNTH_LOG_HRESULT_EXCEPTION(ex, L"Unable to create the synth's endpoint.");
            fail(SynthFailure::EndpointFailed);
        }
        catch (...)
        {
            MIDI_SF2SYNTH_LOG_GENERAL_EXCEPTION(L"Unable to create the synth's endpoint.");
            fail(SynthFailure::EndpointFailed);
        }
    }

    _Use_decl_annotations_
    void SynthHost::StopInstance(std::wstring const& id) noexcept
    {
        try
        {
            auto const found = m_instances.find(id);

            if (found == m_instances.end())
            {
                UpdateStatus(id, [](SynthStatus& status)
                    {
                        if (status.State != SynthState::Failed)
                        {
                            status.State = SynthState::Off;
                        }

                        status.InUse = false;
                        status.ActiveVoices = 0;
                    });

                RaiseChanged();
                return;
            }

            auto instance = found->second;

            {
                std::lock_guard<std::mutex> guard(m_runningLock);
                std::erase(m_running, instance);
            }

            WaitForWorkerToLetGo();

            try
            {
                instance->Connection.MessageReceived(instance->MessageToken);
                instance->Device.ClientEndpointInUseChanged(instance->InUseToken);
                instance->Device.StreamConfigRequestReceived(instance->StreamConfigToken);
            }
            MIDI_SF2SYNTH_CATCH_AND_LOG(L"Unable to remove the synth's event handlers.")

            try
            {
                instance->Connection.RemoveMessageProcessingPlugin(instance->Device.PluginId());

                if (m_session != nullptr)
                {
                    m_session.DisconnectEndpointConnection(instance->Connection.ConnectionId());
                }
            }
            MIDI_SF2SYNTH_CATCH_AND_LOG(L"Unable to disconnect the synth's endpoint.")

            instance->Wire.reset();
            instance->Connection = nullptr;
            instance->Device = nullptr;

            m_instances.erase(found);

            UpdateStatus(id, [](SynthStatus& status)
                {
                    status.State = SynthState::Off;
                    status.Failure = SynthFailure::None;
                    status.InUse = false;
                    status.ActiveVoices = 0;
                    status.ClientEndpointDeviceId.clear();
                });

            RaiseChanged();
        }
        MIDI_SF2SYNTH_CATCH_AND_LOG(L"Unable to stop the synth.")
    }

    _Use_decl_annotations_
    void SynthHost::RenameInstance(std::wstring const& id, std::wstring const& name) noexcept
    {
        try
        {
            auto const found = m_instances.find(id);

            if (found == m_instances.end())
            {
                return;
            }

            auto& instance = *found->second;
            winrt::hstring const newName{ name };

            if (!instance.Device.UpdateEndpointName(newName))
            {
                MIDI_SF2SYNTH_LOG_WARNING(L"The endpoint name could not be updated.");
            }

            if (!instance.Device.UpdateFunctionBlock(MakeFunctionBlock(newName)))
            {
                MIDI_SF2SYNTH_LOG_WARNING(L"The function block name could not be updated.");
            }

            instance.Definition.Name = name;
        }
        MIDI_SF2SYNTH_CATCH_AND_LOG(L"Unable to rename the synth.")
    }
}
