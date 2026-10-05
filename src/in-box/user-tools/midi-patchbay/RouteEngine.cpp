// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "RouteEngine.h"
#include "StringResources.h"

#include "BeatClockGenerator.h"
#include "LfoMessageGenerator.h"
#include "TimeCodeGenerator.h"

#include <midi_timestamp.h>
#include <ump_helpers.h>

namespace internal = ::WindowsMidiServicesInternal;

namespace midipatchbay
{
    namespace
    {
        constexpr uint32_t MaximumWordsPerUmp = 4;

        // What one send carries at most. A burst larger than this goes out in pieces.
        constexpr uint32_t MaximumSendBufferWords = 256;

        // What a throttle takes from its queue at a time.
        constexpr uint32_t ThrottleBatchWords = 256;

        constexpr wchar_t SessionName[] = L"MIDI Patchbay";

        // Long enough to hear, short enough that the button does not feel stuck.
        constexpr uint32_t TestNoteMilliseconds = 350;

        // After the last message a stopped generator scheduled, before its connections close, so
        // a stop or a last full frame is not lost with them. The same margin MIDI Clock uses.
        constexpr uint64_t DrainMarginMilliseconds = 25;
        constexpr uint64_t LongestDrainMilliseconds = 2000;

        std::wstring LowerCopy(_In_ std::wstring value) noexcept
        {
            std::transform(value.begin(), value.end(), value.begin(),
                [](wchar_t c) { return static_cast<wchar_t>(::towlower(c)); });

            return value;
        }
    }

    struct RouteEngine::Counters
    {
        std::atomic<uint64_t> In{ 0 };
        std::atomic<uint64_t> Out{ 0 };
        std::atomic<uint64_t> Failures{ 0 };
        std::atomic<uint64_t> Dropped{ 0 };
    };

    // A link into a destination, ready to send.
    struct RouteEngine::Leaf
    {
        winrt::com_ptr<IMidiEndpointConnectionRaw> Destination{ nullptr };

        // What one send may carry. Never less than the longest message.
        uint32_t BufferWords{ 0 };

        uint32_t Cell{ 0 };

        // Set when the patch waits for each send to complete. What reaches the destination is
        // queued here, and the destination's send thread sends it on.
        std::unique_ptr<SendQueue> WaitQueue{};
        DestinationSender* Sender{ nullptr };

        bool Usable{ false };
    };

    // Sends what is queued for one destination by patches that wait for each send to complete.
    // One thread for each destination, so a send that waits holds up only the device it is
    // waiting for.
    struct RouteEngine::DestinationSender
    {
        winrt::com_ptr<IMidiEndpointConnectionRaw> Destination{ nullptr };

        // Owned by the runtime, which stops this thread before it lets them go.
        std::vector<Leaf*> Leaves{};
        Counters* Cells{ nullptr };

        DestinationSender() noexcept = default;
        DestinationSender(DestinationSender const&) = delete;
        DestinationSender& operator=(DestinationSender const&) = delete;

        ~DestinationSender() noexcept
        {
            Stop();
        }

        bool Start() noexcept
        {
            try
            {
                auto const maxWords = Destination == nullptr ? 0 : Destination->GetSupportedMaxMidiWordsPerTransmission();

                if (maxWords < MaximumWordsPerUmp || Leaves.empty())
                {
                    return false;
                }

                m_buffer.assign(maxWords, 0);

                if (!m_wake.try_create(wil::EventOptions::None, nullptr))
                {
                    return false;
                }

                // At MIDI 1.0 wire speed, a message can be due every millisecond or so
                m_paceTimer.reset(::CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS));

                if (!m_paceTimer)
                {
                    m_paceTimer.reset(::CreateWaitableTimerExW(nullptr, nullptr, 0, TIMER_ALL_ACCESS));
                }

                if (!m_paceTimer)
                {
                    return false;
                }

                m_ticksPerSecond = internal::GetMidiTimestampFrequency();
                m_thread = std::thread([this]() { Run(); });

                return true;
            }
            MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to start a send thread.")

            return false;
        }

        bool IsRunning() const noexcept
        {
            return m_thread.joinable();
        }

        void RequestStop() noexcept
        {
            m_stopRequested.store(true);

            if (m_wake)
            {
                m_wake.SetEvent();
            }
        }

        // The thread finishes the send it is in first
        void Stop() noexcept
        {
            RequestStop();

            try
            {
                if (m_thread.joinable())
                {
                    m_thread.join();
                }
            }
            MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to stop a send thread.")
        }

        void Wake() noexcept
        {
            m_wake.SetEvent();
        }

    private:
        void Run() noexcept
        {
            try
            {
                HANDLE const handles[]{ m_wake.get(), m_paceTimer.get() };

                while (!m_stopRequested.load())
                {
                    if (::WaitForMultipleObjects(ARRAYSIZE(handles), handles, FALSE, INFINITE) == WAIT_FAILED)
                    {
                        // never spin on a broken handle
                        LOG_LAST_ERROR();
                        break;
                    }

                    auto const waitTicks = SendWhatIsAllowed();

                    if (waitTicks > 0)
                    {
                        ArmPaceTimer(waitTicks);
                    }
                }
            }
            MIDI_PATCHBAY_CATCH_AND_LOG(L"A send thread failed.")
        }

        // Takes from each link in turn, so a busy one cannot hold up the others. Returns how
        // long until the next message may go, or zero.
        uint64_t SendWhatIsAllowed() noexcept
        {
            uint64_t earliestWait{ 0 };
            bool sentAny{ true };

            while (sentAny && !m_stopRequested.load())
            {
                sentAny = false;
                earliestWait = 0;

                for (auto* leaf : Leaves)
                {
                    if (m_stopRequested.load())
                    {
                        break;
                    }

                    uint64_t timestamp{ 0 };
                    uint32_t messageCount{ 0 };
                    uint64_t waitTicks{ 0 };

                    auto const wordCount = leaf->WaitQueue->Take(
                        internal::GetCurrentMidiTimestamp(),
                        m_buffer.data(),
                        static_cast<uint32_t>(m_buffer.size()),
                        timestamp,
                        messageCount,
                        waitTicks);

                    if (wordCount > 0)
                    {
                        sentAny = true;

                        if (FAILED(Destination->SendMidiMessagesRaw(timestamp, wordCount, m_buffer.data())) &&
                            Cells != nullptr)
                        {
                            Cells[leaf->Cell].Failures.fetch_add(1, std::memory_order_relaxed);
                        }
                    }
                    else if (waitTicks > 0 && (earliestWait == 0 || waitTicks < earliestWait))
                    {
                        earliestWait = waitTicks;
                    }
                }
            }

            return earliestWait;
        }

        void ArmPaceTimer(_In_ uint64_t const ticks) noexcept
        {
            if (m_ticksPerSecond == 0)
            {
                return;
            }

            // relative, in 100 nanosecond units, and never zero
            LARGE_INTEGER dueTime{};
            dueTime.QuadPart = -static_cast<LONGLONG>((std::max)((ticks * 10'000'000ull) / m_ticksPerSecond, 1ull));

            // If this fails, the next message to arrive still wakes the thread, only later
            LOG_IF_WIN32_BOOL_FALSE(::SetWaitableTimer(m_paceTimer.get(), &dueTime, 0, nullptr, nullptr, FALSE));
        }

        std::vector<uint32_t> m_buffer{};

        wil::unique_event_nothrow m_wake{};
        wil::unique_handle m_paceTimer{};

        std::atomic<bool> m_stopRequested{ false };
        uint64_t m_ticksPerSecond{ 0 };

        std::thread m_thread{};
    };

    // Where one thread works: the receive callback of one source, or one throttle. Each has its
    // own send buffers, so no two threads ever share one.
    struct RouteEngine::Context final : RouteSink
    {
        Runtime* Owner{ nullptr };
        uint64_t Timestamp{ 0 };

        void Prepare(_In_ Runtime& owner);
        void FlushAll() noexcept;

        void CountLink(_In_ uint32_t cell) noexcept override;
        void CountBlock(_In_ uint32_t cell, _In_ bool passed) noexcept override;

        void Send(
            _In_ uint32_t leaf,
            _In_reads_(wordCount) uint32_t const* words,
            _In_ uint8_t wordCount) noexcept override;

        void Throttle(
            _In_ uint32_t throttle,
            _In_reads_(wordCount) uint32_t const* words,
            _In_ uint8_t wordCount) noexcept override;

        ::midipatchbay::BlockState* StateOf(_In_ uint32_t state) noexcept override;

        void Clock(
            _In_ uint32_t target,
            _In_reads_(wordCount) uint32_t const* words,
            _In_ uint8_t wordCount) noexcept override;

    private:
        struct Buffer
        {
            std::vector<uint32_t> Words{};
            uint32_t Used{ 0 };
            uint32_t Messages{ 0 };
            bool Touched{ false };
        };

        void Flush(_In_ uint32_t leaf) noexcept;

        // One for each leaf, sized when the graph is applied.
        std::vector<Buffer> m_buffers{};

        // Reserved for every leaf, so adding to it never allocates.
        std::vector<uint32_t> m_touched{};
    };

    // A throttle's queue, and the thread that runs what comes after the throttle at its pace.
    struct RouteEngine::ThrottleRunner
    {
        Runtime* Owner{ nullptr };
        uint32_t Index{ 0 };
        uint32_t Cell{ 0 };

        SendQueue Queue{};
        Context Work{};

        ThrottleRunner() noexcept = default;
        ThrottleRunner(ThrottleRunner const&) = delete;
        ThrottleRunner& operator=(ThrottleRunner const&) = delete;

        ~ThrottleRunner() noexcept
        {
            Stop();
        }

        bool Start() noexcept
        {
            try
            {
                if (!m_wake.try_create(wil::EventOptions::None, nullptr))
                {
                    return false;
                }

                m_paceTimer.reset(::CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS));

                if (!m_paceTimer)
                {
                    m_paceTimer.reset(::CreateWaitableTimerExW(nullptr, nullptr, 0, TIMER_ALL_ACCESS));
                }

                if (!m_paceTimer)
                {
                    return false;
                }

                m_ticksPerSecond = internal::GetMidiTimestampFrequency();
                m_running.store(true);
                m_thread = std::thread([this]() { Run(); });

                return true;
            }
            MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to start a throttle thread.")

            m_running.store(false);
            return false;
        }

        void RequestStop() noexcept
        {
            m_stopRequested.store(true);
            m_running.store(false);

            if (m_wake)
            {
                m_wake.SetEvent();
            }
        }

        // The thread finishes the message it is on first
        void Stop() noexcept
        {
            RequestStop();

            try
            {
                if (m_thread.joinable())
                {
                    m_thread.join();
                }
            }
            MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to stop a throttle thread.")
        }

        // Any thread. False when there is no room, or nothing is running to empty the queue.
        bool Push(
            _In_ uint64_t timestamp,
            _In_reads_(wordCount) uint32_t const* words,
            _In_ uint32_t wordCount) noexcept
        {
            if (!m_running.load(std::memory_order_relaxed))
            {
                return false;
            }

            if (!Queue.Push(timestamp, words, wordCount))
            {
                return false;
            }

            m_wake.SetEvent();
            return true;
        }

    private:
        void Run() noexcept
        {
            try
            {
                HANDLE const handles[]{ m_wake.get(), m_paceTimer.get() };

                while (!m_stopRequested.load())
                {
                    if (::WaitForMultipleObjects(ARRAYSIZE(handles), handles, FALSE, INFINITE) == WAIT_FAILED)
                    {
                        // never spin on a broken handle
                        LOG_LAST_ERROR();
                        break;
                    }

                    auto const waitTicks = RunWhatIsAllowed();

                    if (waitTicks > 0)
                    {
                        ArmPaceTimer(waitTicks);
                    }
                }
            }
            MIDI_PATCHBAY_CATCH_AND_LOG(L"A throttle thread failed.")

            m_running.store(false);
        }

        // Runs what the speed lets go now through what comes after the throttle. Returns how
        // long until the next message may go, or zero when nothing is waiting. Defined after
        // Runtime, which it reads.
        uint64_t RunWhatIsAllowed() noexcept;

        void ArmPaceTimer(_In_ uint64_t const ticks) noexcept
        {
            if (m_ticksPerSecond == 0)
            {
                return;
            }

            // relative, in 100 nanosecond units, and never zero
            LARGE_INTEGER dueTime{};
            dueTime.QuadPart = -static_cast<LONGLONG>((std::max)((ticks * 10'000'000ull) / m_ticksPerSecond, 1ull));

            // If this fails, the next message to arrive still wakes the thread, only later
            LOG_IF_WIN32_BOOL_FALSE(::SetWaitableTimer(m_paceTimer.get(), &dueTime, 0, nullptr, nullptr, FALSE));
        }

        std::array<uint32_t, ThrottleBatchWords> m_buffer{};

        wil::unique_event_nothrow m_wake{};
        wil::unique_handle m_paceTimer{};

        std::atomic<bool> m_stopRequested{ false };
        std::atomic<bool> m_running{ false };
        uint64_t m_ticksPerSecond{ 0 };

        std::thread m_thread{};
    };

    // Everything one applied graph needs. Replaced whole when the graph changes.
    struct RouteEngine::Runtime
    {
        RouteGraph Graph{};

        std::unique_ptr<Counters[]> Cells{};

        // Sized once, before anything takes their addresses.
        std::vector<Leaf> Leaves{};

        std::vector<std::unique_ptr<ThrottleRunner>> Throttles{};
        std::vector<std::unique_ptr<DestinationSender>> Senders{};

        // One for each clock divider. Shared with the graphs before and after this one, so a
        // divider keeps its count through a change somewhere else.
        std::vector<std::shared_ptr<::midipatchbay::BlockState>> States{};

        // The LFO each of the graph's clock targets is, filled in before anything can reach this
        // graph and never changed after. Empty where that LFO isn't running.
        std::vector<std::shared_ptr<midiapp::LfoMessageGenerator>> ClockTargets{};

        // Each throttle finishes the message it is on, then each send thread the send it is in,
        // and whatever is still queued goes with the graph. All are told first, so they stop
        // together.
        void StopThreads() noexcept
        {
            for (auto& runner : Throttles)
            {
                if (runner != nullptr)
                {
                    runner->RequestStop();
                }
            }

            for (auto& runner : Throttles)
            {
                if (runner != nullptr)
                {
                    runner->Stop();
                }
            }

            for (auto& sender : Senders)
            {
                if (sender != nullptr)
                {
                    sender->RequestStop();
                }
            }

            for (auto& sender : Senders)
            {
                if (sender != nullptr)
                {
                    sender->Stop();
                }
            }
        }
    };

    uint64_t RouteEngine::ThrottleRunner::RunWhatIsAllowed() noexcept
    {
        auto const& graph = Owner->Graph;
        auto const& throttle = graph.Throttles[Index];

        while (!m_stopRequested.load())
        {
            uint64_t timestamp{ 0 };
            uint32_t messageCount{ 0 };
            uint64_t waitTicks{ 0 };

            auto const taken = Queue.Take(
                internal::GetCurrentMidiTimestamp(),
                m_buffer.data(),
                static_cast<uint32_t>(m_buffer.size()),
                timestamp,
                messageCount,
                waitTicks);

            if (taken == 0)
            {
                return waitTicks;
            }

            Work.Timestamp = timestamp;

            uint32_t position{ 0 };

            while (position < taken)
            {
                auto const length = internal::GetUmpLengthInMidiWordsFromFirstWord(m_buffer[position]);

                if (length == 0 || length > MaximumWordsPerUmp || position + length > taken)
                {
                    break;
                }

                for (uint32_t i = 0; i < throttle.EdgeCount; i++)
                {
                    RunEdge(graph, graph.Edges[throttle.FirstEdge + i],
                        m_buffer.data() + position, static_cast<uint8_t>(length), Work);
                }

                position += length;
            }

            Work.FlushAll();
        }

        return 0;
    }

    // What one endpoint does with what arrives from it: the graph, the links out of it, and the
    // send buffers its callback thread uses. Replaced whole when the graph changes.
    struct RouteEngine::HubPlan
    {
        std::shared_ptr<Runtime> Owner{};

        // Indexes into the graph's roots.
        std::vector<uint32_t> Roots{};

        Context Work{};
    };

    // One for every open connection, registered as its raw messages-received callback before it
    // opens. Walks every arriving message through the links out of that endpoint, and ignores
    // what arrives while the endpoint has none.
    struct RouteEngine::SourceHub : winrt::implements<SourceHub, IMidiEndpointConnectionMessagesReceivedCallback>
    {
        void Plan(_In_ std::shared_ptr<HubPlan> plan) noexcept
        {
            m_plan.store(std::move(plan));
        }

        STDMETHOD(MessagesReceived)(
            GUID sessionId,
            GUID connectionId,
            UINT64 timestamp,
            UINT32 wordCount,
            UINT32 const* messages) override
        {
            UNREFERENCED_PARAMETER(sessionId);
            UNREFERENCED_PARAMETER(connectionId);

            if (messages != nullptr && wordCount > 0)
            {
                OnMessagesReceived(timestamp, wordCount, messages);
            }

            return S_OK;
        }

        void OnMessagesReceived(
            _In_ uint64_t timestamp,
            _In_ uint32_t wordCount,
            _In_reads_(wordCount) uint32_t const* messages) noexcept
        {
            // A callback still running when the plan is replaced finishes on the one it took.
            auto const plan = m_plan.load();

            if (plan == nullptr || plan->Owner == nullptr || plan->Roots.empty())
            {
                return;
            }

            auto const& graph = plan->Owner->Graph;
            auto& work = plan->Work;

            work.Timestamp = timestamp;

            uint32_t position{ 0 };

            while (position < wordCount)
            {
                auto const length = internal::GetUmpLengthInMidiWordsFromFirstWord(messages[position]);

                if (length == 0 || length > MaximumWordsPerUmp || position + length > wordCount)
                {
                    // The SDK only hands over complete UMPs, so a partial tail means something
                    // upstream is malformed. Stop rather than read past the end of the buffer.
                    break;
                }

                for (auto const root : plan->Roots)
                {
                    RunRoot(graph, graph.Roots[root], messages + position, static_cast<uint8_t>(length), work);
                }

                position += length;
            }

            work.FlushAll();
        }

    private:
        std::atomic<std::shared_ptr<HubPlan>> m_plan{};
    };

    // An endpoint connection, kept for as long as some graph wants it. Every one gets a hub,
    // because the COM extensions only take a callback before a connection opens, and an endpoint
    // that is only a destination now can be a source after the next edit.
    struct RouteEngine::Connection
    {
        midi2::MidiEndpointConnection Endpoint{ nullptr };
        winrt::com_ptr<IMidiEndpointConnectionRaw> Raw{ nullptr };
        winrt::com_ptr<SourceHub> Hub{ nullptr };
        bool Opened{ false };
    };

    // Where one generator's messages go: the graph, its links, and the send buffers its thread
    // uses. Replaced whole when the graph changes, like a hub's plan.
    struct RouteEngine::GeneratorPlan
    {
        std::shared_ptr<Runtime> Owner{};

        // Index into the graph's generators.
        uint32_t Index{ 0 };

        Context Work{};
    };

    // One running generator. The generators in midi-app-shared do the timing, the same ones MIDI
    // Clock and MIDI Glass use; this hands what they make to the graph, and outlives any one
    // graph so a change elsewhere does not restart it.
    class RouteEngine::GeneratorRunner
    {
    public:
        GeneratorRunner(_In_ BlockKind kind, _In_ std::wstring restartSignature) noexcept :
            m_kind(kind),
            m_restartSignature(std::move(restartSignature))
        {
        }

        ~GeneratorRunner() noexcept
        {
            Stop();
        }

        GeneratorRunner(GeneratorRunner const&) = delete;
        GeneratorRunner& operator=(GeneratorRunner const&) = delete;

        std::wstring const& RestartSignature() const noexcept
        {
            return m_restartSignature;
        }

        void Plan(_In_ std::shared_ptr<GeneratorPlan> plan) noexcept
        {
            m_plan.store(std::move(plan));
        }

        // Builds the generator without starting it, so a clock input can find an LFO before
        // anything reaches the graph it runs in.
        bool Create(_In_ BlockSettings const& settings, _In_ bool followsClock) noexcept
        {
            try
            {
                midiapp::GeneratorSink sink = [this](uint64_t timestamp, uint32_t const* words, uint32_t wordCount)
                    {
                        Emit(timestamp, words, wordCount);
                    };

                switch (m_kind)
                {
                case BlockKind::ClockGenerator:
                {
                    midiapp::BeatClockGeneratorOptions options{};

                    options.BeatsPerMinute = settings.Clock.BeatsPerMinute;
                    options.GroupIndexes = { settings.Clock.Group };
                    options.SendStartMessage = settings.Clock.SendStartStop;
                    options.SendStopMessage = settings.Clock.SendStartStop;
                    options.SwingPercent = settings.Clock.SwingPercent;
                    options.SwingSubdivision = settings.Clock.SwingSubdivision;

                    m_clock = std::make_unique<midiapp::BeatClockGenerator>(std::move(sink), std::move(options));
                    break;
                }

                case BlockKind::TimeCodeGenerator:
                {
                    midiapp::TimeCodeGeneratorOptions options{};

                    options.FrameRate = settings.TimeCode.FrameRate;
                    options.StartPosition = settings.TimeCode.Start;
                    options.GroupIndexes = { settings.TimeCode.Group };
                    options.SendFullFrameMessages = settings.TimeCode.SendFullFrame;

                    m_timeCode = std::make_unique<midiapp::TimeCodeGenerator>(std::move(sink), std::move(options));
                    break;
                }

                case BlockKind::LfoGenerator:
                    m_followsClock = followsClock;

                    // Shared, because a graph that is being replaced can still hand it a pulse
                    // after this runner has gone. Only its own thread calls the sink, and Stop
                    // ends that thread.
                    m_lfo = std::make_shared<midiapp::LfoMessageGenerator>(std::move(sink), LfoOptions(settings.Lfo, followsClock));
                    break;

                default:
                    return false;
                }

                m_liveSignature = BlockSettingsSignature(m_kind, settings);
                return true;
            }
            MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to create a generator.")

            return false;
        }

        bool Start(_In_ uint64_t originTimestamp) noexcept
        {
            try
            {
                if (m_clock != nullptr)
                {
                    m_clock->Start(originTimestamp);
                }
                else if (m_timeCode != nullptr)
                {
                    m_timeCode->Start(originTimestamp);
                }
                else if (m_lfo != nullptr)
                {
                    m_lfo->Start(originTimestamp);
                }
                else
                {
                    return false;
                }

                return true;
            }
            MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to start a generator.")

            return false;
        }

        // The LFO a clock input feeds, when this is one that follows a clock.
        std::shared_ptr<midiapp::LfoMessageGenerator> ClockInput() const noexcept
        {
            return m_followsClock ? m_lfo : nullptr;
        }

        // What can change while it runs. Takes effect at the first message it has not scheduled.
        void Update(_In_ BlockSettings const& settings) noexcept
        {
            try
            {
                auto signature = BlockSettingsSignature(m_kind, settings);

                if (signature == m_liveSignature)
                {
                    return;
                }

                m_liveSignature = std::move(signature);

                if (m_clock != nullptr)
                {
                    m_clock->BeatsPerMinute(settings.Clock.BeatsPerMinute);
                    m_clock->SwingPercent(settings.Clock.SwingPercent);
                }

                if (m_lfo != nullptr)
                {
                    m_lfo->Options(LfoOptions(settings.Lfo, m_followsClock));
                }
            }
            MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to change a running generator.")
        }

        // Its last messages, such as a clock's stop, go through the graph it has now. Returns
        // when the last of them is meant to play, so its connections can stay open until then.
        uint64_t Stop() noexcept
        {
            uint64_t last{ 0 };

            try
            {
                if (m_clock != nullptr)
                {
                    last = (std::max)(last, m_clock->Stop());
                    m_clock.reset();
                }

                if (m_timeCode != nullptr)
                {
                    last = (std::max)(last, m_timeCode->Stop());
                    m_timeCode.reset();
                }

                if (m_lfo != nullptr)
                {
                    last = (std::max)(last, m_lfo->Stop());
                    m_lfo.reset();
                }
            }
            MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to stop a generator.")

            return last;
        }

    private:
        static midiapp::LfoMessageGeneratorOptions LfoOptions(
            _In_ LfoGeneratorSettings const& lfo,
            _In_ bool followsClock) noexcept
        {
            midiapp::LfoMessageGeneratorOptions options{};

            options.Wave = lfo.Wave;
            options.BeatsPerCycle = lfo.BeatsPerCycle;
            options.BeatsPerMinute = lfo.BeatsPerMinute;
            options.Lowest = static_cast<double>(lfo.LowestHundredths) / FullScaleHundredths;
            options.Highest = static_cast<double>(lfo.HighestHundredths) / FullScaleHundredths;
            options.IntervalMilliseconds = lfo.IntervalMilliseconds;
            options.Target = lfo.Target;
            options.ReturnsToMiddleWhenStopped = lfo.ReturnsToMiddle;
            options.FollowsClock = followsClock;
            options.KeepsToStartAndStop = followsClock && lfo.KeepsToStartAndStop;

            return options;
        }

        // On the generator's own thread, which is the only one that uses its plan's buffers.
        void Emit(
            _In_ uint64_t timestamp,
            _In_reads_(wordCount) uint32_t const* words,
            _In_ uint32_t wordCount) noexcept
        {
            auto const plan = m_plan.load();

            if (plan == nullptr || plan->Owner == nullptr || words == nullptr ||
                wordCount == 0 || wordCount > MaximumWordsPerUmp)
            {
                return;
            }

            auto const& graph = plan->Owner->Graph;

            if (plan->Index >= graph.Generators.size())
            {
                return;
            }

            auto const& generator = graph.Generators[plan->Index];
            auto& work = plan->Work;

            work.Timestamp = timestamp;
            work.CountBlock(generator.Cell, true);

            for (uint32_t i = 0; i < generator.EdgeCount; i++)
            {
                RunEdge(graph, graph.Edges[generator.FirstEdge + i], words, static_cast<uint8_t>(wordCount), work);
            }

            work.FlushAll();
        }

        BlockKind m_kind{ BlockKind::ClockGenerator };
        bool m_followsClock{ false };

        std::wstring m_restartSignature{};
        std::wstring m_liveSignature{};

        std::atomic<std::shared_ptr<GeneratorPlan>> m_plan{};

        std::unique_ptr<midiapp::BeatClockGenerator> m_clock{};
        std::unique_ptr<midiapp::TimeCodeGenerator> m_timeCode{};
        std::shared_ptr<midiapp::LfoMessageGenerator> m_lfo{};
    };

    _Use_decl_annotations_
    void RouteEngine::Context::Prepare(Runtime& owner)
    {
        Owner = &owner;

        m_buffers.resize(owner.Leaves.size());

        for (size_t i = 0; i < owner.Leaves.size(); i++)
        {
            m_buffers[i].Words.assign((std::max)(owner.Leaves[i].BufferWords, MaximumWordsPerUmp), 0);
        }

        m_touched.clear();
        m_touched.reserve(owner.Leaves.size());
    }

    _Use_decl_annotations_
    void RouteEngine::Context::CountLink(uint32_t cell) noexcept
    {
        Owner->Cells[cell].Out.fetch_add(1, std::memory_order_relaxed);
    }

    _Use_decl_annotations_
    void RouteEngine::Context::CountBlock(uint32_t cell, bool passed) noexcept
    {
        auto& counters = Owner->Cells[cell];

        counters.In.fetch_add(1, std::memory_order_relaxed);

        if (passed)
        {
            counters.Out.fetch_add(1, std::memory_order_relaxed);
        }
    }

    _Use_decl_annotations_
    void RouteEngine::Context::Send(uint32_t leafIndex, uint32_t const* words, uint8_t wordCount) noexcept
    {
        if (leafIndex >= m_buffers.size() || !Owner->Leaves[leafIndex].Usable)
        {
            return;
        }

        auto& buffer = m_buffers[leafIndex];

        if (buffer.Used + wordCount > buffer.Words.size())
        {
            Flush(leafIndex);
        }

        std::copy_n(words, wordCount, buffer.Words.data() + buffer.Used);

        buffer.Used += wordCount;
        buffer.Messages++;

        if (!buffer.Touched)
        {
            buffer.Touched = true;
            m_touched.push_back(leafIndex);
        }
    }

    _Use_decl_annotations_
    void RouteEngine::Context::Throttle(uint32_t throttle, uint32_t const* words, uint8_t wordCount) noexcept
    {
        if (throttle >= Owner->Throttles.size())
        {
            return;
        }

        auto& runner = Owner->Throttles[throttle];

        if (runner == nullptr || !runner->Push(Timestamp, words, wordCount))
        {
            Owner->Cells[Owner->Graph.Throttles[throttle].Cell].Dropped.fetch_add(1, std::memory_order_relaxed);
        }
    }

    _Use_decl_annotations_
    void RouteEngine::Context::Flush(uint32_t leafIndex) noexcept
    {
        auto& buffer = m_buffers[leafIndex];

        if (buffer.Used == 0)
        {
            return;
        }

        auto const& leaf = Owner->Leaves[leafIndex];

        if (leaf.WaitQueue != nullptr)
        {
            // Never waited for here. This thread also carries everything else this source or
            // throttle sends on.
            if (leaf.WaitQueue->Push(Timestamp, buffer.Words.data(), buffer.Used))
            {
                leaf.Sender->Wake();
            }
            else
            {
                Owner->Cells[leaf.Cell].Dropped.fetch_add(buffer.Messages, std::memory_order_relaxed);
            }
        }
        else if (FAILED(leaf.Destination->SendMidiMessagesRaw(Timestamp, buffer.Used, buffer.Words.data())))
        {
            Owner->Cells[leaf.Cell].Failures.fetch_add(1, std::memory_order_relaxed);
        }

        buffer.Used = 0;
        buffer.Messages = 0;
    }

    void RouteEngine::Context::FlushAll() noexcept
    {
        for (auto const leafIndex : m_touched)
        {
            Flush(leafIndex);
            m_buffers[leafIndex].Touched = false;
        }

        m_touched.clear();
    }

    _Use_decl_annotations_
    ::midipatchbay::BlockState* RouteEngine::Context::StateOf(uint32_t state) noexcept
    {
        if (state >= Owner->States.size())
        {
            return nullptr;
        }

        return Owner->States[state].get();
    }

    _Use_decl_annotations_
    void RouteEngine::Context::Clock(uint32_t target, uint32_t const* words, uint8_t wordCount) noexcept
    {
        if (target >= Owner->ClockTargets.size() || Owner->ClockTargets[target] == nullptr)
        {
            return;
        }

        Owner->ClockTargets[target]->ReceiveClock(Timestamp, words, wordCount);
    }

    namespace
    {
        // What a running generator can't take on the fly, including whether an LFO follows a
        // clock.
        std::wstring RestartSignatureOf(_In_ RouteGraph const& graph, _In_ RouteGenerator const& generator)
        {
            return GeneratorRestartSignature(generator.Kind, graph.Settings[generator.Settings]) +
                (generator.FollowsClock ? L"|clock" : L"");
        }

        // Until the service has played everything a stopped generator scheduled, so closing a
        // connection does not throw away a clock's stop or a time code's last full frame.
        void WaitUntilPlayed(_In_ uint64_t timestamp) noexcept
        {
            auto const frequency = internal::GetMidiTimestampFrequency();

            if (timestamp == 0 || frequency == 0)
            {
                return;
            }

            auto const now = internal::GetCurrentMidiTimestamp();
            auto const ahead = timestamp > now ? (timestamp - now) * 1000ull / frequency : 0ull;

            std::this_thread::sleep_for(std::chrono::milliseconds{
                (std::min)(ahead + DrainMarginMilliseconds, LongestDrainMilliseconds) });
        }
    }

    RouteEngine& RouteEngine::Current() noexcept
    {
        static RouteEngine instance{};
        return instance;
    }

    _Use_decl_annotations_
    std::shared_ptr<RouteEngine::Runtime> RouteEngine::Publish(std::shared_ptr<Runtime> runtime, size_t activeRoutes) noexcept
    {
        std::scoped_lock guard{ m_publishLock };

        m_activeRoutes = activeRoutes;

        return std::exchange(m_runtime, std::move(runtime));
    }

    _Use_decl_annotations_
    void RouteEngine::SetLastError(winrt::hstring const& message) noexcept
    {
        std::scoped_lock guard{ m_publishLock };

        m_lastError = message;
    }

    _Use_decl_annotations_
    void RouteEngine::CloseConnection(Connection& connection) noexcept
    {
        try
        {
            if (connection.Hub != nullptr)
            {
                connection.Hub->Plan(nullptr);

                if (connection.Raw != nullptr)
                {
                    LOG_IF_FAILED(connection.Raw->RemoveMessagesReceivedCallback());
                }
            }

            if (connection.Endpoint != nullptr && m_session != nullptr)
            {
                m_session.DisconnectEndpointConnection(connection.Endpoint.ConnectionId());
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to disconnect an endpoint connection.")
    }

    void RouteEngine::TearDownLocked() noexcept
    {
        try
        {
            // Generators first, while the graph their last messages go through is still whole.
            uint64_t lastScheduled{ 0 };

            for (auto& [key, runner] : m_generators)
            {
                UNREFERENCED_PARAMETER(key);

                if (runner != nullptr)
                {
                    lastScheduled = (std::max)(lastScheduled, runner->Stop());
                }
            }

            m_generators.clear();

            // Nothing new is walked through the graph after this.
            for (auto& [key, connection] : m_connections)
            {
                UNREFERENCED_PARAMETER(key);

                if (connection != nullptr && connection->Hub != nullptr)
                {
                    connection->Hub->Plan(nullptr);
                }
            }

            if (auto const previous = Publish(nullptr, 0))
            {
                previous->StopThreads();
            }

            WaitUntilPlayed(lastScheduled);

            for (auto& [key, connection] : m_connections)
            {
                UNREFERENCED_PARAMETER(key);

                if (connection != nullptr)
                {
                    CloseConnection(*connection);
                }
            }

            m_connections.clear();
            m_blockStates.clear();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to stop routing.")
    }

    _Use_decl_annotations_
    void RouteEngine::Apply(RouteGraph graph) noexcept
    {
        std::scoped_lock guard{ m_lock };

        try
        {
            if (graph.Signature == m_signature)
            {
                return;
            }

            m_signature = graph.Signature;
            SetLastError({});

            if (graph.Roots.empty() && graph.Generators.empty())
            {
                TearDownLocked();

                // nothing to route, so the session is dropped too rather than left holding
                // the service open for an idle app
                if (m_session != nullptr)
                {
                    m_session.Close();
                    m_session = nullptr;
                }

                return;
            }

            if (m_session == nullptr)
            {
                if (!midi2::MidiApi::EnsureServiceAvailable())
                {
                    SetLastError(resources::GetString(L"ErrorServiceUnavailable"));
                    m_signature.clear();
                    return;
                }

                m_session = midi2::MidiSession::Create(SessionName);

                if (m_session == nullptr)
                {
                    SetLastError(resources::GetString(L"ErrorSessionFailed"));
                    m_signature.clear();
                    return;
                }
            }

            // Built whole before anything uses it. The running graph carries on meanwhile.
            auto const next = std::make_shared<Runtime>();

            auto& runtime = *next;
            runtime.Graph = std::move(graph);

            auto const& routes = runtime.Graph;

            runtime.Cells = std::make_unique<Counters[]>((std::max)(routes.Cells.size(), size_t{ 1 }));
            runtime.Leaves.resize(routes.Leaves.size());

            // Each stateful step keeps its state from the graph before, when it is still there.
            {
                std::unordered_map<std::wstring, std::shared_ptr<::midipatchbay::BlockState>> states{};

                for (auto const& planned : routes.States)
                {
                    auto const& name = routes.Cells[planned.Cell];
                    auto const kept = m_blockStates.find(name);

                    std::shared_ptr<::midipatchbay::BlockState> state{};

                    // A responder's state is made before anything can reach it, never after.
                    if (kept != m_blockStates.end() &&
                        (planned.Kind != ::midipatchbay::BlockKind::CiResponder || kept->second->Ci != nullptr))
                    {
                        state = kept->second;
                    }
                    else
                    {
                        state = std::make_shared<::midipatchbay::BlockState>();
                        ::midipatchbay::PrepareBlockState(planned.Kind, *state);
                    }

                    runtime.States.push_back(state);
                    states.insert_or_assign(name, std::move(state));
                }

                m_blockStates = std::move(states);
            }

            // Pass one: work out which connections are needed and which of them are sources.
            std::set<ConnectionKey> sourceKeys{};
            std::set<ConnectionKey> allKeys{};

            for (auto const& root : routes.Roots)
            {
                ConnectionKey const key{ root.SourceDeviceId, root.WaitForSendComplete };

                sourceKeys.insert(key);
                allKeys.insert(key);
            }

            for (auto const& leaf : routes.Leaves)
            {
                allKeys.insert(ConnectionKey{ leaf.DestinationDeviceId, leaf.WaitForSendComplete });
            }

            // Pass two: create the connections that are not open yet. Every one gets its hub
            // before it opens, because the COM extensions take a callback then or never.
            //
            // One that opened without a hub, because its callback failed while it was only a
            // destination, can't become a source, so it is made again.
            for (auto const& key : sourceKeys)
            {
                auto const found = m_connections.find(key);

                if (found != m_connections.end() && found->second != nullptr && found->second->Hub == nullptr)
                {
                    CloseConnection(*found->second);
                    m_connections.erase(found);
                }
            }

            std::vector<ConnectionKey> created{};

            for (auto const& key : allKeys)
            {
                if (m_connections.count(key) != 0)
                {
                    continue;
                }

                auto const& [id, waitForSendComplete] = key;

                // Settings only where a patch waits, so every other connection opens exactly as
                // it always has
                auto endpoint = waitForSendComplete
                    ? m_session.CreateEndpointConnection(winrt::hstring{ id }, midi2::MidiEndpointConnectionSettings{ true })
                    : m_session.CreateEndpointConnection(winrt::hstring{ id });

                if (endpoint == nullptr)
                {
                    // an endpoint that went away between planning and here is not an error:
                    // its routes simply stay idle until it comes back
                    MIDI_PATCHBAY_LOG_INFO_WITH_ENDPOINT(L"Endpoint connection could not be created.", id.c_str());
                    continue;
                }

                auto connection = std::make_unique<Connection>();

                connection->Endpoint = endpoint;
                connection->Raw = endpoint.try_as<IMidiEndpointConnectionRaw>();

                if (connection->Raw == nullptr)
                {
                    SetLastError(resources::GetString(L"ErrorNoComExtensions"));
                    CloseConnection(*connection);
                    continue;
                }

                auto hub = winrt::make_self<SourceHub>();

                if (SUCCEEDED(connection->Raw->SetMessagesReceivedCallback(hub.get())))
                {
                    connection->Hub = hub;
                }
                else if (sourceKeys.count(key) != 0)
                {
                    SetLastError(resources::GetString(L"ErrorReceiveCallback"));
                    CloseConnection(*connection);
                    continue;
                }

                m_connections.emplace(key, std::move(connection));
                created.push_back(key);
            }

            // Pass three: every link into a destination, now that every connection exists.
            auto const ticksPerSecond = internal::GetMidiTimestampFrequency();

            for (size_t i = 0; i < routes.Leaves.size(); i++)
            {
                auto const& planned = routes.Leaves[i];
                auto& leaf = runtime.Leaves[i];

                leaf.Cell = planned.LinkCell;

                auto const found = m_connections.find(ConnectionKey{ planned.DestinationDeviceId, planned.WaitForSendComplete });

                if (found == m_connections.end() || found->second == nullptr)
                {
                    continue;
                }

                auto const& raw = found->second->Raw;

                // A buffer shorter than the longest UMP could not hold even one message.
                auto const maxWords = raw->GetSupportedMaxMidiWordsPerTransmission();

                if (maxWords < MaximumWordsPerUmp)
                {
                    continue;
                }

                leaf.Destination = raw;
                leaf.BufferWords = (std::min)(maxWords, MaximumSendBufferWords);

                if (planned.WaitForSendComplete)
                {
                    leaf.WaitQueue = std::make_unique<SendQueue>();
                    leaf.WaitQueue->Configure(0, ticksPerSecond);
                }

                leaf.Usable = true;
            }

            // Pass four: a send thread for each destination in a patch that waits. A link whose
            // thread cannot start is left out, rather than left to fill a queue nothing empties.
            std::map<ConnectionKey, DestinationSender*> sendersByKey{};

            for (size_t i = 0; i < routes.Leaves.size(); i++)
            {
                auto& leaf = runtime.Leaves[i];

                if (!leaf.Usable || leaf.WaitQueue == nullptr)
                {
                    continue;
                }

                auto& sender = sendersByKey[ConnectionKey{ routes.Leaves[i].DestinationDeviceId, true }];

                if (sender == nullptr)
                {
                    runtime.Senders.push_back(std::make_unique<DestinationSender>());

                    sender = runtime.Senders.back().get();
                    sender->Destination = leaf.Destination;
                    sender->Cells = runtime.Cells.get();
                }

                sender->Leaves.push_back(&leaf);
                leaf.Sender = sender;
            }

            for (auto& sender : runtime.Senders)
            {
                if (!sender->Start())
                {
                    SetLastError(resources::GetString(L"ErrorRoutingFailed"));
                }
            }

            for (auto& leaf : runtime.Leaves)
            {
                if (leaf.Sender != nullptr && !leaf.Sender->IsRunning())
                {
                    leaf.Usable = false;
                }
            }

            // Pass five: a thread for each throttle. All of them exist before any starts, because
            // what comes after one throttle can lead into another.
            for (size_t i = 0; i < routes.Throttles.size(); i++)
            {
                auto runner = std::make_unique<ThrottleRunner>();

                runner->Owner = &runtime;
                runner->Index = static_cast<uint32_t>(i);
                runner->Cell = routes.Throttles[i].Cell;
                runner->Queue.Configure(routes.Throttles[i].Speed, ticksPerSecond);
                runner->Work.Prepare(runtime);

                runtime.Throttles.push_back(std::move(runner));
            }

            for (auto& runner : runtime.Throttles)
            {
                if (!runner->Start())
                {
                    SetLastError(resources::GetString(L"ErrorRoutingFailed"));
                }
            }

            // Pass six: what each source does with what arrives from it.
            size_t activeRoutes{ 0 };
            std::map<ConnectionKey, std::shared_ptr<HubPlan>> plans{};

            for (size_t i = 0; i < routes.Roots.size(); i++)
            {
                auto const& root = routes.Roots[i];
                ConnectionKey const key{ root.SourceDeviceId, root.WaitForSendComplete };
                auto const connection = m_connections.find(key);

                if (connection == m_connections.end() || connection->second == nullptr || connection->second->Hub == nullptr)
                {
                    continue;
                }

                auto& plan = plans[key];

                if (plan == nullptr)
                {
                    plan = std::make_shared<HubPlan>();
                    plan->Owner = next;
                    plan->Work.Prepare(runtime);
                }

                plan->Roots.push_back(static_cast<uint32_t>(i));
                activeRoutes++;
            }

            // Pass seven: generators that stop, because their patch no longer routes them or a
            // setting they cannot take on the fly changed. Their last messages, such as a clock's
            // stop, go through the graph that is still running.
            uint64_t lastScheduled{ 0 };
            std::unordered_map<std::wstring, size_t> plannedGenerators{};

            for (size_t i = 0; i < routes.Generators.size(); i++)
            {
                plannedGenerators.emplace(routes.Generators[i].Key, i);
            }

            for (auto it = m_generators.begin(); it != m_generators.end();)
            {
                auto const planned = plannedGenerators.find(it->first);
                auto keeps = false;

                if (planned != plannedGenerators.end() && it->second != nullptr)
                {
                    keeps = it->second->RestartSignature() ==
                        RestartSignatureOf(routes, routes.Generators[planned->second]);
                }

                if (keeps)
                {
                    ++it;
                    continue;
                }

                if (it->second != nullptr)
                {
                    lastScheduled = (std::max)(lastScheduled, it->second->Stop());
                }

                it = m_generators.erase(it);
            }

            // Pass eight: every generator that isn't running is built, not started, and each
            // clock input finds the LFO it feeds, all before anything can reach the new graph.
            std::vector<bool> starting(routes.Generators.size(), false);

            for (size_t i = 0; i < routes.Generators.size(); i++)
            {
                auto const& planned = routes.Generators[i];

                if (m_generators.count(planned.Key) != 0)
                {
                    continue;
                }

                auto runner = std::make_unique<GeneratorRunner>(planned.Kind, RestartSignatureOf(routes, planned));

                if (!runner->Create(routes.Settings[planned.Settings], planned.FollowsClock))
                {
                    SetLastError(resources::GetString(L"ErrorRoutingFailed"));
                    continue;
                }

                m_generators.insert_or_assign(planned.Key, std::move(runner));
                starting[i] = true;
            }

            runtime.ClockTargets.resize(routes.ClockTargets.size());

            for (size_t i = 0; i < routes.ClockTargets.size(); i++)
            {
                auto const found = m_generators.find(routes.ClockTargets[i]);

                if (found != m_generators.end() && found->second != nullptr)
                {
                    runtime.ClockTargets[i] = found->second->ClockInput();
                }
            }

            // Pass nine: open what pass two created. Its hub has its plan first, so nothing that
            // arrives meets a hub that does not know its links yet. One that cannot open is let
            // go, so the next change tries it again.
            for (auto const& key : created)
            {
                auto const found = m_connections.find(key);

                if (found == m_connections.end() || found->second == nullptr)
                {
                    continue;
                }

                auto& connection = *found->second;
                auto const plan = plans.find(key);

                if (plan != plans.end() && connection.Hub != nullptr)
                {
                    connection.Hub->Plan(plan->second);
                }

                if (connection.Endpoint.Open())
                {
                    connection.Opened = true;
                    continue;
                }

                MIDI_PATCHBAY_LOG_INFO_WITH_ENDPOINT(L"Endpoint connection could not be opened.", key.first.c_str());

                for (size_t i = 0; i < routes.Leaves.size(); i++)
                {
                    if (ConnectionKey{ routes.Leaves[i].DestinationDeviceId, routes.Leaves[i].WaitForSendComplete } == key)
                    {
                        runtime.Leaves[i].Usable = false;
                    }
                }

                CloseConnection(connection);
                m_connections.erase(found);
            }

            // Pass ten: generators still running carry on into the new graph, and new ones start
            // together.
            auto const origin = midi2::MidiClock::Now() + midiapp::BeatClockGenerator::SuggestedStartLeadTicks();

            for (size_t i = 0; i < routes.Generators.size(); i++)
            {
                auto const& planned = routes.Generators[i];
                auto const running = m_generators.find(planned.Key);

                if (running == m_generators.end() || running->second == nullptr)
                {
                    continue;
                }

                auto plan = std::make_shared<GeneratorPlan>();
                plan->Owner = next;
                plan->Index = static_cast<uint32_t>(i);
                plan->Work.Prepare(runtime);

                running->second->Plan(std::move(plan));

                if (!starting[i])
                {
                    running->second->Update(routes.Settings[planned.Settings]);
                }
                else if (!running->second->Start(origin))
                {
                    SetLastError(resources::GetString(L"ErrorRoutingFailed"));
                    m_generators.erase(running);
                    continue;
                }

                activeRoutes++;
            }

            // Pass eleven: the switch. Every open endpoint moves to its new plan, or to none.
            for (auto& [key, connection] : m_connections)
            {
                if (connection == nullptr || connection->Hub == nullptr)
                {
                    continue;
                }

                auto const plan = plans.find(key);

                connection->Hub->Plan(plan == plans.end() ? nullptr : plan->second);
            }

            // The graph this replaces stops once nothing new can reach it.
            if (auto const previous = Publish(next, activeRoutes))
            {
                previous->StopThreads();
            }

            // Connections nothing wants any more close last, once anything a stopped generator
            // scheduled through them has played.
            std::vector<ConnectionKey> unwanted{};

            for (auto const& [key, connection] : m_connections)
            {
                UNREFERENCED_PARAMETER(connection);

                if (allKeys.count(key) == 0)
                {
                    unwanted.push_back(key);
                }
            }

            if (!unwanted.empty())
            {
                WaitUntilPlayed(lastScheduled);

                for (auto const& key : unwanted)
                {
                    auto const found = m_connections.find(key);

                    if (found == m_connections.end())
                    {
                        continue;
                    }

                    if (found->second != nullptr)
                    {
                        CloseConnection(*found->second);
                    }

                    m_connections.erase(found);
                }
            }
        }
        catch (winrt::hresult_error const& ex)
        {
            MIDI_PATCHBAY_LOG_HRESULT_EXCEPTION(ex, L"Unable to apply the routing plan.");
            TearDownLocked();
            SetLastError(resources::GetString(L"ErrorRoutingFailed"));
            m_signature.clear();
        }
        catch (...)
        {
            MIDI_PATCHBAY_LOG_GENERAL_EXCEPTION(L"Unable to apply the routing plan.");
            TearDownLocked();
            SetLastError(resources::GetString(L"ErrorRoutingFailed"));
            m_signature.clear();
        }
    }

    void RouteEngine::Shutdown() noexcept
    {
        std::scoped_lock guard{ m_lock };

        try
        {
            TearDownLocked();

            if (m_session != nullptr)
            {
                m_session.Close();
                m_session = nullptr;
            }

            m_signature.clear();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to shut down the routing engine.")
    }

    RouteEngine::~RouteEngine() noexcept
    {
        // The window shuts the engine down on a thread of its own, which can still be running
        // when the process exits. This waits for it, then stops anything it left.
        Shutdown();
    }

    std::unordered_map<std::wstring, RouteStats> RouteEngine::Stats() const noexcept
    {
        std::unordered_map<std::wstring, RouteStats> result{};

        // Held only long enough to take the graph, so reading counts never waits on Apply.
        std::shared_ptr<Runtime> runtime{};

        {
            std::scoped_lock guard{ m_publishLock };
            runtime = m_runtime;
        }

        if (runtime == nullptr)
        {
            return result;
        }

        try
        {
            auto const& routes = runtime->Graph;

            for (size_t i = 0; i < routes.Cells.size(); i++)
            {
                auto const& counters = runtime->Cells[i];

                auto const in = counters.In.load(std::memory_order_relaxed);
                auto const out = counters.Out.load(std::memory_order_relaxed);

                RouteStats stats{};

                stats.MessagesForwarded = out;
                stats.MessagesKeptOut = in > out ? in - out : 0;
                stats.SendFailures = counters.Failures.load(std::memory_order_relaxed);
                stats.MessagesDropped = counters.Dropped.load(std::memory_order_relaxed);
                stats.IsActive = true;

                result[routes.Cells[i]] = stats;
            }

            for (auto const& leaf : runtime->Leaves)
            {
                if (leaf.WaitQueue != nullptr && leaf.Cell < routes.Cells.size())
                {
                    result[routes.Cells[leaf.Cell]].MessagesWaiting += leaf.WaitQueue->WaitingMessageCount();
                }
            }

            for (auto const& runner : runtime->Throttles)
            {
                if (runner != nullptr && runner->Cell < routes.Cells.size())
                {
                    result[routes.Cells[runner->Cell]].MessagesWaiting += runner->Queue.WaitingMessageCount();
                }
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to read the routing counts.")

        return result;
    }

    winrt::hstring RouteEngine::LastErrorMessage() const noexcept
    {
        std::scoped_lock guard{ m_publishLock };
        return m_lastError;
    }

    size_t RouteEngine::ActiveRouteCount() const noexcept
    {
        std::scoped_lock guard{ m_publishLock };
        return m_activeRoutes;
    }

    _Use_decl_annotations_
    std::optional<::midipatchbay::CiResponderSnapshot> RouteEngine::CiResponderStatus(std::wstring const& cell) const noexcept
    {
        std::shared_ptr<Runtime> runtime{};

        {
            std::scoped_lock guard{ m_publishLock };
            runtime = m_runtime;
        }

        if (runtime == nullptr)
        {
            return std::nullopt;
        }

        try
        {
            auto const& routes = runtime->Graph;

            for (size_t i = 0; i < routes.States.size() && i < runtime->States.size(); i++)
            {
                auto const& planned = routes.States[i];
                auto const& state = runtime->States[i];

                if (planned.Kind == ::midipatchbay::BlockKind::CiResponder &&
                    planned.Cell < routes.Cells.size() && routes.Cells[planned.Cell] == cell &&
                    state != nullptr && state->Ci != nullptr)
                {
                    return state->Ci->Snapshot();
                }
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to read what a MIDI-CI step has been doing.")

        return std::nullopt;
    }

    _Use_decl_annotations_
    bool RouteEngine::SendTestNote(
        std::wstring const& endpointDeviceId,
        int32_t groupIndex,
        uint8_t noteIndex) noexcept
    {
        try
        {
            if (endpointDeviceId.empty())
            {
                return false;
            }

            auto const group = static_cast<uint32_t>(groupIndex == AllGroups ? 0 : std::clamp(groupIndex, 0, 15));
            auto const note = static_cast<uint32_t>(noteIndex & 0x7F);

            // MIDI 1.0 channel voice on channel 1: the one shape every endpoint understands,
            // whether or not it is natively UMP.
            auto const noteOn = (2u << 28) | (group << 24) | (0x9u << 20) | (note << 8) | 100u;
            auto const noteOff = (2u << 28) | (group << 24) | (0x8u << 20) | (note << 8) | 0u;

            midi2::MidiSession temporarySession{ nullptr };
            midi2::MidiEndpointConnection temporaryConnection{ nullptr };
            winrt::com_ptr<IMidiEndpointConnectionRaw> raw{ nullptr };

            {
                std::scoped_lock guard{ m_lock };

                auto const id = LowerCopy(endpointDeviceId);

                for (auto const waitsForSendComplete : { false, true })
                {
                    auto const it = m_connections.find(ConnectionKey{ id, waitsForSendComplete });

                    if (it != m_connections.end() && it->second != nullptr && it->second->Opened)
                    {
                        raw = it->second->Raw;
                        break;
                    }
                }
            }

            if (raw == nullptr)
            {
                if (!midi2::MidiApi::EnsureServiceAvailable())
                {
                    return false;
                }

                temporarySession = midi2::MidiSession::Create(SessionName);

                if (temporarySession == nullptr)
                {
                    return false;
                }

                temporaryConnection = temporarySession.CreateEndpointConnection(winrt::hstring{ endpointDeviceId });

                if (temporaryConnection == nullptr || !temporaryConnection.Open())
                {
                    temporarySession.Close();
                    return false;
                }

                raw = temporaryConnection.try_as<IMidiEndpointConnectionRaw>();
            }

            auto const closeTemporary = wil::scope_exit([&temporarySession]()
                {
                    if (temporarySession != nullptr)
                    {
                        temporarySession.Close();
                    }
                });

            if (raw == nullptr)
            {
                return false;
            }

            auto sent = SUCCEEDED(raw->SendMidiMessagesRaw(0, 1, &noteOn));

            std::this_thread::sleep_for(std::chrono::milliseconds{ TestNoteMilliseconds });

            sent = SUCCEEDED(raw->SendMidiMessagesRaw(0, 1, &noteOff)) && sent;

            return sent;
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to play the test note.")

        return false;
    }
}
