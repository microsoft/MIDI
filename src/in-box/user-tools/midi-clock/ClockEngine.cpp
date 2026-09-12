// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "ClockEngine.h"
#include "StringResources.h"

namespace res = ::midiclock::resources;

namespace midiclock
{
    ClockEngine::~ClockEngine() noexcept
    {
        StopAll();

        try
        {
            std::lock_guard<std::recursive_mutex> const guard{ m_lock };

            if (m_session != nullptr)
            {
                m_session.Close();
                m_session = nullptr;
            }
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    std::map<std::wstring, ClockStartResult> ClockEngine::Start(
        std::vector<ClockStartRequest> const& requests) noexcept
    {
        std::map<std::wstring, ClockStartResult> results{};

        try
        {
            if (requests.empty())
            {
                return results;
            }

            std::lock_guard<std::recursive_mutex> const guard{ m_lock };

            if (!midi2::MidiApi::EnsureServiceAvailable())
            {
                for (auto const& request : requests)
                {
                    results[request.Id] = ClockStartResult::ServiceUnavailable;
                }

                return results;
            }

            if (m_session == nullptr)
            {
                m_session = midi2::MidiSession::Create(res::GetString(L"AppDisplayName"));
            }

            if (m_session == nullptr)
            {
                for (auto const& request : requests)
                {
                    results[request.Id] = ClockStartResult::SessionFailed;
                }

                return results;
            }

            // Everything that can fail or block happens here, before a single generator is
            // started, so the shared origin below is not spent waiting on the service.
            std::vector<std::wstring> ready{};

            for (auto const& request : requests)
            {
                if (request.Id.empty())
                {
                    continue;
                }

                StopUnderLock(request.Id);

                if (request.EndpointDeviceId.empty() || request.GroupIndexes.empty())
                {
                    results[request.Id] = ClockStartResult::NoEndpointChosen;
                    continue;
                }

                RunningClock clock{};

                clock.Connection = m_session.CreateEndpointConnection(
                    winrt::hstring{ request.EndpointDeviceId });

                if (clock.Connection == nullptr || !clock.Connection.Open())
                {
                    if (clock.Connection != nullptr)
                    {
                        m_session.DisconnectEndpointConnection(clock.Connection.ConnectionId());
                    }

                    results[request.Id] = ClockStartResult::ConnectionFailed;
                    continue;
                }

                midiapp::BeatClockGeneratorOptions options{};

                options.BeatsPerMinute = request.BeatsPerMinute;
                options.PulsesPerQuarterNote = request.PulsesPerQuarterNote;
                options.GroupIndexes = request.GroupIndexes;
                options.SendStartMessage = request.SendStartStop;
                options.SendStopMessage = request.SendStartStop;

                clock.Generator = std::make_unique<midiapp::BeatClockGenerator>(clock.Connection, options);

                m_running[request.Id] = std::move(clock);

                ready.push_back(request.Id);
            }

            if (ready.empty())
            {
                return results;
            }

            // One instant for the whole batch. Queuing a generator is only a thread creation,
            // so the lead only has to cover that, not the service calls above.
            auto const origin = midi2::MidiClock::Now() + midiapp::BeatClockGenerator::SuggestedStartLeadTicks();

            for (auto const& id : ready)
            {
                auto const entry = m_running.find(id);

                if (entry != m_running.end() && entry->second.Generator != nullptr)
                {
                    entry->second.Generator->Start(origin);
                    results[id] = ClockStartResult::Success;
                }
            }

            MIDI_CLOCK_LOG_INFO(L"Clocks started.");
        }
        MIDI_CLOCK_CATCH_AND_LOG(L"Unable to start one or more clocks.")

        return results;
    }

    _Use_decl_annotations_
    void ClockEngine::StopUnderLock(std::wstring const& id) noexcept
    {
        try
        {
            auto const entry = m_running.find(id);

            if (entry == m_running.end())
            {
                return;
            }

            uint64_t lastTimestamp{ 0 };

            if (entry->second.Generator != nullptr)
            {
                lastTimestamp = entry->second.Generator->Stop();
                entry->second.Generator.reset();
            }

            if (entry->second.Connection != nullptr)
            {
                // Pulses are already in the service queue, so closing the connection now would
                // cut them off along with the stop message that follows them.
                auto const deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(1500);

                while (midi2::MidiClock::Now() <= lastTimestamp &&
                    std::chrono::steady_clock::now() < deadline)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                }

                // the service still has to hand the last message on once its timestamp arrives
                std::this_thread::sleep_for(std::chrono::milliseconds(25));

                if (m_session != nullptr)
                {
                    m_session.DisconnectEndpointConnection(entry->second.Connection.ConnectionId());
                }
            }

            m_running.erase(entry);
        }
        MIDI_CLOCK_CATCH_AND_LOG(L"Unable to stop a clock cleanly.")
    }

    _Use_decl_annotations_
    void ClockEngine::Stop(std::wstring const& id) noexcept
    {
        std::lock_guard<std::recursive_mutex> const guard{ m_lock };

        StopUnderLock(id);
    }

    void ClockEngine::StopAll() noexcept
    {
        try
        {
            std::lock_guard<std::recursive_mutex> const guard{ m_lock };

            std::vector<std::wstring> ids{};

            for (auto const& entry : m_running)
            {
                ids.push_back(entry.first);
            }

            // Every generator is asked to stop before any of them is waited out, so a set of
            // clocks stops together rather than one lingering behind the next one's drain.
            for (auto const& id : ids)
            {
                auto const entry = m_running.find(id);

                if (entry != m_running.end() && entry->second.Generator != nullptr)
                {
                    entry->second.Generator->Stop();
                }
            }

            for (auto const& id : ids)
            {
                StopUnderLock(id);
            }
        }
        MIDI_CLOCK_CATCH_AND_LOG(L"Unable to stop the clocks.")
    }

    _Use_decl_annotations_
    void ClockEngine::SetBeatsPerMinute(std::wstring const& id, double beatsPerMinute) noexcept
    {
        try
        {
            std::lock_guard<std::recursive_mutex> const guard{ m_lock };

            auto const entry = m_running.find(id);

            if (entry != m_running.end() && entry->second.Generator != nullptr)
            {
                entry->second.Generator->BeatsPerMinute(beatsPerMinute);
            }
        }
        MIDI_CLOCK_CATCH_AND_LOG(L"Unable to change the tempo of a running clock.")
    }

    _Use_decl_annotations_
    bool ClockEngine::IsRunning(std::wstring const& id) const noexcept
    {
        std::lock_guard<std::recursive_mutex> const guard{ m_lock };

        return m_running.find(id) != m_running.end();
    }

    std::vector<std::wstring> ClockEngine::RunningIds() const noexcept
    {
        std::vector<std::wstring> ids{};

        try
        {
            std::lock_guard<std::recursive_mutex> const guard{ m_lock };

            for (auto const& entry : m_running)
            {
                ids.push_back(entry.first);
            }
        }
        catch (...)
        {
        }

        return ids;
    }
}
