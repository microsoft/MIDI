// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "LivePlayer.h"
#include "PadGrid.h"

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

        m_clocks = ClockGenerator::Create();

        m_clocks->SendRealTime = [weak](uint32_t controlIndex, uint8_t status)
            {
                auto strong = weak.lock();

                if (strong == nullptr)
                {
                    return;
                }

                strong->SendPrepared(
                    controlIndex,
                    strong->m_engine.EvaluateSystemRealTime(controlIndex, status, strong->m_sends));
            };

        m_clocks->BeatMoved = [weak](uint32_t controlIndex, int32_t beatInBar, double phase, bool running)
            {
                auto strong = weak.lock();

                if (strong == nullptr)
                {
                    return;
                }

                if (strong->BeatMoved)
                {
                    strong->BeatMoved(controlIndex, beatInBar, phase, running);
                }

                // The tempo rides along with the beat, so a clock taking its tempo from a knob
                // shows the number changing under the hand rather than only at the ends.
                if (strong->TempoChanged && (phase < 0.0001 || !running))
                {
                    strong->TempoChanged(
                        controlIndex,
                        running && strong->m_clocks != nullptr
                            ? strong->m_clocks->TempoOf(controlIndex)
                            : 0.0);
                }

                // Everything on the layout that said it follows this clock. A beat light next
                // to a clock generator is the ordinary reason somebody adds one.
                strong->PulseTempoFollowers(controlIndex, phase, running);
            };

        m_clocks->Start(dispatcher);

        m_lfos = LfoGenerator::Create();

        m_lfos->ValueMoved = [weak](uint32_t controlIndex, double value, double phase, bool running)
            {
                auto strong = weak.lock();

                if (strong == nullptr)
                {
                    return;
                }

                // Straight out, not through the throttle. The sweep's own update rate is
                // already the limit, and a second one would eat samples the customer asked for.
                strong->SendPrepared(
                    controlIndex,
                    strong->m_engine.Evaluate(
                        controlIndex, MessageTrigger::Changes, value, strong->m_sends));

                if (strong->LfoMoved)
                {
                    strong->LfoMoved(controlIndex, value, phase, running);
                }
            };

        m_lfos->Start(dispatcher);

        m_steps = StepSequencer::Create();

        m_steps->StepChanged = [weak](uint32_t controlIndex, uint64_t run, int32_t stepIndex, bool starts)
            {
                if (auto strong = weak.lock())
                {
                    strong->OnStepChanged(controlIndex, run, stepIndex, starts);
                }
            };

        m_steps->Start(dispatcher);

        m_devices.SetChangedHandler([weak, queue = m_dispatcher]()
            {
                if (queue == nullptr)
                {
                    return;
                }

                // The watcher calls on its own thread. Nothing below the window layer calls up
                // into the UI, so the marshalling happens here. The player is only resolved on
                // the UI thread, because releasing the last reference here would destroy it
                // under the catalog registry's lock.
                queue.TryEnqueue([weak]()
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

        // Before the send table goes away. A drum machine left running after the layout that
        // started it has closed is the worst thing this control could do.
        if (m_clocks != nullptr)
        {
            m_clocks->CancelAll();
            m_clocks->Stop();
        }

        if (m_lfos != nullptr)
        {
            m_lfos->CancelAll();
            m_lfos->Stop();
        }

        // Its last note ends while there is still somewhere to send the note off.
        StopAllSteps(false);

        if (m_steps != nullptr)
        {
            m_steps->Stop();
        }

        // A note held on a pad grid as the window closes still has to end.
        ReleaseAllPads();

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
        // An edit can renumber or remove a grid while a finger is on it. Its notes end now,
        // through the engine that started them, rather than being left sounding with nothing
        // that remembers them.
        ReleaseAllPads();

        // A sequencer is stopped for the same reason, and started again afterwards from its
        // first step if it is still there, so an edit made while it plays is heard at once.
        std::vector<std::wstring> runningSteps{};

        for (auto const& steps : m_stepControls)
        {
            if (steps.Run != 0)
            {
                runningSteps.push_back(steps.ControlId);
            }
        }

        StopAllSteps(false);

        m_throttles.assign(m_document.ControlCount(), ValueThrottle{});
        m_throttlesY.assign(m_document.ControlCount(), ValueThrottle{});
        m_soundingNotes.assign(m_document.ControlCount(), 0xFFFF);
        m_clockTickCounts.assign(m_document.ControlCount(), 0);

        m_relativeBase.clear();
        m_relativeBase.reserve(m_document.ControlCount());

        for (auto const& page : m_document.Pages)
        {
            for (auto const& control : page.Controls)
            {
                m_relativeBase.push_back(control.DefaultValue);
            }
        }

        m_clockControls.clear();
        m_lfoControls.clear();
        m_stepControls.clear();
        m_padControls.clear();

        size_t index{ 0 };

        // Every clock on the layout, and the control each one takes its tempo from, worked out
        // once here rather than on every tick.
        std::vector<std::pair<std::wstring, uint32_t>> idsByIndex{};

        for (auto const& page : m_document.Pages)
        {
            for (auto const& control : page.Controls)
            {
                if (index < m_throttles.size())
                {
                    auto const interval =
                        static_cast<uint32_t>(std::max(0, control.SendIntervalMilliseconds));

                    m_throttles[index].SetMinimumInterval(interval);
                    m_throttlesY[index].SetMinimumInterval(interval);
                }

                idsByIndex.emplace_back(control.Id, static_cast<uint32_t>(index));

                if (control.Kind == ControlKind::BeatClock)
                {
                    ClockEntry entry{};

                    entry.ControlIndex = static_cast<uint32_t>(index);
                    entry.ControlId = control.Id;
                    entry.Spec = control.Clock;

                    m_clockControls.push_back(std::move(entry));
                }

                if (control.Kind == ControlKind::Lfo)
                {
                    LfoEntry entry{};

                    entry.ControlIndex = static_cast<uint32_t>(index);
                    entry.Spec = control.Lfo;

                    m_lfoControls.push_back(std::move(entry));
                }

                if (control.Kind == ControlKind::Steps)
                {
                    StepsEntry entry{};

                    entry.ControlIndex = static_cast<uint32_t>(index);
                    entry.ControlId = control.Id;
                    entry.Spec = control.Steps;

                    m_stepControls.push_back(std::move(entry));
                }

                if (IsPadGrid(control.Kind))
                {
                    PadEntry entry{};

                    entry.ControlIndex = static_cast<uint32_t>(index);
                    entry.StartNote = std::clamp(control.Pads.StartNote, 0, 127);
                    entry.BendRange = std::clamp(
                        control.Pads.BendRangeSemitones, MinimumBendRangeSemitones, MaximumBendRangeSemitones);
                    entry.Voices.Configure(control.Pads.Glide, entry.BendRange);

                    m_padControls.push_back(std::move(entry));
                }

                index++;
            }
        }

        for (auto& clock : m_clockControls)
        {
            clock.TempoSourceIndex = -1;

            if (clock.Spec.TempoControlId.empty())
            {
                continue;
            }

            for (auto const& [id, at] : idsByIndex)
            {
                if (id == clock.Spec.TempoControlId)
                {
                    clock.TempoSourceIndex = static_cast<int32_t>(at);
                    break;
                }
            }
        }

        // The sequencers that were playing before the edit, now playing the edited pattern.
        if (m_steps != nullptr && !runningSteps.empty())
        {
            for (auto& steps : m_stepControls)
            {
                if (std::find(runningSteps.begin(), runningSteps.end(), steps.ControlId) != runningSteps.end())
                {
                    steps.Run = m_steps->Run(steps.ControlIndex, steps.Spec, m_document.Tempo.BeatsPerMinute);
                }
            }
        }
    }

    void LivePlayer::StartClocks()
    {
        // Connections come up in two steps that do not land together: the engine is prepared on
        // this thread from whatever the device watcher had resolved at that moment, and the
        // send table arrives from a background thread afterwards. Either can be ready first.
        //
        // A clock's start message is a one shot, so starting before both are ready burns it on
        // nothing and leaves the clock running, which makes every later pass skip it. Waiting
        // for both costs a second at launch and is the difference between a drum machine that
        // follows and one that never does.
        if (m_clocks == nullptr || m_sendTable.empty())
        {
            return;
        }

        auto reachable = false;

        for (auto const& destination : m_engine.Destinations())
        {
            if (destination.IsAvailable)
            {
                reachable = true;
                break;
            }
        }

        if (!reachable)
        {
            return;
        }

        for (auto const& clock : m_clockControls)
        {
            if (!clock.Spec.StartsRunning || m_clocks->IsRunning(clock.ControlIndex))
            {
                continue;
            }

            m_clocks->Run(clock.ControlIndex, clock.Spec.BeatsPerMinute, clock.Spec.SendsTransport);
        }

        if (m_lfos == nullptr)
        {
            return;
        }

        for (auto const& lfo : m_lfoControls)
        {
            if (!lfo.Spec.StartsRunning || m_lfos->IsRunning(lfo.ControlIndex))
            {
                continue;
            }

            m_lfos->Run(lfo.ControlIndex, lfo.Spec, m_document.Tempo.BeatsPerMinute);
        }

        if (m_steps == nullptr)
        {
            return;
        }

        for (auto& steps : m_stepControls)
        {
            if (!steps.Spec.StartsRunning || steps.Run != 0)
            {
                continue;
            }

            steps.Run = m_steps->Run(steps.ControlIndex, steps.Spec, m_document.Tempo.BeatsPerMinute);
        }
    }

    _Use_decl_annotations_
    LivePlayer::StepsEntry* LivePlayer::FindStepControl(uint32_t controlIndex) noexcept
    {
        for (auto& steps : m_stepControls)
        {
            if (steps.ControlIndex == controlIndex)
            {
                return &steps;
            }
        }

        return nullptr;
    }

    _Use_decl_annotations_
    void LivePlayer::OnStepChanged(uint32_t controlIndex, uint64_t run, int32_t stepIndex, bool starts) noexcept
    {
        try
        {
            auto* const entry = FindStepControl(controlIndex);

            // Queued before a stop and arrived after it. Playing it would start a note that
            // nothing is left to end.
            if (entry == nullptr || run == 0 || entry->Run != run)
            {
                return;
            }

            // The note before ends first, whether its time ran out or the next step came early.
            if (entry->SoundingNote != 0xFFFF)
            {
                auto const sounding = entry->SoundingNote;

                entry->SoundingNote = 0xFFFF;

                SendPrepared(controlIndex, m_engine.EvaluateNote(controlIndex, sounding, 0.0, false, m_sends));
            }

            if (!starts)
            {
                return;
            }

            if (stepIndex >= 0 && static_cast<size_t>(stepIndex) < entry->Spec.Pattern.size())
            {
                auto const& step = entry->Spec.Pattern[static_cast<size_t>(stepIndex)];

                if (step.On)
                {
                    auto const note = static_cast<uint16_t>(std::clamp(step.Note, 0, 127));

                    entry->SoundingNote = note;

                    SendPrepared(
                        controlIndex,
                        m_engine.EvaluateNote(controlIndex, note, step.Velocity, true, m_sends));
                }
            }

            if (StepMoved)
            {
                StepMoved(controlIndex, stepIndex, true);
            }
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    void LivePlayer::StopSteps(uint32_t controlIndex, bool announce) noexcept
    {
        try
        {
            auto* const entry = FindStepControl(controlIndex);

            if (entry == nullptr)
            {
                return;
            }

            if (m_steps != nullptr)
            {
                m_steps->CancelFor(controlIndex);
            }

            auto const wasRunning = entry->Run != 0;

            entry->Run = 0;

            if (entry->SoundingNote != 0xFFFF)
            {
                auto const sounding = entry->SoundingNote;

                entry->SoundingNote = 0xFFFF;

                SendPrepared(controlIndex, m_engine.EvaluateNote(controlIndex, sounding, 0.0, false, m_sends));
            }

            if (wasRunning && announce && StepMoved)
            {
                StepMoved(controlIndex, -1, false);
            }
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    void LivePlayer::StopAllSteps(bool announce) noexcept
    {
        for (auto const& steps : m_stepControls)
        {
            StopSteps(steps.ControlIndex, announce);
        }
    }

    _Use_decl_annotations_
    void LivePlayer::SetOutputEnabled(bool enabled) noexcept
    {
        // A sequencer left running behind a shut gate would pick up mid pattern the next time it
        // opened, with its last note never ended. It stops, and its note off goes out while the
        // gate is still open.
        if (!enabled && m_outputEnabled)
        {
            StopAllSteps(true);
        }

        m_outputEnabled = enabled;
    }

    _Use_decl_annotations_
    void LivePlayer::PulseTempoFollowers(uint32_t clockControlIndex, double phase, bool running)
    {
        if (!ActivitySeen)
        {
            return;
        }

        // A tick is raised twenty four times a quarter note; only the first of them is the
        // beat. Everything else would turn a lamp into a solid light.
        auto const onTheBeat = running && phase < 0.0001;

        std::wstring clockId{};

        for (auto const& clock : m_clockControls)
        {
            if (clock.ControlIndex == clockControlIndex)
            {
                clockId = clock.ControlId;
                break;
            }
        }

        if (clockId.empty())
        {
            return;
        }

        for (auto const& watcher : m_engine.TempoWatchers())
        {
            if (watcher.ClockControlId != clockId)
            {
                continue;
            }

            auto const index = static_cast<uint32_t>(watcher.ControlIndex);

            if (!running)
            {
                ActivitySeen(index, ListenerState::Off);
            }
            else if (onTheBeat)
            {
                ActivitySeen(index, ListenerState::Blink);
            }
        }
    }

    _Use_decl_annotations_
    void LivePlayer::TempoSourceMoved(uint32_t controlIndex, double value)
    {
        if (m_clocks == nullptr)
        {
            return;
        }

        for (auto const& clock : m_clockControls)
        {
            if (clock.TempoSourceIndex != static_cast<int32_t>(controlIndex))
            {
                continue;
            }

            // A fader's value is a position, so the clock says what its two ends mean. Somebody
            // wanting 60 to 180 gets exactly that rather than the whole legal range.
            auto const lowest = clock.Spec.LowestBeatsPerMinute;
            auto const highest = clock.Spec.HighestBeatsPerMinute;

            m_clocks->SetTempo(
                clock.ControlIndex,
                lowest + std::clamp(value, 0.0, 1.0) * (highest - lowest));
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
        auto const queue = m_dispatcher;

        std::thread([weak, ownerId, requests, queue]()
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

                    // Not resolved here, so the player is never destroyed on this thread.
                    if (queue != nullptr)
                    {
                        // The send table is only ever read on the UI thread, which is why the hot
                        // path needs no lock. Swapping it whole is what keeps that true.
                        queue.TryEnqueue([weak, table]()
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

                                // A clock marked to run on its own starts here rather than when
                                // the window opened: the device table resolves on this thread a
                                // moment later, and a start message sent before that goes
                                // nowhere at all.
                                inner->StartClocks();
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
            // the finger left it, and a spring return never reaches its rest value at all.
            double trailing{ 0.0 };

            if (!throttle.Release(value, trailing))
            {
                return;
            }

            value = trailing;
        }
        else if (!throttle.ShouldSend(value, NowMilliseconds()))
        {
            return;
        }

        SendPrepared(
            controlIndex,
            m_engine.Evaluate(controlIndex, MessageTrigger::Changes, value, m_sends));

        if (m_engine.HasRelativeRows(controlIndex) && controlIndex < m_relativeBase.size())
        {
            auto const ticks = TakeRelativeTicks(m_relativeBase[controlIndex], value);

            SendPrepared(controlIndex, m_engine.EvaluateRelative(controlIndex, ticks, m_sends));
        }

        // A knob or a fader can be a tempo control. Nothing happens unless some clock on this
        // layout named it, so every other control pays one loop over an empty list.
        TempoSourceMoved(controlIndex, value);
    }

    _Use_decl_annotations_
    void LivePlayer::ValueYChanged(uint32_t controlIndex, double value, bool isFinal)
    {
        if (controlIndex >= m_throttlesY.size())
        {
            return;
        }

        auto& throttle = m_throttlesY[controlIndex];

        if (isFinal)
        {
            double trailing{ 0.0 };

            if (!throttle.Release(value, trailing))
            {
                return;
            }

            value = trailing;
        }
        else if (!throttle.ShouldSend(value, NowMilliseconds()))
        {
            return;
        }

        SendPrepared(
            controlIndex,
            m_engine.EvaluateAxis(
                controlIndex, MessageTrigger::Changes, value, ValueAxis::Y, m_sends));
    }

    _Use_decl_annotations_
    void LivePlayer::KeyChanged(uint32_t controlIndex, int32_t key, double velocity, bool isDown)
    {
        if (controlIndex >= m_soundingNotes.size())
        {
            return;
        }

        auto const* const control = m_document.ControlAtIndex(controlIndex);

        if (control == nullptr)
        {
            return;
        }

        if (!isDown)
        {
            // The note that was actually started, not whatever the key maps to now. A keyboard
            // edited mid gesture must not leave a note sounding.
            auto const sounding = m_soundingNotes[controlIndex];

            if (sounding == 0xFFFF)
            {
                return;
            }

            m_soundingNotes[controlIndex] = 0xFFFF;

            SendPrepared(
                controlIndex,
                m_engine.EvaluateNote(controlIndex, sounding, 0.0, false, m_sends));

            return;
        }

        if (key < 0)
        {
            return;
        }

        auto const note = std::clamp(control->Keyboard.LowestNote + key, 0, 127);

        m_soundingNotes[controlIndex] = static_cast<uint16_t>(note);

        SendPrepared(
            controlIndex,
            m_engine.EvaluateNote(
                controlIndex, static_cast<uint16_t>(note), velocity, true, m_sends));
    }

    _Use_decl_annotations_
    LivePlayer::PadEntry* LivePlayer::FindPadControl(uint32_t controlIndex) noexcept
    {
        for (auto& entry : m_padControls)
        {
            if (entry.ControlIndex == controlIndex)
            {
                return &entry;
            }
        }

        return nullptr;
    }

    _Use_decl_annotations_
    void LivePlayer::SendPadActions(
        uint32_t controlIndex,
        int32_t bendRange,
        std::span<PadAction const> actions) noexcept
    {
        for (auto const& action : actions)
        {
            uint32_t written{ 0 };

            switch (action.Kind)
            {
            case PadActionKind::NoteOn:
                written = m_engine.EvaluateNote(controlIndex, action.Note, action.Value, true, m_sends);
                break;

            case PadActionKind::NoteOff:
                written = m_engine.EvaluateNote(controlIndex, action.Note, 0.0, false, m_sends);
                break;

            case PadActionKind::PortamentoFrom:
                written = m_engine.EvaluatePortamento(controlIndex, action.Note, m_sends);
                break;

            case PadActionKind::Bend:
                written = m_engine.EvaluatePerNotePitchBend(
                    controlIndex, action.Note, action.Value, static_cast<double>(bendRange), m_sends);
                break;

            case PadActionKind::BendRange:
                written = m_engine.EvaluatePerNoteBendRange(controlIndex, action.Value, m_sends);
                break;

            default:
                break;
            }

            // Straight out, in order, and never through a throttle: a note on held back is a
            // defect, and the portamento message has to reach the instrument before the note
            // it names the start of.
            SendPrepared(controlIndex, written);
        }
    }

    _Use_decl_annotations_
    void LivePlayer::PadPressed(uint32_t controlIndex, uint32_t touch, int32_t note, double velocity, double pitch)
    {
        auto* const entry = FindPadControl(controlIndex);

        if (entry == nullptr)
        {
            return;
        }

        std::array<PadAction, MaximumPadActions> actions{};

        auto const count = entry->Voices.Press(touch, note, velocity, pitch, actions);

        SendPadActions(controlIndex, entry->BendRange, std::span<PadAction const>{ actions.data(), count });
    }

    _Use_decl_annotations_
    void LivePlayer::PadMoved(uint32_t controlIndex, uint32_t touch, int32_t note, double velocity, double pitch)
    {
        auto* const entry = FindPadControl(controlIndex);

        if (entry == nullptr)
        {
            return;
        }

        std::array<PadAction, MaximumPadActions> actions{};

        auto const count = entry->Voices.Move(touch, note, velocity, pitch, actions);

        SendPadActions(controlIndex, entry->BendRange, std::span<PadAction const>{ actions.data(), count });
    }

    _Use_decl_annotations_
    void LivePlayer::PadReleased(uint32_t controlIndex, uint32_t touch)
    {
        auto* const entry = FindPadControl(controlIndex);

        if (entry == nullptr)
        {
            return;
        }

        std::array<PadAction, MaximumPadActions> actions{};

        auto const count = entry->Voices.Release(touch, actions);

        SendPadActions(controlIndex, entry->BendRange, std::span<PadAction const>{ actions.data(), count });
    }

    void LivePlayer::ReleaseAllPads() noexcept
    {
        for (auto& entry : m_padControls)
        {
            if (entry.Voices.HeldCount() == 0)
            {
                continue;
            }

            std::array<PadAction, PadVoices::MaximumTouches> actions{};

            auto const count = entry.Voices.ReleaseAll(actions);

            SendPadActions(entry.ControlIndex, entry.BendRange, std::span<PadAction const>{ actions.data(), count });
        }
    }

    _Use_decl_annotations_
    void LivePlayer::Switched(uint32_t controlIndex, bool isOn)
    {
        Switched(controlIndex, isOn, 1.0);
    }

    _Use_decl_annotations_
    void LivePlayer::Switched(uint32_t controlIndex, bool isOn, double velocity)
    {
        // A clock is running or it is not, and pressing it is what changes which.
        if (m_clocks != nullptr)
        {
            for (auto const& clock : m_clockControls)
            {
                if (clock.ControlIndex != controlIndex)
                {
                    continue;
                }

                if (isOn)
                {
                    m_clocks->Run(
                        controlIndex, clock.Spec.BeatsPerMinute, clock.Spec.SendsTransport);
                }
                else
                {
                    m_clocks->CancelFor(controlIndex);
                }

                return;
            }
        }

        // A sweep is running or it is not, and pressing it is what changes which. Whether the
        // press latches or has to be held is the control's own setting, so both arrive here the
        // same way and the surface decides which one a release means.
        if (m_lfos != nullptr)
        {
            for (auto const& lfo : m_lfoControls)
            {
                if (lfo.ControlIndex != controlIndex)
                {
                    continue;
                }

                if (isOn)
                {
                    m_lfos->Run(controlIndex, lfo.Spec, m_document.Tempo.BeatsPerMinute);
                }
                else
                {
                    m_lfos->CancelFor(controlIndex);
                }

                return;
            }
        }

        // A sequencer is the same: a press starts it from its first step and a release or a
        // second press stops it, and a stop always ends the note it was playing.
        if (auto* const steps = FindStepControl(controlIndex))
        {
            if (!isOn)
            {
                StopSteps(controlIndex, true);
            }
            else if (steps->Run == 0 && m_steps != nullptr)
            {
                steps->Run = m_steps->Run(controlIndex, steps->Spec, m_document.Tempo.BeatsPerMinute);
            }

            return;
        }

        // A pad grid pressed through assistive technology has no finger to say which pad, so
        // it plays its first one, the note the grid starts from. Its note row has no number of
        // its own, so going on below would play note zero.
        if (auto* const pads = FindPadControl(controlIndex))
        {
            constexpr uint32_t AutomationTouch = 0xFFFFFFFFu;

            if (isOn)
            {
                PadPressed(controlIndex, AutomationTouch, pads->StartNote, velocity, pads->StartNote);
            }
            else
            {
                PadReleased(controlIndex, AutomationTouch);
            }

            return;
        }

        // Nothing but a continuous control is ever throttled. Rate limiting a note on would be a
        // defect, not a feature.
        //
        // How hard it was hit rides in as the value, which is what the message's own two ends
        // then scale. A pad that does not measure pressure passes 1.0 and lands on the top end,
        // exactly as it did before. A note row is told on or off by the press itself, so a
        // light touch with a pen still plays a note rather than sending a note off.
        auto const hit = isOn ? std::clamp(velocity, 0.0, 1.0) : 0.0;

        SendPrepared(
            controlIndex,
            m_engine.EvaluatePress(
                controlIndex,
                isOn ? MessageTrigger::TurnsOn : MessageTrigger::TurnsOff,
                isOn,
                hit,
                m_sends));

        // A control that only declares "changes" still has to do something when a pad is hit.
        SendPrepared(
            controlIndex,
            m_engine.EvaluatePress(controlIndex, MessageTrigger::Changes, isOn, hit, m_sends));

        RunPlan(controlIndex, isOn ? MessageTrigger::TurnsOn : MessageTrigger::TurnsOff);
    }

    _Use_decl_annotations_
    void LivePlayer::Touched(uint32_t controlIndex, bool isTouched)
    {
        // A control that springs back when let go has not turned anything by doing so.
        if (!isTouched && m_engine.HasRelativeRows(controlIndex) && controlIndex < m_relativeBase.size())
        {
            if (auto const* const control = m_document.ControlAtIndex(controlIndex);
                control != nullptr && control->ReturnsToDefault)
            {
                m_relativeBase[controlIndex] = control->DefaultValue;
            }
        }

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
    bool LivePlayer::RunSequenceNow(Sequence const& sequence, uint32_t controlIndex)
    {
        if (!m_outputEnabled || m_runner == nullptr || m_sendTable.empty())
        {
            return false;
        }

        auto const plan = BuildPlanForSequence(m_document, m_engine.Destinations(), sequence);

        if (plan.Actions.empty())
        {
            return false;
        }

        // A second press restarts it rather than layering a second copy over the first.
        m_runner->CancelFor(controlIndex);
        m_runner->Run(controlIndex, plan);

        return true;
    }

    _Use_decl_annotations_
    void LivePlayer::StopSequenceNow(uint32_t controlIndex) noexcept
    {
        if (m_runner != nullptr)
        {
            m_runner->CancelFor(controlIndex);
        }
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
    std::wstring LivePlayer::DescribeValue(
        uint32_t controlIndex,
        ValueAxis axis,
        double position) const
    {
        uint32_t value{ 0 };
        bool isAbsolute{ false };

        if (m_engine.TryDescribeValue(controlIndex, axis, position, value, isAbsolute) && isAbsolute)
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

        if (!learning && !FeedbackMoved && !ActivitySeen)
        {
            return;
        }

        // Which entry of this layout's device table the message came in on, so an activity lamp
        // can be narrowed to one device.
        auto const destinationIndex = m_devices.IndexOfEndpoint(endpointDeviceId);

        // This is a service callback thread and the buffer belongs to the caller, so what is
        // wanted is resolved here and only the answer is marshalled.
        struct Move
        {
            uint32_t ControlIndex{ 0 };
            double Value{ 0.0 };
            bool Blinks{ false };
        };

        std::vector<Move> moves{};
        std::vector<LearnedBinding> captures{};
        std::vector<uint32_t> lit{};
        std::vector<std::pair<uint32_t, ListenerState>> latched{};
        std::vector<uint32_t> ticks{};

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
            bool blinks{ false };

            if (FeedbackMoved && m_engine.TryResolveFeedback(
                words + position, length, destinationIndex, controlIndex, value, blinks))
            {
                moves.push_back({ static_cast<uint32_t>(controlIndex), value, blinks });
            }

            if (ActivitySeen)
            {
                std::array<BindingEngine::FeedbackHit, 32> watching{};

                auto const count = m_engine.CollectFeedbackHits(
                    words + position, length, destinationIndex, watching);

                for (uint32_t i = 0; i < count; ++i)
                {
                    auto const index = static_cast<uint32_t>(watching[i].ControlIndex);

                    switch (watching[i].Kind)
                    {
                    case BindingEngine::FeedbackHitKind::On:
                        latched.emplace_back(index, ListenerState::On);
                        break;

                    case BindingEngine::FeedbackHitKind::Off:
                        latched.emplace_back(index, ListenerState::Off);
                        break;

                    case BindingEngine::FeedbackHitKind::ClockTick:
                        ticks.push_back(index);
                        break;

                    default:
                        // One message can light several lamps, but a burst of a hundred must
                        // not queue a hundred hits at the same one.
                        if (std::find(lit.begin(), lit.end(), index) == lit.end())
                        {
                            lit.push_back(index);
                        }

                        break;
                    }
                }
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

        if (moves.empty() && captures.empty() && lit.empty() && latched.empty() && ticks.empty())
        {
            return;
        }

        std::weak_ptr<LivePlayer> weak{ weak_from_this() };

        m_dispatcher.TryEnqueue([weak, moves, captures, lit, latched, ticks]()
            {
                auto strong = weak.lock();

                if (strong == nullptr || strong->m_stopping)
                {
                    return;
                }

                if (strong->FeedbackMoved)
                {
                    for (auto const& move : moves)
                    {
                        if (move.Blinks && strong->FeedbackBlinks)
                        {
                            strong->FeedbackBlinks(move.ControlIndex);
                        }
                        else
                        {
                            strong->FeedbackMoved(move.ControlIndex, move.Value);
                        }
                    }
                }

                if (strong->ActivitySeen)
                {
                    for (auto const controlIndex : lit)
                    {
                        strong->ActivitySeen(controlIndex, ListenerState::Blink);
                    }

                    for (auto const& [controlIndex, state] : latched)
                    {
                        strong->ActivitySeen(controlIndex, state);
                    }

                    // Twenty four clock messages is a quarter note. Counting them here rather
                    // than in the engine keeps the engine free of state between messages.
                    for (auto const controlIndex : ticks)
                    {
                        if (controlIndex >= strong->m_clockTickCounts.size())
                        {
                            continue;
                        }

                        auto& count = strong->m_clockTickCounts[controlIndex];

                        if (++count < ClockTicksPerQuarterNote)
                        {
                            continue;
                        }

                        count = 0;

                        strong->ActivitySeen(controlIndex, ListenerState::Blink);
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
