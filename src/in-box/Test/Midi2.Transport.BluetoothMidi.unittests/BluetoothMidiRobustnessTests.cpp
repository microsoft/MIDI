// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "BluetoothMidiRobustnessTests.h"

#include <atomic>
#include <functional>
#include <map>
#include <random>
#include <set>
#include <stop_token>
#include <thread>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace
{
    // Long enough that a busy test machine never trips it, short enough that a hang fails the
    // test instead of stalling the run.
    constexpr DWORD JoinTimeoutMilliseconds = 5000;

    // The shape both production workers share: look for work and decide whether to sleep while
    // holding the queue lock, then sleep outside it. Stopping works the way the transport's
    // Shutdown does: request the stop, and signal under the queue lock.
    class TestWorker
    {
    public:
        // Runs on the worker after it has checked for a stop and before it takes the queue lock.
        // That is the window a stop used to be lost in.
        std::function<void()> AfterStopCheck{ nullptr };

        ~TestWorker()
        {
            // Never leaves a thread behind, even when a test failed part way through
            if (m_thread.joinable())
            {
                RequestStop();
                m_wakeup.Signal();
                m_thread.join();
            }
        }

        void Start()
        {
            // The worker can ask to stop before this assignment completes, so it stops through a
            // stop source made before the thread rather than through m_thread
            m_thread = std::jthread([this, stopToken = m_stopSource.get_token()](std::stop_token) { Run(stopToken); });
        }

        void Queue(_In_ int const item)
        {
            {
                auto lock = m_queueLock.lock_exclusive();
                m_queue.push_back(item);
            }

            m_wakeup.Signal();
        }

        void RequestStop()
        {
            m_stopSource.request_stop();
            m_wakeup.SignalStop(m_queueLock);
        }

        // False when the worker did not finish in time. It is then woken by hand so the test can
        // still clean up, which only works because the stop is already requested.
        bool JoinWithin(_In_ DWORD const timeoutMilliseconds)
        {
            auto const finished = m_finished.wait(timeoutMilliseconds);

            if (!finished)
            {
                m_wakeup.Signal();
            }

            if (m_thread.joinable())
            {
                m_thread.join();
            }

            return finished;
        }

        size_t Processed() const noexcept
        {
            return m_processed.load();
        }

    private:
        void Run(_In_ std::stop_token stopToken)
        {
            std::stop_callback wakeOnStop{ stopToken, [this]() { m_wakeup.SignalStop(m_queueLock); } };

            while (!stopToken.stop_requested())
            {
                if (AfterStopCheck != nullptr)
                {
                    AfterStopCheck();
                }

                bool haveWork{ false };
                bool stopping{ false };

                {
                    auto lock = m_queueLock.lock_exclusive();

                    if (!m_queue.empty())
                    {
                        m_queue.pop_front();
                        haveWork = true;
                    }
                    else
                    {
                        stopping = !m_wakeup.PrepareToSleep(stopToken.stop_requested());
                    }
                }

                if (stopping)
                {
                    break;
                }

                if (haveWork)
                {
                    m_processed++;
                    continue;
                }

                m_wakeup.Sleep(INFINITE);
            }

            m_finished.SetEvent();
        }

        wil::srwlock m_queueLock;
        std::deque<int> m_queue{ };
        MidiBleWorkerWakeup m_wakeup;
        std::atomic<size_t> m_processed{ 0 };
        wil::slim_event_manual_reset m_finished;
        std::stop_source m_stopSource{ };

        // declared last so it is destroyed first
        std::jthread m_thread;
    };

    MidiBleUtilities::DiscoveredDeviceRetention Retention(
        _In_ std::wstring const& id,
        _In_ uint64_t const lastSeenTimestamp,
        _In_ bool const mustKeep)
    {
        return MidiBleUtilities::DiscoveredDeviceRetention{ winrt::hstring{ id }, lastSeenTimestamp, mustKeep };
    }

    std::set<std::wstring> ToSet(_In_ std::vector<winrt::hstring> const& ids)
    {
        std::set<std::wstring> result{ };

        for (auto const& id : ids)
        {
            result.insert(std::wstring{ id });
        }

        return result;
    }

    constexpr uint64_t StaleAfter = 60000;

    // An operation which finishes only when the test says so, the way a Bluetooth call to a busy
    // or silent device does. The test always releases it and waits for it to finish, so nothing is
    // left running in this DLL after the test returns.
    class HeldOperation
    {
    public:
        HeldOperation() : Operation{ Run(m_release) } { }

        ~HeldOperation()
        {
            Release();
        }

        void Release()
        {
            m_release->SetEvent();

            auto const deadline = GetTickCount64() + JoinTimeoutMilliseconds;

            while (Operation.Status() == winrt::Windows::Foundation::AsyncStatus::Started && GetTickCount64() < deadline)
            {
                ::Sleep(5);
            }
        }

    private:
        std::shared_ptr<wil::slim_event_manual_reset> m_release{ std::make_shared<wil::slim_event_manual_reset>() };

        static winrt::Windows::Foundation::IAsyncOperation<int32_t> Run(std::shared_ptr<wil::slim_event_manual_reset> release)
        {
            co_await winrt::resume_background();
            release->wait();
            co_return 7;
        }

    public:
        winrt::Windows::Foundation::IAsyncOperation<int32_t> Operation;
    };

    uint64_t MillisecondsSince(_In_ uint64_t const start)
    {
        return GetTickCount64() - start;
    }
}


void BluetoothMidiRobustnessTests::TestStopRaisedBeforeSleepIsNotLost()
{
    wil::srwlock queueLock;
    MidiBleWorkerWakeup wakeup;
    std::stop_source stopSource;

    // The worker has just checked for a stop and found none
    VERIFY_IS_FALSE(stopSource.stop_requested());

    // Shutdown lands now, before the worker takes the queue lock
    stopSource.request_stop();
    wakeup.SignalStop(queueLock);

    // The worker finds an empty queue. It must stop rather than reset the event and sleep.
    {
        auto lock = queueLock.lock_exclusive();
        VERIFY_IS_FALSE(wakeup.PrepareToSleep(stopSource.stop_requested()));
    }

    VERIFY_IS_TRUE(wakeup.IsSignaled());
}


void BluetoothMidiRobustnessTests::TestStopRaisedAfterResetWakesTheSleeper()
{
    wil::srwlock queueLock;
    MidiBleWorkerWakeup wakeup;

    {
        auto lock = queueLock.lock_exclusive();
        VERIFY_IS_TRUE(wakeup.PrepareToSleep(false));
    }

    VERIFY_IS_FALSE(wakeup.IsSignaled());

    wakeup.SignalStop(queueLock);

    VERIFY_IS_TRUE(wakeup.Sleep(0));
}


void BluetoothMidiRobustnessTests::TestStopSignalWaitsForTheQueueLock()
{
    wil::srwlock queueLock;
    MidiBleWorkerWakeup wakeup;

    {
        auto lock = queueLock.lock_exclusive();
        VERIFY_IS_TRUE(wakeup.PrepareToSleep(false));
    }

    // While the worker holds the queue lock it is between its stop check and its reset, so the
    // stop signal must wait for it. That ordering is the whole fix.
    auto held = queueLock.lock_exclusive();

    std::thread signaler([&]() { wakeup.SignalStop(queueLock); });

    ::Sleep(100);
    VERIFY_IS_FALSE(wakeup.IsSignaled());

    held.reset();
    signaler.join();

    VERIFY_IS_TRUE(wakeup.IsSignaled());
}


void BluetoothMidiRobustnessTests::TestWorkerStopsWhenStopLandsBetweenCheckAndReset()
{
    TestWorker worker;

    std::atomic<bool> stopRaised{ false };

    // Shutdown arrives in exactly the window which used to lose it
    worker.AfterStopCheck = [&]()
        {
            if (!stopRaised.exchange(true))
            {
                worker.RequestStop();
            }
        };

    worker.Start();

    VERIFY_IS_TRUE(worker.JoinWithin(JoinTimeoutMilliseconds));
    VERIFY_IS_TRUE(stopRaised.load());
}


void BluetoothMidiRobustnessTests::TestWorkerAlwaysStopsUnderRacingShutdown()
{
    std::mt19937 random{ 20261023 };
    std::uniform_int_distribution<int> spins{ 0, 2000 };
    std::uniform_int_distribution<int> items{ 0, 3 };

    constexpr int Iterations = 500;

    for (int iteration = 0; iteration < Iterations; iteration++)
    {
        TestWorker worker;
        worker.Start();

        for (int item = items(random); item > 0; item--)
        {
            worker.Queue(item);
        }

        // Varies where in its loop the worker is when the stop arrives
        for (volatile int spin = spins(random); spin > 0; spin--)
        {
        }

        worker.RequestStop();

        if (!worker.JoinWithin(JoinTimeoutMilliseconds))
        {
            Log::Error(String().Format(L"Worker did not stop on iteration %d", iteration));
            return;
        }
    }

    Log::Comment(String().Format(L"%d racing shutdowns, every worker stopped", Iterations));
}


void BluetoothMidiRobustnessTests::TestQueuedWorkIsNotLostToTheStopCheck()
{
    TestWorker worker;
    worker.Start();

    constexpr int Producers = 4;
    constexpr int ItemsPerProducer = 2000;

    std::vector<std::thread> producers{ };

    for (int producer = 0; producer < Producers; producer++)
    {
        producers.emplace_back([&worker]()
            {
                for (int item = 0; item < ItemsPerProducer; item++)
                {
                    worker.Queue(item);
                }
            });
    }

    for (auto& producer : producers)
    {
        producer.join();
    }

    auto const deadline = GetTickCount64() + JoinTimeoutMilliseconds;

    while (worker.Processed() < static_cast<size_t>(Producers * ItemsPerProducer) && GetTickCount64() < deadline)
    {
        ::Sleep(1);
    }

    VERIFY_ARE_EQUAL(static_cast<size_t>(Producers * ItemsPerProducer), worker.Processed());

    worker.RequestStop();
    VERIFY_IS_TRUE(worker.JoinWithin(JoinTimeoutMilliseconds));
}


void BluetoothMidiRobustnessTests::TestPeripheralWriteFromAttachedClientIsAccepted()
{
    VERIFY_IS_TRUE(MidiBleUtilities::IsWriteFromAttachedPeripheralClient(
        L"BluetoothLE#BluetoothLE00:1a:7d:da:71:13-4c:87:5d:2e:91:07",
        L"BluetoothLE#BluetoothLE00:1a:7d:da:71:13-4c:87:5d:2e:91:07"));
}


void BluetoothMidiRobustnessTests::TestPeripheralWriteIgnoresCase()
{
    VERIFY_IS_TRUE(MidiBleUtilities::IsWriteFromAttachedPeripheralClient(
        L"BluetoothLE#BluetoothLE00:1A:7D:DA:71:13-4C:87:5D:2E:91:07",
        L"bluetoothle#bluetoothle00:1a:7d:da:71:13-4c:87:5d:2e:91:07"));
}


void BluetoothMidiRobustnessTests::TestPeripheralWriteFromAnotherClientIsRejected()
{
    auto const attached = winrt::hstring{ L"BluetoothLE#BluetoothLE00:1a:7d:da:71:13-4c:87:5d:2e:91:07" };

    VERIFY_IS_FALSE(MidiBleUtilities::IsWriteFromAttachedPeripheralClient(
        L"BluetoothLE#BluetoothLE00:1a:7d:da:71:13-4c:87:5d:2e:91:08", attached));

    // a prefix of the approved id, and the approved id with more on the end
    VERIFY_IS_FALSE(MidiBleUtilities::IsWriteFromAttachedPeripheralClient(
        L"BluetoothLE#BluetoothLE00:1a:7d:da:71:13-4c:87:5d:2e:91", attached));

    VERIFY_IS_FALSE(MidiBleUtilities::IsWriteFromAttachedPeripheralClient(
        L"BluetoothLE#BluetoothLE00:1a:7d:da:71:13-4c:87:5d:2e:91:070", attached));
}


void BluetoothMidiRobustnessTests::TestPeripheralWriteWithNoAttachedClientIsRejected()
{
    // A Central waiting for approval has no connection attached
    VERIFY_IS_FALSE(MidiBleUtilities::IsWriteFromAttachedPeripheralClient(
        L"BluetoothLE#BluetoothLE00:1a:7d:da:71:13-4c:87:5d:2e:91:07", L""));
}


void BluetoothMidiRobustnessTests::TestPeripheralWriteWithNoWriterIdIsRejected()
{
    VERIFY_IS_FALSE(MidiBleUtilities::IsWriteFromAttachedPeripheralClient(
        L"", L"BluetoothLE#BluetoothLE00:1a:7d:da:71:13-4c:87:5d:2e:91:07"));

    VERIFY_IS_FALSE(MidiBleUtilities::IsWriteFromAttachedPeripheralClient(L"", L""));
}


void BluetoothMidiRobustnessTests::TestEvictionLeavesATableWithRoomAlone()
{
    uint64_t const now = 1000000;

    std::vector<MidiBleUtilities::DiscoveredDeviceRetention> devices{
        Retention(L"A", now - 1000, false),
        Retention(L"B", now - 2000, false),
        Retention(L"C", now, true),
    };

    bool canAdd{ false };
    auto const evicted = MidiBleUtilities::SelectDiscoveredDevicesToEvict(devices, 10, now, StaleAfter, canAdd);

    VERIFY_IS_TRUE(evicted.empty());
    VERIFY_IS_TRUE(canAdd);
}


void BluetoothMidiRobustnessTests::TestEvictionDropsStaleDevicesEvenWithRoom()
{
    uint64_t const now = 1000000;

    std::vector<MidiBleUtilities::DiscoveredDeviceRetention> devices{
        Retention(L"Fresh", now - 1000, false),
        Retention(L"Stale", now - StaleAfter - 1, false),
        Retention(L"NeverHeard", 0, false),
    };

    bool canAdd{ false };
    auto const evicted = ToSet(MidiBleUtilities::SelectDiscoveredDevicesToEvict(devices, 10, now, StaleAfter, canAdd));

    VERIFY_ARE_EQUAL(static_cast<size_t>(2), evicted.size());
    VERIFY_IS_TRUE(evicted.contains(L"Stale"));
    VERIFY_IS_TRUE(evicted.contains(L"NeverHeard"));
    VERIFY_IS_TRUE(canAdd);
}


void BluetoothMidiRobustnessTests::TestEvictionNeverDropsAKeptDevice()
{
    uint64_t const now = 1000000;

    // Paired devices the GATT watcher reported have never been heard, and must stay anyway
    std::vector<MidiBleUtilities::DiscoveredDeviceRetention> devices{
        Retention(L"PairedNeverHeard", 0, true),
        Retention(L"WantedButAsleep", now - 10 * StaleAfter, true),
        Retention(L"Stranger", now - 10 * StaleAfter, false),
    };

    bool canAdd{ false };
    auto const evicted = ToSet(MidiBleUtilities::SelectDiscoveredDevicesToEvict(devices, 10, now, StaleAfter, canAdd));

    VERIFY_ARE_EQUAL(static_cast<size_t>(1), evicted.size());
    VERIFY_IS_TRUE(evicted.contains(L"Stranger"));
    VERIFY_IS_TRUE(canAdd);
}


void BluetoothMidiRobustnessTests::TestEvictionDropsTheLongestUnheardWhenFull()
{
    uint64_t const now = 1000000;

    std::vector<MidiBleUtilities::DiscoveredDeviceRetention> devices{
        Retention(L"Newest", now - 10, false),
        Retention(L"Oldest", now - 300, false),
        Retention(L"KeptAndOlder", now - 900, true),
        Retention(L"Middle", now - 200, false),
    };

    bool canAdd{ false };
    auto const evicted = ToSet(MidiBleUtilities::SelectDiscoveredDevicesToEvict(devices, 4, now, StaleAfter, canAdd));

    VERIFY_ARE_EQUAL(static_cast<size_t>(1), evicted.size());
    VERIFY_IS_TRUE(evicted.contains(L"Oldest"));
    VERIFY_IS_TRUE(canAdd);
}


void BluetoothMidiRobustnessTests::TestEvictionTurnsAwayWhenFullOfKeptDevices()
{
    uint64_t const now = 1000000;

    std::vector<MidiBleUtilities::DiscoveredDeviceRetention> devices{
        Retention(L"A", 0, true),
        Retention(L"B", now - 10 * StaleAfter, true),
        Retention(L"C", now, true),
    };

    bool canAdd{ true };
    auto const evicted = MidiBleUtilities::SelectDiscoveredDevicesToEvict(devices, 3, now, StaleAfter, canAdd);

    VERIFY_IS_TRUE(evicted.empty());
    VERIFY_IS_FALSE(canAdd);
}


void BluetoothMidiRobustnessTests::TestEvictionBoundsTheTableUnderAnAddressFlood()
{
    constexpr size_t MaxDevices = MIDI_BLE_MAX_DISCOVERED_DEVICES;
    constexpr int FloodAddresses = 10000;

    struct Entry
    {
        uint64_t LastSeenTimestamp{ 0 };
        bool MustKeep{ false };
    };

    std::map<std::wstring, Entry> table{ };

    // The customer's own gear, which must survive the flood
    for (int kept = 0; kept < 5; kept++)
    {
        table[L"KEPT" + std::to_wstring(kept)] = Entry{ 0, true };
    }

    uint64_t now = 1000000;

    // A new random address every 10 ms, all advertising the MIDI service
    for (int address = 0; address < FloodAddresses; address++)
    {
        now += 10;

        std::vector<MidiBleUtilities::DiscoveredDeviceRetention> devices{ };
        devices.reserve(table.size());

        for (auto const& entry : table)
        {
            devices.push_back(Retention(entry.first, entry.second.LastSeenTimestamp, entry.second.MustKeep));
        }

        bool canAdd{ false };
        auto const evicted = MidiBleUtilities::SelectDiscoveredDevicesToEvict(devices, MaxDevices, now, StaleAfter, canAdd);

        for (auto const& id : evicted)
        {
            table.erase(std::wstring{ id });
        }

        if (canAdd)
        {
            table[L"FLOOD" + std::to_wstring(address)] = Entry{ now, false };
        }

        if (table.size() > MaxDevices)
        {
            Log::Error(String().Format(L"Table grew to %u entries after %d addresses", static_cast<uint32_t>(table.size()), address + 1));
            return;
        }
    }

    for (int kept = 0; kept < 5; kept++)
    {
        VERIFY_IS_TRUE(table.contains(L"KEPT" + std::to_wstring(kept)));
    }

    VERIFY_ARE_EQUAL(MaxDevices, table.size());

    // The newest address always gets in, so a real device which starts advertising during a
    // flood is still found
    VERIFY_IS_TRUE(table.contains(L"FLOOD" + std::to_wstring(FloodAddresses - 1)));
}

void BluetoothMidiRobustnessTests::TestAwaitReturnsTheResultWhenTheOperationCompletes()
{
    HeldOperation held;
    held.Release();

    MidiBleUtilities::AwaitOutcome outcome{ };

    auto const result = MidiBleUtilities::AwaitWithTimeoutOrAbandon(
        held.Operation, 5000, int32_t{ -1 }, []() noexcept { return false; }, outcome);

    VERIFY_ARE_EQUAL(7, result);
    VERIFY_IS_FALSE(outcome.TimedOut);
    VERIFY_IS_FALSE(outcome.Failed);
    VERIFY_IS_FALSE(outcome.Abandoned);
}


void BluetoothMidiRobustnessTests::TestAwaitGivesUpSoonAfterShutdownStarts()
{
    HeldOperation held;
    std::atomic<bool> shuttingDown{ false };

    // The connection starts shutting down while its writer is waiting on Bluetooth
    std::thread shutdown([&shuttingDown]()
        {
            ::Sleep(300);
            shuttingDown = true;
        });

    auto const start = GetTickCount64();
    MidiBleUtilities::AwaitOutcome outcome{ };

    auto const result = MidiBleUtilities::AwaitWithTimeoutOrAbandon(
        held.Operation, 10000, int32_t{ -1 }, [&shuttingDown]() noexcept { return shuttingDown.load(); }, outcome);

    auto const elapsed = MillisecondsSince(start);
    shutdown.join();

    Log::Comment(String().Format(L"Gave up after %llu ms of a 10000 ms timeout", elapsed));

    VERIFY_ARE_EQUAL(-1, result);
    VERIFY_IS_TRUE(outcome.Abandoned);
    VERIFY_IS_TRUE(outcome.TimedOut);

    // 300 ms until the shutdown, then at most one poll slice, plus slack for a busy machine
    VERIFY_IS_LESS_THAN(elapsed, static_cast<uint64_t>(300 + MIDI_BLE_AWAIT_POLL_SLICE_MS + 1500));
}


void BluetoothMidiRobustnessTests::TestAwaitGivesUpAtOnceWhenAlreadyShuttingDown()
{
    HeldOperation held;

    auto const start = GetTickCount64();
    MidiBleUtilities::AwaitOutcome outcome{ };

    auto const result = MidiBleUtilities::AwaitWithTimeoutOrAbandon(
        held.Operation, 5000, int32_t{ -1 }, []() noexcept { return true; }, outcome);

    auto const elapsed = MillisecondsSince(start);

    VERIFY_ARE_EQUAL(-1, result);
    VERIFY_IS_TRUE(outcome.Abandoned);

    // without waiting even one slice
    VERIFY_IS_LESS_THAN(elapsed, static_cast<uint64_t>(MIDI_BLE_AWAIT_POLL_SLICE_MS));
}


void BluetoothMidiRobustnessTests::TestAwaitRunsToItsTimeoutWhenNotShuttingDown()
{
    HeldOperation held;

    constexpr uint32_t Timeout = 600;

    auto const start = GetTickCount64();
    MidiBleUtilities::AwaitOutcome outcome{ };

    auto const result = MidiBleUtilities::AwaitWithTimeoutOrAbandon(
        held.Operation, Timeout, int32_t{ -1 }, []() noexcept { return false; }, outcome);

    auto const elapsed = MillisecondsSince(start);

    VERIFY_ARE_EQUAL(-1, result);
    VERIFY_IS_TRUE(outcome.TimedOut);
    VERIFY_IS_FALSE(outcome.Abandoned);

    // GetTickCount64 moves in steps of about 16 ms
    VERIFY_IS_GREATER_THAN_OR_EQUAL(elapsed + 32, static_cast<uint64_t>(Timeout));
}
