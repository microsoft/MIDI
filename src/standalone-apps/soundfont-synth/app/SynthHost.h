// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "SynthStore.h"

#include "AudioEngine.h"
#include "SoundFont.h"

namespace midisoundfontsynth
{
    enum class SynthState : int32_t
    {
        Off,
        Loading,
        Running,
        Failed,
    };

    enum class SynthFailure : int32_t
    {
        None,
        FileNotFound,
        FileUnreadable,
        FileTooLarge,
        NotASoundFont,
        NothingPlayable,
        CompressedSamples,
        OutOfMemory,
        ServiceUnavailable,
        EndpointFailed,
    };

    // What the window shows for one synth. A copy, so the UI never holds a lock.
    struct SynthStatus
    {
        std::wstring Id{};
        SynthState State{ SynthState::Off };
        SynthFailure Failure{ SynthFailure::None };

        bool InUse{ false };
        uint32_t ActiveVoices{ 0 };
        uint64_t MessagesReceived{ 0 };

        uint32_t MelodicPresetCount{ 0 };
        uint32_t DrumKitCount{ 0 };
        std::wstring BankName{};

        // Some of the bank could not be used. It still plays; the customer should know why a
        // program may be missing.
        bool PartlyLoaded{ false };

        std::wstring ClientEndpointDeviceId{};
    };

    // Every synth the app runs: the SoundFont, the engine, the virtual device apps connect to,
    // and the one audio stream they all play through.
    //
    // Threads:
    //   UI thread       calls the public methods, which queue work and return at once
    //   control thread  loads banks and creates and removes virtual devices, one at a time
    //   worker thread   answers MIDI-CI, sends replies and runs the audio policy every few ms
    //   SDK threads     deliver incoming MIDI straight into each synth's queue
    //   audio thread    renders, inside the audio engine
    class SynthHost
    {
    public:
        static SynthHost& Current() noexcept;

        // UI thread, once. The handler is raised on any thread when a status changes.
        void Start(_In_ std::function<void()> changed) noexcept;

        // Once, after the UI has gone. Blocks until every synth has been removed from Windows and
        // the audio device has been let go.
        void Shutdown() noexcept;

        void Enable(_In_ SynthDefinition const& definition) noexcept;
        void Disable(_In_ std::wstring const& id) noexcept;
        void Rename(_In_ std::wstring const& id, _In_ std::wstring const& name) noexcept;

        // Stops the synth if it runs and drops everything known about it.
        void Forget(_In_ std::wstring const& id) noexcept;

        // Takes effect immediately, on whatever is sounding.
        void SetVolume(_In_ std::wstring const& id, _In_ double decibels) noexcept;

        // Fades every voice on every synth.
        void SilenceAll() noexcept;

        std::vector<SynthStatus> Snapshot() const;

        void ApplyAudioSettings(_In_ SoundFontSynth::AudioOutputSettings const& settings) noexcept;
        SoundFontSynth::AudioEngineStatus AudioStatus() const;

        static constexpr uint32_t WorkerIntervalMilliseconds = 5;

    private:
        SynthHost() = default;

        struct Instance;

        void PostCommand(_In_ std::function<void()> command) noexcept;
        void ControlThread() noexcept;
        void WorkerThread() noexcept;

        void StartInstance(_In_ SynthDefinition definition) noexcept;
        void StopInstance(_In_ std::wstring const& id) noexcept;
        void DiscardPartialInstance(_In_ Instance& instance) noexcept;
        void RenameInstance(_In_ std::wstring const& id, _In_ std::wstring const& name) noexcept;

        bool EnsureSession() noexcept;
        void WaitForWorkerToLetGo() noexcept;

        std::shared_ptr<SoundFontSynth::SoundFont const> LoadSoundFont(
            _In_ std::wstring const& path,
            _Out_ SynthFailure& failure,
            _Out_ SoundFontSynth::Sf2LoadStatistics& statistics) noexcept;

        void UpdateStatus(_In_ std::wstring const& id, _In_ std::function<void(SynthStatus&)> const& update) noexcept;
        void RemoveStatus(_In_ std::wstring const& id) noexcept;
        void RaiseChanged() noexcept;

        std::function<void()> m_changed{};

        // control thread
        std::thread m_control{};
        std::mutex m_commandLock{};
        std::condition_variable m_commandReady{};
        std::deque<std::function<void()>> m_commands{};
        bool m_stopControl{ false };

        // owned by the control thread
        std::map<std::wstring, std::shared_ptr<Instance>> m_instances{};
        winrt::Windows::Devices::Midi2::MidiSession m_session{ nullptr };

        // read by the worker
        std::thread m_worker{};
        mutable std::mutex m_runningLock{};
        std::vector<std::shared_ptr<Instance>> m_running{};
        std::atomic<bool> m_stopWorker{ false };
        std::atomic<uint64_t> m_workerPasses{ 0 };
        wil::unique_event m_workerWake{ wil::EventOptions::None };

        SoundFontSynth::AudioEngine m_audio{};

        mutable std::mutex m_statusLock{};
        std::map<std::wstring, SynthStatus> m_status{};

        // Loaded banks by file, held weakly so a bank nobody plays is freed. Shared by every synth
        // that plays the same unchanged file, so a second synth on a 250 MB bank costs nothing.
        std::mutex m_fontCacheLock{};
        std::map<std::wstring, std::weak_ptr<SoundFontSynth::SoundFont const>> m_fontCache{};

        bool m_started{ false };
    };
}
