// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// MIDI remote control: a foot controller or another device switching pages, pressing controls,
// turning controls on and off and changing pad grids while the layout runs.
//
// Which rule a message sets off is worked out on the service's callback thread, next to the
// feedback a control follows, and only the answer is marshalled. What the rule does happens on
// the dispatcher's thread, through the same paths a finger takes, so a rule can only ever do
// what the layout already holds.

#include "pch.h"
#include "LivePlayer.h"
#include "PadGrid.h"
#include "PadGridChanges.h"

namespace glass
{
    namespace
    {
        // Labels for what the monitor shows. None of them is a control, so a filter on the
        // selected control leaves them out.
        constexpr uint32_t RemoteSequenceIndex = 0xFFFFFFF0u;
        constexpr uint32_t PageOpenSequenceIndex = 0xFFFFFFF1u;
        constexpr uint32_t PageFeedbackIndex = 0xFFFFFFF2u;

        uint64_t NowMilliseconds() noexcept
        {
            return static_cast<uint64_t>(::GetTickCount64());
        }

        // A press latches on these rather than springing back.
        bool Latches(_In_ Control const& control) noexcept
        {
            switch (control.Kind)
            {
            case ControlKind::Toggle:
            case ControlKind::BeatClock:

            // A sweep or a sequencer that runs only while held would stop as soon as it
            // started, because a rule can't hold anything down. Pressed by a rule, it latches.
            case ControlKind::Lfo:
            case ControlKind::Steps:
                return true;

            default:
                return false;
            }
        }

        bool IsTapped(_In_ ControlKind kind) noexcept
        {
            switch (kind)
            {
            case ControlKind::Pad:
            case ControlKind::Button:
            case ControlKind::PageTab:
            case ControlKind::NotePads:
            case ControlKind::HexPads:
            case ControlKind::Fretboard:
                return true;

            default:
                return false;
            }
        }
    }

    void LivePlayer::PrepareRemote()
    {
        auto table = std::make_shared<RemoteRuleTable>();

        table->Prepare(m_document);

        auto const hasRules = !table->IsEmpty();

        {
            std::lock_guard guard{ m_remoteLock };
            m_remoteRules = std::move(table);
        }

        m_hasRemoteRules = hasRules;

        if (!hasRules)
        {
            std::lock_guard guard{ m_echoLock };
            m_echo.Clear();
        }

        m_controlStates.Prepare(m_document);
    }

    _Use_decl_annotations_
    void LivePlayer::CollectRemoteHits(
        int32_t deviceIndex,
        uint32_t const* words,
        uint32_t wordCount,
        std::vector<RemoteHit>& hits) noexcept
    {
        if (!m_hasRemoteRules.load() || words == nullptr || wordCount == 0)
        {
            return;
        }

        try
        {
            std::shared_ptr<RemoteRuleTable const> table{};

            {
                std::lock_guard guard{ m_remoteLock };
                table = m_remoteRules;
            }

            if (table == nullptr || table->IsEmpty())
            {
                return;
            }

            {
                std::lock_guard guard{ m_echoLock };

                if (m_echo.IsEcho(deviceIndex, words, wordCount, NowMilliseconds()))
                {
                    return;
                }
            }

            std::array<RemoteHit, MaximumRemoteHitsPerMessage> found{};

            auto const count = table->Match(words, wordCount, deviceIndex, found);

            for (uint32_t i = 0; i < count; ++i)
            {
                auto hit = found[i];

                hit.DeviceIndex = deviceIndex;
                hit.WordCount = std::min<uint32_t>(wordCount, 2);

                for (uint32_t word = 0; word < hit.WordCount; ++word)
                {
                    hit.Words[word] = words[word];
                }

                hits.push_back(hit);
            }
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    void LivePlayer::RunRemoteHits(std::vector<RemoteHit> const& hits)
    {
        auto const now = NowMilliseconds();
        auto const& rules = m_document.Remote.Rules;

        for (size_t at = 0; at < hits.size(); ++at)
        {
            auto const& hit = hits[at];

            if (hit.RuleIndex >= rules.size())
            {
                continue;
            }

            auto const& rule = rules[hit.RuleIndex];

            // An expression pedal swept across its travel sends a burst. A rule that takes the
            // value only needs the last of it.
            if (TakesFromMessage(rule))
            {
                auto const later = std::any_of(hits.begin() + static_cast<ptrdiff_t>(at) + 1, hits.end(),
                    [&hit](RemoteHit const& other) { return other.RuleIndex == hit.RuleIndex; });

                if (later)
                {
                    continue;
                }
            }

            m_heard.NoteSent(hit.DeviceIndex, hit.Words, hit.WordCount, now);

            if (RemoteRuleHeard)
            {
                RemoteRuleHeard(hit.RuleIndex);
            }

            // The editor listens in Edit mode so a rule's row can light, but nothing it does
            // happens until Try mode.
            if (!m_outputEnabled || !m_remoteLive || m_stopping || !m_remoteRate.Allow(now))
            {
                continue;
            }

            RemoteCommand command{};

            auto const current = m_shownPage == static_cast<size_t>(-1) ? size_t{ 0 } : m_shownPage;

            if (ResolveRemoteCommand(m_document, rule, hit, current, command))
            {
                RunRemoteCommand(command);
            }
        }
    }

    _Use_decl_annotations_
    void LivePlayer::RunRemoteCommand(RemoteCommand const& command)
    {
        switch (command.Action)
        {
        case RemoteActionKind::GoToPage:
        case RemoteActionKind::NextPage:
        case RemoteActionKind::PreviousPage:
        case RemoteActionKind::PageFromMessage:
            if (PageRequested && command.PageIndex >= 0)
            {
                PageRequested(static_cast<uint32_t>(command.PageIndex));
            }
            break;

        case RemoteActionKind::PressControl:
            for (auto const index : command.Controls)
            {
                PressFromRemote(index);
            }
            break;

        case RemoteActionKind::SetControlValue:
            for (auto const index : command.Controls)
            {
                if (!TakesInput(index))
                {
                    continue;
                }

                // A whole gesture, the way a screen reader setting a value is one: moved, then
                // let go, so the throttle sends the value it was put at.
                ValueChanged(index, command.Value, false);
                ValueChanged(index, command.Value, true);
                RememberValue(index, command.Value);

                if (ControlValueSet)
                {
                    ControlValueSet(index, command.Value);
                }
            }
            break;

        case RemoteActionKind::EnableControl:
        case RemoteActionKind::DisableControl:
        case RemoteActionKind::ToggleEnabled:
        case RemoteActionKind::ShowControl:
        case RemoteActionKind::HideControl:
        case RemoteActionKind::ToggleShown:
        case RemoteActionKind::ShowOnlyThis:
        {
            auto const changed = m_controlStates.Apply(command);

            for (auto const index : changed)
            {
                if (!TakesInput(index))
                {
                    StopControl(index);
                }
            }

            if (!changed.empty() && ControlStatesChanged)
            {
                ControlStatesChanged(changed);
            }

            break;
        }

        case RemoteActionKind::PadGrid:
        {
            PlanAction action{};

            action.Kind = ActionKind::ChangePadGrid;
            action.PadFunction = command.PadFunction;
            action.PadTargets = command.PadTargets;

            ChangePadGrids(action);
            break;
        }

        case RemoteActionKind::SetKeyRoot:
        case RemoteActionKind::SetScale:
        {
            // The same change a root or scale picker makes, so every picker and readout follows.
            Control picker{};

            picker.Kind = ControlKind::Switch;
            picker.Switch.Targets = command.PadTargets;

            int32_t position{ 0 };

            if (command.Action == RemoteActionKind::SetKeyRoot)
            {
                picker.Switch.Picks = SwitchPicks::RootNote;
                position = command.KeyRoot;
            }
            else
            {
                picker.Switch.Picks = SwitchPicks::Scale;
                picker.Switch.Scales.push_back(static_cast<MusicalScale>(command.Scale));
            }

            if (ApplyPick(m_document, picker, position, m_gridStates))
            {
                AnnounceGridChanges();
            }

            break;
        }

        case RemoteActionKind::RunSequence:
        {
            auto const* const sequence = m_document.FindSequence(command.SequenceName);

            if (sequence == nullptr || m_runner == nullptr || m_sendTable.empty())
            {
                break;
            }

            auto const plan = BuildPlanForSequence(m_document, m_engine.Destinations(), *sequence);

            if (!plan.Actions.empty())
            {
                m_runner->CancelFor(RemoteSequenceIndex);
                m_runner->Run(RemoteSequenceIndex, plan);
            }

            break;
        }

        case RemoteActionKind::Panic:
            Panic();
            break;

        default:
            break;
        }
    }

    _Use_decl_annotations_
    void LivePlayer::PressFromRemote(uint32_t controlIndex)
    {
        auto const* const control = m_document.ControlAtIndex(controlIndex);

        if (control == nullptr || !TakesInput(controlIndex))
        {
            return;
        }

        // Where a finger or a rule last put it, or where the layout starts it.
        auto const last = controlIndex < m_lastValues.size() && m_lastValues[controlIndex] >= 0.0
            ? m_lastValues[controlIndex]
            : control->DefaultValue;

        if (Latches(*control))
        {
            auto const isOn = last < 0.5;

            Switched(controlIndex, isOn);

            if (ControlValueSet)
            {
                ControlValueSet(controlIndex, isOn ? 1.0 : 0.0);
            }

            return;
        }

        // A switch moves on a position, round to the first after the last: one foot switch
        // stepping through a mode.
        if (control->Kind == ControlKind::Switch)
        {
            auto const positions = SwitchPositionCount(*control);
            auto const next = (SwitchPositionAt(last, positions) + 1) % positions;
            auto const value = SwitchValueOf(next, positions);

            ValueChanged(controlIndex, value, true);
            RememberValue(controlIndex, value);

            if (ControlValueSet)
            {
                ControlValueSet(controlIndex, value);
            }

            return;
        }

        if (IsTapped(control->Kind))
        {
            Switched(controlIndex, true, 1.0);
            Switched(controlIndex, false, 0.0);

            // Moves nothing, and lights the control so the person on stage sees it go.
            if (ControlValueSet)
            {
                ControlValueSet(controlIndex, 0.0);
            }

            return;
        }

        // A knob or a fader has nothing to press, so it is touched and let go, which is what a
        // dump or a sequence hung off it waits for.
        Touched(controlIndex, true);
        Touched(controlIndex, false);
    }

    _Use_decl_annotations_
    void LivePlayer::StopControl(uint32_t controlIndex) noexcept
    {
        try
        {
            if (m_clocks != nullptr)
            {
                m_clocks->CancelFor(controlIndex);
            }

            if (m_lfos != nullptr)
            {
                m_lfos->CancelFor(controlIndex);
            }

            if (FindStepControl(controlIndex) != nullptr)
            {
                StopSteps(controlIndex, true);
            }

            if (m_runner != nullptr)
            {
                m_runner->CancelFor(controlIndex);
            }

            if (controlIndex < m_lastValues.size())
            {
                m_lastValues[controlIndex] = 0.0;
            }
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    void LivePlayer::PageShown(size_t pageIndex)
    {
        if (pageIndex >= m_document.Pages.size() || pageIndex == m_shownPage)
        {
            return;
        }

        m_shownPage = pageIndex;

        m_pageOpenPending = true;

        // Waits for the connections, or for Try mode in the editor.
        if (m_outputEnabled && m_remoteLive && !m_sendTable.empty())
        {
            QueuePageOpened();
        }
    }

    void LivePlayer::QueuePageOpened()
    {
        if (m_dispatcher == nullptr)
        {
            return;
        }

        // Never from inside the owner's page build: an open sequence can ask for another page,
        // and building one page from the middle of building another would tear it down under
        // itself.
        std::weak_ptr<LivePlayer> weak{ weak_from_this() };

        m_dispatcher.TryEnqueue([weak]()
            {
                auto strong = weak.lock();

                if (strong == nullptr || strong->m_stopping || !strong->m_pageOpenPending ||
                    !strong->m_outputEnabled || !strong->m_remoteLive || strong->m_sendTable.empty())
                {
                    return;
                }

                try
                {
                    strong->RunPageOpened(strong->m_shownPage);
                }
                catch (...)
                {
                }
            });
    }

    _Use_decl_annotations_
    void LivePlayer::RunPageOpened(size_t pageIndex)
    {
        m_pageOpenPending = false;

        if (pageIndex >= m_document.Pages.size() || m_stopping)
        {
            return;
        }

        int32_t device{ -1 };
        uint32_t words[1]{};

        // The controller's lights follow the page. Not when the controller is what just asked
        // for this page with this same message: that would only send it straight back.
        if (BuildPageFeedback(m_document, pageIndex, device, words) == 1 &&
            !m_heard.IsEcho(device, words, 1, NowMilliseconds()))
        {
            SendWords(PageFeedbackIndex, device, words, 1);
        }

        auto const& name = m_document.Pages[pageIndex].OpenSequenceName;

        if (name.empty() || m_runner == nullptr)
        {
            return;
        }

        if (auto const* const sequence = m_document.FindSequence(name))
        {
            auto const plan = BuildPlanForSequence(m_document, m_engine.Destinations(), *sequence);

            // A page left before its sequence finished stops it, so two pages' sequences never
            // play over each other.
            m_runner->CancelFor(PageOpenSequenceIndex);

            if (!plan.Actions.empty())
            {
                m_runner->Run(PageOpenSequenceIndex, plan);
            }
        }
    }

    _Use_decl_annotations_
    void LivePlayer::SetRemoteLive(bool live)
    {
        if (live == m_remoteLive)
        {
            return;
        }

        m_remoteLive = live;

        // Leaving opens the page again next time; arriving opens it now.
        if (!live)
        {
            m_pageOpenPending = m_shownPage < m_document.Pages.size();
        }
        else if (m_pageOpenPending && m_outputEnabled && !m_sendTable.empty())
        {
            QueuePageOpened();
        }
    }

    _Use_decl_annotations_
    void LivePlayer::UpdateRemoteRules(LayoutDocument const& document)
    {
        if (!m_started || m_stopping)
        {
            return;
        }

        m_document.Remote = document.Remote;
        PrepareRemote();
    }

    void LivePlayer::ForgetControlStates() noexcept
    {
        m_controlStates.Reset();
        m_lastValues.clear();
        m_heard.Clear();
    }

    _Use_decl_annotations_
    void LivePlayer::RememberValue(uint32_t controlIndex, double value) noexcept
    {
        try
        {
            if (controlIndex >= m_lastValues.size())
            {
                if (controlIndex >= m_document.ControlCount())
                {
                    return;
                }

                m_lastValues.resize(m_document.ControlCount(), -1.0);
            }

            m_lastValues[controlIndex] = value;
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    void LivePlayer::NoteSentForEcho(int32_t destinationIndex, uint32_t const* words, uint32_t wordCount) noexcept
    {
        if (!m_hasRemoteRules.load(std::memory_order_relaxed))
        {
            return;
        }

        try
        {
            std::lock_guard guard{ m_echoLock };
            m_echo.NoteSent(destinationIndex, words, wordCount, NowMilliseconds());
        }
        catch (...)
        {
        }
    }
}
