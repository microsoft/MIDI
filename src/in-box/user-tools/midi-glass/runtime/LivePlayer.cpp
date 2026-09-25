// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "LivePlayer.h"

#include <ump_helpers.h>

namespace internal = ::WindowsMidiServicesInternal;

namespace glass
{
    namespace
    {
        uint64_t NowMilliseconds() noexcept
        {
            return static_cast<uint64_t>(::GetTickCount64());
        }
    }

    std::shared_ptr<LivePlayer> LivePlayer::Create()
    {
        // The constructor is private so nothing can put one of these on the stack: the detached
        // threads below hold a weak_ptr and there has to be a shared_ptr for them to hold it to.
        return std::shared_ptr<LivePlayer>(new LivePlayer());
    }

    LivePlayer::~LivePlayer() noexcept
    {
        m_stopping = true;

        if (m_runner != nullptr)
        {
            m_runner->Stop();
        }
    }

    _Use_decl_annotations_
    void LivePlayer::Start(
        LayoutDocument const& document,
        winrt::Microsoft::UI::Dispatching::DispatcherQueue const& dispatcher,
        std::wstring const& ownerId)
    {
        if (m_started)
        {
            return;
        }

        m_started = true;
        m_document = document;
        m_dispatcher = dispatcher;
        m_ownerId = ownerId;

        RebuildThrottles();

        std::weak_ptr<LivePlayer> weak{ shared_from_this() };

        m_runner = SequenceRunner::Create();

        m_runner->Send = [weak](uint32_t controlIndex, int32_t destinationIndex, uint32_t const* words, uint32_t wordCount)
            {
                if (auto strong = weak.lock())
                {
                    strong->SendWords(controlIndex, destinationIndex, words, wordCount);
                }
            };

        m_runner->SetControlValue = [weak](uint32_t controlIndex, double value)
            {
                auto strong = weak.lock();

                if (strong != nullptr && strong->ControlValueSet)
                {
                    strong->ControlValueSet(controlIndex, value);
                }
            };

        m_runner->GoToPage = [weak](uint32_t pageIndex)
            {
                auto strong = weak.lock();

                if (strong != nullptr && strong->PageRequested)
                {
                    strong->PageRequested(pageIndex);
                }
            };

        m_runner->Start(dispatcher);

        m_devices.SetChangedHandler([weak]()
            {
                auto strong = weak.lock();

                if (strong == nullptr || strong->m_dispatcher == nullptr)
                {
                    return;
                }

                // The watcher calls on its own thread. Nothing below the window layer calls up
                // into the UI, so the marshalling happens here.
                strong->m_dispatcher.TryEnqueue([weak]()
                    {
                        if (auto inner = weak.lock())
                        {
                            inner->ReopenConnections();
                        }
                    });
            });

        m_devices.SetDocument(m_document);

        // Starting the watcher blocks on the service, so it never runs on the UI thread. An
        // exception escaping a thread body calls terminate, so nothing leaves any of these.
        std::thread([weak]()
            {
                winrt::init_apartment(winrt::apartment_type::multi_threaded);

                try
                {
                    if (auto strong = weak.lock())
                    {
                        strong->m_devices.Start();
                    }
                }
                catch (...)
                {
                }

                winrt::uninit_apartment();
            }).detach();

        ReopenConnections();
    }

    _Use_decl_annotations_
    void LivePlayer::UpdateDocument(LayoutDocument const& document)
    {
        if (!m_started || m_stopping)
        {
            return;
        }

        m_document = document;

        RebuildThrottles();

        m_devices.SetDocument(m_document);

        // An edit can change what a control sends without changing which devices the layout
        // wants, and the signature check below would skip it. Forcing it keeps Try mode honest.
        m_destinationSignature.clear();

        ReopenConnections();
    }

    void LivePlayer::Stop()
    {
        if (!m_started || m_stopping)
        {
            return;
        }

        m_stopping = true;

        m_sendTable.clear();

        if (m_runner != nullptr)
        {
            m_runner->Stop();
        }

        auto const ownerId = m_ownerId;

        std::thread([ownerId]()
            {
                winrt::init_apartment(winrt::apartment_type::multi_threaded);

                try
                {
                    glass::OutputRouter::Current().Close(ownerId);
                }
                catch (...)
                {
                }

                winrt::uninit_apartment();
            }).detach();
    }

    void LivePlayer::RebuildThrottles()
    {
        m_throttles.assign(m_document.ControlCount(), ValueThrottle{});

        size_t index{ 0 };

        for (auto const& page : m_document.Pages)
        {
            for (auto const& control : page.Controls)
            {
                if (index < m_throttles.size())
                {
                    m_throttles[index].SetMinimumInterval(
                        static_cast<uint32_t>(std::max(0, control.SendIntervalMilliseconds)));
                }

                index++;
            }
        }
    }

    void LivePlayer::ReopenConnections()
    {
        if (m_stopping)
        {
            return;
        }

        // The watcher fires once per endpoint on the machine at startup, and again whenever
        // anything at all is plugged in. Rebuilding connections for a device this layout does
        // not use would interrupt what it is playing, so the work is skipped unless what this
        // layout resolved to actually changed.
        std::wstring signature{};

        for (auto const& device : m_devices.Devices())
        {
            signature += device.Name;
            signature += L'\x1';
            signature += device.EndpointDeviceId;
            signature += L'\x1';
            signature += device.IsAvailable ? L'1' : L'0';
            signature += L'\n';
        }

        if (signature == m_destinationSignature && !m_sendTable.empty())
        {
            return;
        }

        m_destinationSignature = signature;

        // Every device name resolves to an index here, once, so nothing on the path a finger
        // takes ever compares a string.
        m_engine.Prepare(m_document, m_devices.BuildDestinations());
        m_plans.Prepare(m_document, m_engine.Destinations());

        if (DevicesChanged)
        {
            DevicesChanged();
        }

        std::vector<std::wstring> endpointIds{};
        std::vector<uint16_t> groupMasks{};

        m_devices.BuildOutputRequests(endpointIds, groupMasks);

        std::vector<OutputRequest> requests{};
        requests.reserve(endpointIds.size());

        for (size_t i = 0; i < endpointIds.size(); ++i)
        {
            requests.push_back({ endpointIds[i], i < groupMasks.size() ? groupMasks[i] : uint16_t{ 0 } });
        }

        std::weak_ptr<LivePlayer> weak{ shared_from_this() };
        auto const ownerId = m_ownerId;

        std::thread([weak, ownerId, requests]()
            {
                winrt::init_apartment(winrt::apartment_type::multi_threaded);

                try
                {
                    std::vector<winrt::com_ptr<IMidiEndpointConnectionRaw>> table{};

                    FeedbackHandler handler =
                        [weak](std::wstring const& endpointDeviceId, uint64_t, uint32_t wordCount, uint32_t const* words)
                        {
                            if (auto strong = weak.lock())
                            {
                                strong->OnFeedbackWords(endpointDeviceId, wordCount, words);
                            }
                        };

                    OutputRouter::Current().Open(ownerId, requests, handler, table);

                    auto strong = weak.lock();

                    if (strong != nullptr && strong->m_dispatcher != nullptr)
                    {
                        // The send table is only ever read on the UI thread, which is why the hot
                        // path needs no lock. Swapping it whole is what keeps that true.
                        strong->m_dispatcher.TryEnqueue([weak, table]()
                            {
                                auto inner = weak.lock();

                                if (inner == nullptr || inner->m_stopping)
                                {
                                    return;
                                }

                                inner->m_sendTable = table;

                                if (inner->DevicesChanged)
                                {
                                    inner->DevicesChanged();
                                }

                                inner->SendStartupValues();
                            });
                    }
                }
                catch (...)
                {
                }

                winrt::uninit_apartment();
            }).detach();
    }

    _Use_decl_annotations_
    void LivePlayer::SendPrepared(uint32_t controlIndex, uint32_t count) noexcept
    {
        if (!m_outputEnabled)
        {
            return;
        }

        for (uint32_t i = 0; i < count; ++i)
        {
            auto const& send = m_sends[i];

            if (send.DestinationIndex < 0 ||
                static_cast<size_t>(send.DestinationIndex) >= m_sendTable.size())
            {
                continue;
            }

            auto const& connection = m_sendTable[static_cast<size_t>(send.DestinationIndex)];

            if (connection == nullptr || send.WordCount == 0)
            {
                continue;
            }

            // Zero is now. Measured at a third of a microsecond from the UI thread, which is why
            // this is done in the pointer handler rather than queued to a render tick.
            connection->SendMidiMessagesRaw(0, send.WordCount, send.Words);

            if (Sent)
            {
                SentMessage message{};

                message.TimestampMilliseconds = NowMilliseconds();
                message.ControlIndex = controlIndex;
                message.DestinationIndex = send.DestinationIndex;
                message.WordCount = send.WordCount;

                for (uint32_t word = 0; word < send.WordCount && word < 4; ++word)
                {
                    message.Words[word] = send.Words[word];
                }

                Sent(message);
            }
        }
    }

    _Use_decl_annotations_
    void LivePlayer::ValueChanged(uint32_t controlIndex, double value, bool isFinal)
    {
        if (controlIndex >= m_throttles.size())
        {
            return;
        }

        auto& throttle = m_throttles[controlIndex];

        if (isFinal)
        {
            // The last value is always sent. Without it a fader settles a few units from where
            // the finger left it and the surface and the desk disagree for the rest of the set.
            double pending{ 0.0 };

            if (!throttle.Release(pending))
            {
                return;
            }

            value = pending;
        }
        else if (!throttle.ShouldSend(value, NowMilliseconds()))
        {
            return;
        }

        SendPrepared(
            controlIndex,
            m_engine.Evaluate(controlIndex, MessageTrigger::Changes, value, m_sends));
    }

    _Use_decl_annotations_
    void LivePlayer::SetDirectly(uint32_t controlIndex, double value)
    {
        ValueChanged(controlIndex, value, false);
        ValueChanged(controlIndex, value, true);
    }

    _Use_decl_annotations_
    void LivePlayer::Switched(uint32_t controlIndex, bool isOn)
    {
        // Nothing but a continuous control is ever throttled. Rate limiting a note on would be a
        // defect, not a feature.
        SendPrepared(
            controlIndex,
            m_engine.Evaluate(
                controlIndex,
                isOn ? MessageTrigger::TurnsOn : MessageTrigger::TurnsOff,
                isOn ? 1.0 : 0.0,
                m_sends));

        // A control that only declares "changes" still has to do something when a pad is hit.
        SendPrepared(
            controlIndex,
            m_engine.Evaluate(controlIndex, MessageTrigger::Changes, isOn ? 1.0 : 0.0, m_sends));

        RunPlan(controlIndex, isOn ? MessageTrigger::TurnsOn : MessageTrigger::TurnsOff);
    }

    _Use_decl_annotations_
    void LivePlayer::Touched(uint32_t controlIndex, bool isTouched)
    {
        SendPrepared(
            controlIndex,
            m_engine.Evaluate(
                controlIndex,
                isTouched ? MessageTrigger::Touched : MessageTrigger::Released,
                isTouched ? 1.0 : 0.0,
                m_sends));

        RunPlan(controlIndex, isTouched ? MessageTrigger::Touched : MessageTrigger::Released);
    }

    _Use_decl_annotations_
    void LivePlayer::RunPlan(uint32_t controlIndex, MessageTrigger trigger) noexcept
    {
        if (!m_outputEnabled || m_runner == nullptr)
        {
            return;
        }

        // Lifting the finger stops anything that was running while it was held.
        if (trigger == MessageTrigger::TurnsOff || trigger == MessageTrigger::Released)
        {
            auto const* const held = m_plans.Find(
                controlIndex,
                trigger == MessageTrigger::TurnsOff ? MessageTrigger::TurnsOn : MessageTrigger::Touched);

            if (held != nullptr && held->StopsOnRelease)
            {
                m_runner->CancelFor(controlIndex);
            }
        }

        auto const* const plan = m_plans.Find(controlIndex, trigger);

        if (plan == nullptr)
        {
            return;
        }

        // A plan that runs until the next press means exactly that: the second press stops it
        // rather than starting a second copy.
        if (plan->Loops && !plan->StopsOnRelease && m_runner->IsRunning(controlIndex))
        {
            m_runner->CancelFor(controlIndex);
            return;
        }

        m_runner->Run(controlIndex, *plan);
    }

    _Use_decl_annotations_
    void LivePlayer::SendWords(
        uint32_t controlIndex,
        int32_t destinationIndex,
        uint32_t const* words,
        uint32_t wordCount) noexcept
    {
        if (!m_outputEnabled || words == nullptr || wordCount == 0)
        {
            return;
        }

        if (destinationIndex < 0 || static_cast<size_t>(destinationIndex) >= m_sendTable.size())
        {
            return;
        }

        auto const& connection = m_sendTable[static_cast<size_t>(destinationIndex)];

        if (connection == nullptr)
        {
            return;
        }

        try
        {
            // A dump is thousands of packets and a connection has its own limit on how many
            // words one call may carry, so it goes out in whatever bites that connection
            // accepts rather than in one call that would be rejected whole.
            auto const limit = std::max<uint32_t>(
                2, OutputRouter::Current().MaximumWordsPerSend(connection));

            uint32_t offset{ 0 };

            while (offset < wordCount)
            {
                auto const remaining = wordCount - offset;
                auto bite = std::min(limit, remaining);

                // Never split a 64 bit message down the middle: half of a system exclusive
                // packet is a malformed packet, not a smaller one.
                if (bite < remaining && (bite % 2) != 0)
                {
                    --bite;
                }

                if (bite == 0)
                {
                    break;
                }

                connection->SendMidiMessagesRaw(0, bite, const_cast<uint32_t*>(words) + offset);

                if (Sent)
                {
                    SentMessage message{};

                    message.TimestampMilliseconds = NowMilliseconds();
                    message.ControlIndex = controlIndex;
                    message.DestinationIndex = destinationIndex;
                    message.WordCount = std::min<uint32_t>(bite, 4);

                    for (uint32_t word = 0; word < message.WordCount; ++word)
                    {
                        message.Words[word] = words[offset + word];
                    }

                    Sent(message);
                }

                offset += bite;
            }
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    double LivePlayer::SnapToDetent(uint32_t controlIndex, double position) const
    {
        return m_engine.SnapToDetent(controlIndex, position);
    }

    _Use_decl_annotations_
    std::wstring LivePlayer::DescribeValue(uint32_t controlIndex, double position) const
    {
        uint32_t value{ 0 };
        bool isAbsolute{ false };

        if (m_engine.TryDescribeValue(controlIndex, position, value, isAbsolute) && isAbsolute)
        {
            return std::to_wstring(value);
        }

        return std::to_wstring(static_cast<int32_t>(std::lround(position * 100.0))) + L" %";
    }

    void LivePlayer::SendStartupValues()
    {
        // A layout initializes once per run. Replaying it because an unrelated device was
        // plugged in would push a whole desk back to its opening positions mid set.
        if (!m_outputEnabled || m_document.SuppressAllStartupValues || m_startupValuesSent)
        {
            return;
        }

        // In keyboard order, which is the order the person building the layout could see and fix,
        // rather than the order the controls happen to sit in the file.
        std::array<PreparedSend, 64> sends{};

        auto const written = m_engine.EvaluateStartupValues(sends);

        uint32_t sent{ 0 };

        for (uint32_t i = 0; i < written; ++i)
        {
            auto const& send = sends[i];

            if (send.DestinationIndex < 0 ||
                static_cast<size_t>(send.DestinationIndex) >= m_sendTable.size())
            {
                continue;
            }

            auto const& connection = m_sendTable[static_cast<size_t>(send.DestinationIndex)];

            if (connection != nullptr && send.WordCount != 0)
            {
                connection->SendMidiMessagesRaw(0, send.WordCount, send.Words);
                sent++;
            }
        }

        // Marked only once something actually went out. The devices resolve on a watcher thread
        // a moment after the window opens, so the first pass through here usually has nothing
        // connected yet, and claiming the layout had been initialized then would mean it never
        // was.
        m_startupValuesSent = sent > 0;
    }

    void LivePlayer::Panic()
    {
        // Everything this process is driving, not just this player. Blocking, so it leaves the
        // UI thread rather than making the button feel stuck.
        std::thread([]()
            {
                winrt::init_apartment(winrt::apartment_type::multi_threaded);

                try
                {
                    OutputRouter::Current().Panic();
                }
                catch (...)
                {
                }

                winrt::uninit_apartment();
            }).detach();
    }

    _Use_decl_annotations_
    void LivePlayer::OnFeedbackWords(
        std::wstring const& endpointDeviceId,
        uint32_t wordCount,
        uint32_t const* words)
    {
        if (words == nullptr || wordCount == 0 || m_dispatcher == nullptr)
        {
            return;
        }

        auto const learning = m_learning.load();

        if (!learning && !FeedbackMoved)
        {
            return;
        }

        // This is a service callback thread and the buffer belongs to the caller, so what is
        // wanted is resolved here and only the answer is marshalled.
        std::vector<std::pair<uint32_t, double>> moves{};
        std::vector<LearnedBinding> captures{};

        uint32_t position{ 0 };

        while (position < wordCount)
        {
            auto const length = internal::GetUmpLengthInMidiWordsFromFirstWord(words[position]);

            if (length == 0 || position + length > wordCount)
            {
                break;
            }

            size_t controlIndex{ 0 };
            double value{ 0.0 };

            if (FeedbackMoved && m_engine.TryResolveFeedback(words + position, length, controlIndex, value))
            {
                moves.emplace_back(static_cast<uint32_t>(controlIndex), value);
            }

            if (learning)
            {
                LearnedBinding learned{};

                if (TryLearnFromWords(words + position, length, learned) && IsWorthLearning(learned))
                {
                    learned.DeviceName = m_devices.NameForEndpoint(endpointDeviceId);

                    captures.push_back(std::move(learned));
                }
            }

            position += length;
        }

        if (moves.empty() && captures.empty())
        {
            return;
        }

        std::weak_ptr<LivePlayer> weak{ weak_from_this() };

        m_dispatcher.TryEnqueue([weak, moves, captures]()
            {
                auto strong = weak.lock();

                if (strong == nullptr || strong->m_stopping)
                {
                    return;
                }

                if (strong->FeedbackMoved)
                {
                    for (auto const& [controlIndex, value] : moves)
                    {
                        strong->FeedbackMoved(controlIndex, value);
                    }
                }

                // One capture per arrival. A knob swept across its travel sends a hundred
                // messages and only the first of them should arm anything.
                if (strong->Learned && strong->m_learning.load() && !captures.empty())
                {
                    strong->Learned(captures.front());
                }
            });
    }
}
