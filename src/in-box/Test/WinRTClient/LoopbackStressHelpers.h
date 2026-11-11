// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Shared by the basic loopback and MIDI 2.0 loopback stress tests. Include after stdafx.h.

#include <algorithm>
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace LoopbackStress
{
    using StressClock = std::chrono::steady_clock;

    // MIDI 1.0 note on in a UMP, group 1, channel 1. The note number varies so no two neighbors match.
    constexpr uint32_t NoteOnWord(_In_ uint32_t const index) noexcept
    {
        return 0x20900040 | ((index & 0x7F) << 8);
    }

    // The same note on, as a WinMM short message
    constexpr DWORD NoteOnShortMessage(_In_ uint32_t const index) noexcept
    {
        return 0x00400090 | ((index & 0x7F) << 8);
    }

    // Every run gets identifiers of its own, so a loopback left behind by an aborted run can't collide
    inline std::wstring MakeUniqueId(_In_ std::wstring const& prefix)
    {
        return prefix + std::to_wstring(GetTickCount64()) + std::to_wstring(GetCurrentProcessId());
    }

    inline long long MillisecondsSince(_In_ StressClock::time_point const start)
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(StressClock::now() - start).count();
    }

    inline bool WaitFor(_In_ std::function<bool()> const& condition, _In_ uint32_t const timeoutMs)
    {
        auto const start = StressClock::now();

        while (MillisecondsSince(start) < timeoutMs)
        {
            if (condition())
            {
                return true;
            }

            Sleep(50);
        }

        return condition();
    }

    // Total and longest time for one step which a test repeats
    struct PhaseTiming
    {
        long long TotalMs{ 0 };
        long long LongestMs{ 0 };

        void Add(_In_ long long const ms) noexcept
        {
            TotalMs += ms;
            LongestMs = (std::max)(LongestMs, ms);
        }
    };

    // Zero when the service isn't running. A different value later means it stopped and started again.
    inline DWORD MidiServiceProcessId() noexcept
    {
        DWORD processId{ 0 };

        auto const manager = ::OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);

        if (manager != nullptr)
        {
            auto const service = ::OpenServiceW(manager, L"midisrv", SERVICE_QUERY_STATUS);

            if (service != nullptr)
            {
                SERVICE_STATUS_PROCESS status{};
                DWORD needed{ 0 };

                if (::QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO, reinterpret_cast<LPBYTE>(&status), sizeof(status), &needed) &&
                    status.dwCurrentState == SERVICE_RUNNING)
                {
                    processId = status.dwProcessId;
                }

                ::CloseServiceHandle(service);
            }

            ::CloseServiceHandle(manager);
        }

        return processId;
    }

    // One client of an endpoint: its own session, as a separate application would have.
    class Receiver
    {
    public:
        MidiEndpointConnection Connection{ nullptr };

        std::atomic<uint64_t> Count{ 0 };

        ~Receiver()
        {
            Close();
        }

        bool Open(_In_ winrt::hstring const& endpointDeviceId, _In_ winrt::hstring const& sessionName)
        {
            m_session = MidiSession::Create(sessionName);

            if (m_session == nullptr)
            {
                return false;
            }

            Connection = m_session.CreateEndpointConnection(endpointDeviceId);

            if (Connection == nullptr)
            {
                return false;
            }

            m_token = Connection.MessageReceived([this](auto const&, MidiMessageReceivedEventArgs const& args)
                {
                    if (args.PeekFirstWord() == m_watchFor.load())
                    {
                        m_sawWatchedWord = true;
                    }

                    Count++;
                });

            return Connection.Open();
        }

        // Sends from this connection and waits for this connection to hear it back.
        bool SendAndHearBack(_In_ uint32_t const word, _In_ uint32_t const timeoutMs)
        {
            Watch(word);

            if (!MidiEndpointConnection::SendMessageSucceeded(
                Connection.SendSingleMessageWords(MidiClock::TimestampConstantSendImmediately(), word)))
            {
                return false;
            }

            return WaitToHear(timeoutMs);
        }

        void Watch(_In_ uint32_t const word) noexcept
        {
            m_sawWatchedWord = false;
            m_watchFor = word;
        }

        bool WaitToHear(_In_ uint32_t const timeoutMs)
        {
            return WaitFor([this] { return m_sawWatchedWord.load(); }, timeoutMs);
        }

        void Close() noexcept
        {
            try
            {
                if (Connection != nullptr)
                {
                    Connection.MessageReceived(m_token);

                    if (m_session != nullptr)
                    {
                        m_session.DisconnectEndpointConnection(Connection.ConnectionId());
                    }
                }

                if (m_session != nullptr)
                {
                    m_session.Close();
                }
            }
            catch (...)
            {
            }

            Connection = nullptr;
            m_session = nullptr;
        }

    private:
        MidiSession m_session{ nullptr };
        winrt::event_token m_token{};

        std::atomic<uint32_t> m_watchFor{ 0 };
        std::atomic<bool> m_sawWatchedWord{ false };
    };

    inline std::vector<uint64_t> SnapshotCounts(_In_ std::vector<std::unique_ptr<Receiver>> const& receivers)
    {
        std::vector<uint64_t> counts{};

        for (auto const& receiver : receivers)
        {
            counts.push_back(receiver->Count.load());
        }

        return counts;
    }

    // Sends as fast as one connection can, in batches, until stopped
    class ConnectionFlood
    {
    public:
        ~ConnectionFlood()
        {
            Stop();
        }

        void Start(_In_ MidiEndpointConnection const& connection)
        {
            m_connection = connection;
            m_thread = std::thread([this] { Run(); });
        }

        void Stop() noexcept
        {
            m_stop = true;

            if (m_thread.joinable())
            {
                m_thread.join();
            }
        }

        uint64_t MessagesSent() const noexcept { return m_messagesSent.load(); }
        uint64_t FailedSends() const noexcept { return m_failedSends.load(); }
        bool Threw() const noexcept { return m_threw.load(); }

    private:
        void Run() noexcept
        {
            std::vector<uint32_t> words(32);
            uint32_t note{ 0 };

            while (!m_stop)
            {
                for (auto& word : words)
                {
                    word = NoteOnWord(note++);
                }

                try
                {
                    winrt::array_view<uint32_t> wordArray(words);

                    auto const result = m_connection.SendMultipleMessagesWordArray(
                        MidiClock::TimestampConstantSendImmediately(),
                        0,
                        static_cast<uint32_t>(words.size()),
                        wordArray);

                    if (MidiEndpointConnection::SendMessageSucceeded(result))
                    {
                        m_messagesSent += words.size();
                        continue;
                    }

                    m_failedSends++;
                }
                catch (...)
                {
                    m_threw = true;
                }

                Sleep(1);
            }
        }

        MidiEndpointConnection m_connection{ nullptr };
        std::thread m_thread{};

        std::atomic<bool> m_stop{ false };
        std::atomic<uint64_t> m_messagesSent{ 0 };
        std::atomic<uint64_t> m_failedSends{ 0 };
        std::atomic<bool> m_threw{ false };
    };

    inline void CALLBACK CountingMidiInProc(HMIDIIN, UINT message, DWORD_PTR instance, DWORD_PTR, DWORD_PTR)
    {
        if (message == MIM_DATA && instance != 0)
        {
            reinterpret_cast<std::atomic<uint64_t>*>(instance)->fetch_add(1);
        }
    }

    class WinMMFlood
    {
    public:
        ~WinMMFlood()
        {
            Stop();
        }

        void Start(_In_ HMIDIOUT const output)
        {
            m_output = output;
            m_thread = std::thread([this] { Run(); });
        }

        void Stop() noexcept
        {
            m_stop = true;

            if (m_thread.joinable())
            {
                m_thread.join();
            }
        }

        uint64_t Succeeded() const noexcept { return m_succeeded.load(); }
        uint64_t NoDriver() const noexcept { return m_noDriver.load(); }
        uint64_t OtherFailures() const noexcept { return m_other.load(); }

    private:
        void Run() noexcept
        {
            uint32_t note{ 0 };

            while (!m_stop)
            {
                auto const result = midiOutShortMsg(m_output, NoteOnShortMessage(note++));

                if (result == MMSYSERR_NOERROR)
                {
                    m_succeeded++;
                    continue;
                }

                if (result == MMSYSERR_NODRIVER)
                {
                    m_noDriver++;
                }
                else
                {
                    m_other++;
                }

                Sleep(1);
            }
        }

        HMIDIOUT m_output{ nullptr };
        std::thread m_thread{};

        std::atomic<bool> m_stop{ false };
        std::atomic<uint64_t> m_succeeded{ 0 };
        std::atomic<uint64_t> m_noDriver{ 0 };
        std::atomic<uint64_t> m_other{ 0 };
    };
}
