// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Deliberately free of pch.h, XAML and the MIDI SDK, so the unit tests compile it unchanged.

#include "RemoteRules.h"
#include "LearnCapture.h"

#include <algorithm>
#include <cmath>

namespace glass
{
    namespace
    {
        bool CanListenFor(_In_ MessageKind kind) noexcept
        {
            return kind == MessageKind::Note ||
                kind == MessageKind::ControlChange ||
                kind == MessageKind::ProgramChange;
        }

        // The number a rule takes from a message: a program change's program, a control change's
        // value on the 7 bit scale a foot controller is set up in, or a note's number.
        int32_t CarriedNumber(_In_ LearnedBinding const& learned) noexcept
        {
            if (learned.Kind == MessageKind::ControlChange)
            {
                return static_cast<int32_t>(std::lround(std::clamp(learned.Value, 0.0, 1.0) * 127.0));
            }

            return static_cast<int32_t>(std::min<uint32_t>(learned.Number, 127));
        }

        // Every control on the layout, by the index the binding engine counts with.
        template <typename TVisit>
        void ForEachControl(_In_ LayoutDocument const& document, _In_ TVisit&& visit)
        {
            uint32_t index{ 0 };

            for (size_t page = 0; page < document.Pages.size(); ++page)
            {
                for (auto const& control : document.Pages[page].Controls)
                {
                    visit(control, page, index);
                    ++index;
                }
            }
        }

        bool UsesControls(_In_ RemoteActionKind action) noexcept
        {
            switch (action)
            {
            case RemoteActionKind::PressControl:
            case RemoteActionKind::SetControlValue:
            case RemoteActionKind::EnableControl:
            case RemoteActionKind::DisableControl:
            case RemoteActionKind::ToggleEnabled:
            case RemoteActionKind::ShowControl:
            case RemoteActionKind::HideControl:
            case RemoteActionKind::ToggleShown:
            case RemoteActionKind::ShowOnlyThis:
                return true;

            default:
                return false;
            }
        }
    }

    _Use_decl_annotations_
    bool TakesFromMessage(RemoteRule const& rule) noexcept
    {
        switch (rule.Action)
        {
        case RemoteActionKind::PageFromMessage:
            return true;

        case RemoteActionKind::SetControlValue:
        case RemoteActionKind::SetKeyRoot:
        case RemoteActionKind::SetScale:
            return rule.FromMessage;

        default:
            return false;
        }
    }

    // ---------------------------------------------------------------- matching

    _Use_decl_annotations_
    void RemoteRuleTable::Prepare(LayoutDocument const& document)
    {
        m_rules.clear();

        auto const& rules = document.Remote.Rules;

        for (size_t index = 0; index < rules.size() && index < MaximumRemoteRules; ++index)
        {
            auto const& rule = rules[index];

            if (!rule.Enabled || rule.Action == RemoteActionKind::Unrecognized || !CanListenFor(rule.When.Kind))
            {
                continue;
            }

            PreparedRule prepared{};

            prepared.RuleIndex = static_cast<uint32_t>(index);
            prepared.Kind = rule.When.Kind;
            prepared.GroupIndex = rule.When.GroupIndex;
            prepared.ChannelIndex = rule.When.ChannelIndex;
            prepared.Number = rule.When.Number;
            prepared.TakesValue = TakesFromMessage(rule);

            if (!rule.When.DeviceName.empty())
            {
                prepared.DeviceMissing = true;

                for (size_t device = 0; device < document.Devices.size(); ++device)
                {
                    if (document.Devices[device].Name == rule.When.DeviceName)
                    {
                        prepared.DeviceIndex = static_cast<int32_t>(device);
                        prepared.DeviceMissing = false;
                        break;
                    }
                }
            }

            m_rules.push_back(prepared);
        }
    }

    _Use_decl_annotations_
    uint32_t RemoteRuleTable::Match(
        uint32_t const* words,
        uint32_t wordCount,
        int32_t deviceIndex,
        std::span<RemoteHit> hits) const noexcept
    {
        if (m_rules.empty() || hits.empty())
        {
            return 0;
        }

        LearnedBinding learned{};

        if (!TryLearnFromWords(words, wordCount, learned) || !CanListenFor(learned.Kind))
        {
            return 0;
        }

        // A note off, or a note on at velocity 0, is a switch being let go.
        if (learned.Kind == MessageKind::Note && learned.Value <= 0.0)
        {
            return 0;
        }

        auto const carried = CarriedNumber(learned);
        uint32_t written{ 0 };

        for (auto const& rule : m_rules)
        {
            if (written >= hits.size())
            {
                break;
            }

            if (rule.DeviceMissing ||
                (rule.DeviceIndex >= 0 && rule.DeviceIndex != deviceIndex) ||
                rule.Kind != learned.Kind ||
                rule.GroupIndex != learned.GroupIndex ||
                (rule.ChannelIndex != AnyRemoteChannel && rule.ChannelIndex != learned.ChannelIndex) ||
                (rule.Number != AnyRemoteNumber && rule.Number != static_cast<int32_t>(learned.Number)))
            {
                continue;
            }

            if (learned.Kind == MessageKind::ControlChange && !rule.TakesValue && learned.Value <= 0.0)
            {
                continue;
            }

            auto& hit = hits[written];

            hit.RuleIndex = rule.RuleIndex;
            hit.Carried = carried;
            hit.Value = learned.Kind == MessageKind::ProgramChange
                ? carried / 127.0
                : std::clamp(learned.Value, 0.0, 1.0);

            ++written;
        }

        return written;
    }

    // ---------------------------------------------------------------- resolving

    _Use_decl_annotations_
    int32_t PagePosition(LayoutDocument const& document, size_t pageIndex)
    {
        auto const pages = PagesToChooseFrom(document);
        auto const found = std::find(pages.begin(), pages.end(), pageIndex);

        return found == pages.end() ? -1 : static_cast<int32_t>(found - pages.begin());
    }

    _Use_decl_annotations_
    bool ResolveRemoteCommand(
        LayoutDocument const& document,
        RemoteRule const& rule,
        RemoteHit const& hit,
        size_t currentPageIndex,
        RemoteCommand& command)
    {
        command = {};
        command.Action = rule.Action;

        if (!rule.Enabled)
        {
            return false;
        }

        switch (rule.Action)
        {
        case RemoteActionKind::GoToPage:
        {
            for (size_t page = 0; page < document.Pages.size(); ++page)
            {
                if (!rule.TargetPageId.empty() && document.Pages[page].Id == rule.TargetPageId)
                {
                    command.PageIndex = static_cast<int32_t>(page);
                    return true;
                }
            }

            return false;
        }

        case RemoteActionKind::NextPage:
        case RemoteActionKind::PreviousPage:
        case RemoteActionKind::PageFromMessage:
        {
            auto const pages = PagesToChooseFrom(document);

            if (pages.empty())
            {
                return false;
            }

            if (rule.Action == RemoteActionKind::PageFromMessage)
            {
                if (hit.Carried < 0 || static_cast<size_t>(hit.Carried) >= pages.size())
                {
                    return false;
                }

                command.PageIndex = static_cast<int32_t>(pages[static_cast<size_t>(hit.Carried)]);
                return true;
            }

            // Stops at either end rather than going round. On stage, a page that wraps from the
            // last song back to the first is a surprise nobody wants.
            auto const at = std::find(pages.begin(), pages.end(), currentPageIndex);
            auto position = at == pages.end() ? 0 : static_cast<size_t>(at - pages.begin());

            if (rule.Action == RemoteActionKind::NextPage)
            {
                if (at != pages.end() && position + 1 >= pages.size())
                {
                    return false;
                }

                position = at == pages.end() ? 0 : position + 1;
            }
            else
            {
                if (at == pages.end() || position == 0)
                {
                    return false;
                }

                --position;
            }

            command.PageIndex = static_cast<int32_t>(pages[position]);
            return static_cast<size_t>(command.PageIndex) != currentPageIndex;
        }

        case RemoteActionKind::PadGrid:
            command.PadFunction = rule.PadFunction;
            command.PadTargets = rule.PadTargets;
            return true;

        case RemoteActionKind::SetKeyRoot:
            command.PadTargets = rule.PadTargets;
            command.KeyRoot = rule.FromMessage ? hit.Carried % 12 : rule.KeyRoot;
            return command.KeyRoot >= 0 && command.KeyRoot <= 11;

        case RemoteActionKind::SetScale:
        {
            command.PadTargets = rule.PadTargets;

            auto const scale = rule.FromMessage ? hit.Carried : static_cast<int32_t>(rule.Scale);

            if (scale < 0 || scale > static_cast<int32_t>(MusicalScale::WholeTone))
            {
                return false;
            }

            command.Scale = scale;
            return true;
        }

        case RemoteActionKind::RunSequence:
            command.SequenceName = rule.SequenceName;
            return !rule.SequenceName.empty() && document.FindSequence(rule.SequenceName) != nullptr;

        case RemoteActionKind::Panic:
            return true;

        default:
            break;
        }

        if (!UsesControls(rule.Action))
        {
            return false;
        }

        // A group, or one control. Only what the layout already holds can be reached.
        std::wstring targetGroup{};
        size_t targetPage{ 0 };
        auto found = false;

        ForEachControl(document, [&](Control const& control, size_t page, uint32_t index)
            {
                auto const isTarget = !rule.TargetGroupId.empty()
                    ? control.GroupId == rule.TargetGroupId
                    : (!rule.TargetControlId.empty() && control.Id == rule.TargetControlId);

                if (!isTarget)
                {
                    return;
                }

                command.Controls.push_back(index);

                if (!found)
                {
                    found = true;
                    targetGroup = control.GroupId;
                    targetPage = page;
                }
            });

        if (command.Controls.empty())
        {
            return false;
        }

        if (rule.Action == RemoteActionKind::SetControlValue)
        {
            command.Value = std::clamp(rule.FromMessage ? hit.Value : rule.Value, 0.0, 1.0);
        }

        // Show only this: the controls stacked with it in its group go out of sight. A rule naming
        // a whole group shows that group and hides nothing.
        if (rule.Action == RemoteActionKind::ShowOnlyThis && rule.TargetGroupId.empty() && !targetGroup.empty())
        {
            ForEachControl(document, [&](Control const& control, size_t page, uint32_t index)
                {
                    if (page == targetPage &&
                        control.GroupId == targetGroup &&
                        std::find(command.Controls.begin(), command.Controls.end(), index) == command.Controls.end())
                    {
                        command.HideControls.push_back(index);
                    }
                });
        }

        return true;
    }

    // ---------------------------------------------------------------- control state

    _Use_decl_annotations_
    void ControlStateSet::Prepare(LayoutDocument const& document)
    {
        std::vector<Entry> entries{};
        entries.reserve(document.ControlCount());

        ForEachControl(document, [&](Control const& control, size_t, uint32_t)
            {
                Entry entry{};

                entry.Id = control.Id;
                entry.StartsEnabled = !control.StartsDisabled;
                entry.StartsShown = !control.StartsHidden;
                entry.Enabled = entry.StartsEnabled;
                entry.Shown = entry.StartsShown;

                if (!control.Id.empty())
                {
                    auto const previous = std::find_if(m_entries.begin(), m_entries.end(),
                        [&control](Entry const& old) { return old.Id == control.Id; });

                    if (previous != m_entries.end() &&
                        previous->StartsEnabled == entry.StartsEnabled &&
                        previous->StartsShown == entry.StartsShown)
                    {
                        entry.Enabled = previous->Enabled;
                        entry.Shown = previous->Shown;
                    }
                }

                entries.push_back(std::move(entry));
            });

        m_entries = std::move(entries);
    }

    void ControlStateSet::Reset() noexcept
    {
        for (auto& entry : m_entries)
        {
            entry.Enabled = entry.StartsEnabled;
            entry.Shown = entry.StartsShown;
        }
    }

    _Use_decl_annotations_
    bool ControlStateSet::IsEnabled(size_t controlIndex) const noexcept
    {
        return controlIndex >= m_entries.size() || m_entries[controlIndex].Enabled;
    }

    _Use_decl_annotations_
    bool ControlStateSet::IsShown(size_t controlIndex) const noexcept
    {
        return controlIndex >= m_entries.size() || m_entries[controlIndex].Shown;
    }

    bool ControlStateSet::AnyChanged() const noexcept
    {
        return std::any_of(m_entries.begin(), m_entries.end(),
            [](Entry const& entry) { return !entry.Enabled || !entry.Shown; });
    }

    _Use_decl_annotations_
    bool ControlStateSet::Set(uint32_t controlIndex, bool enabledField, bool value) noexcept
    {
        if (controlIndex >= m_entries.size())
        {
            return false;
        }

        auto& field = enabledField ? m_entries[controlIndex].Enabled : m_entries[controlIndex].Shown;

        if (field == value)
        {
            return false;
        }

        field = value;
        return true;
    }

    _Use_decl_annotations_
    std::vector<uint32_t> ControlStateSet::Apply(RemoteCommand const& command)
    {
        std::vector<uint32_t> changed{};

        auto const note = [&changed](uint32_t index)
            {
                if (std::find(changed.begin(), changed.end(), index) == changed.end())
                {
                    changed.push_back(index);
                }
            };

        // A toggle on a group goes by its first member, so a group that is half on comes out
        // all on or all off rather than swapping halves.
        auto const firstEnabled = !command.Controls.empty() && IsEnabled(command.Controls.front());
        auto const firstShown = !command.Controls.empty() && IsShown(command.Controls.front());

        for (auto const index : command.Controls)
        {
            auto moved = false;

            switch (command.Action)
            {
            case RemoteActionKind::EnableControl:  moved = Set(index, true, true); break;
            case RemoteActionKind::DisableControl: moved = Set(index, true, false); break;
            case RemoteActionKind::ToggleEnabled:  moved = Set(index, true, !firstEnabled); break;
            case RemoteActionKind::ShowControl:    moved = Set(index, false, true); break;
            case RemoteActionKind::HideControl:    moved = Set(index, false, false); break;
            case RemoteActionKind::ToggleShown:    moved = Set(index, false, !firstShown); break;
            case RemoteActionKind::ShowOnlyThis:   moved = Set(index, false, true); break;
            default: break;
            }

            if (moved)
            {
                note(index);
            }
        }

        if (command.Action == RemoteActionKind::ShowOnlyThis)
        {
            for (auto const index : command.HideControls)
            {
                if (Set(index, false, false))
                {
                    note(index);
                }
            }
        }

        return changed;
    }

    // ---------------------------------------------------------------- loops

    _Use_decl_annotations_
    void RemoteEchoGuard::NoteSent(int32_t deviceIndex, uint32_t const* words, uint32_t wordCount, uint64_t nowMilliseconds) noexcept
    {
        LearnedBinding learned{};

        if (deviceIndex < 0 || !TryLearnFromWords(words, wordCount, learned) || !CanListenFor(learned.Kind))
        {
            return;
        }

        auto& entry = m_sent[m_next];

        entry.Milliseconds = nowMilliseconds;
        entry.DeviceIndex = deviceIndex;
        entry.Kind = learned.Kind;
        entry.GroupIndex = learned.GroupIndex;
        entry.ChannelIndex = learned.ChannelIndex;
        entry.Number = learned.Number;
        entry.InUse = true;

        m_next = (m_next + 1) % m_sent.size();
    }

    _Use_decl_annotations_
    bool RemoteEchoGuard::IsEcho(int32_t deviceIndex, uint32_t const* words, uint32_t wordCount, uint64_t nowMilliseconds) const noexcept
    {
        LearnedBinding learned{};

        if (deviceIndex < 0 || !TryLearnFromWords(words, wordCount, learned))
        {
            return false;
        }

        return std::any_of(m_sent.begin(), m_sent.end(), [&](Sent const& sent)
            {
                return sent.InUse &&
                    nowMilliseconds >= sent.Milliseconds &&
                    nowMilliseconds - sent.Milliseconds <= EchoMilliseconds &&
                    sent.DeviceIndex == deviceIndex &&
                    sent.Kind == learned.Kind &&
                    sent.GroupIndex == learned.GroupIndex &&
                    sent.ChannelIndex == learned.ChannelIndex &&
                    sent.Number == learned.Number;
            });
    }

    void RemoteEchoGuard::Clear() noexcept
    {
        m_sent = {};
        m_next = 0;
    }

    _Use_decl_annotations_
    bool RemoteRateLimit::Allow(uint64_t nowMilliseconds) noexcept
    {
        if (m_count == 0 || nowMilliseconds < m_windowStart || nowMilliseconds - m_windowStart >= 1000)
        {
            m_windowStart = nowMilliseconds;
            m_count = 0;
        }

        if (m_count >= MaximumPerSecond)
        {
            return false;
        }

        ++m_count;
        return true;
    }

    // ---------------------------------------------------------------- telling the page

    _Use_decl_annotations_
    uint32_t BuildPageFeedback(LayoutDocument const& document, size_t pageIndex, int32_t& deviceIndex, uint32_t* words) noexcept
    {
        deviceIndex = -1;

        if (words == nullptr)
        {
            return 0;
        }

        words[0] = 0;

        try
        {
            auto const& tell = document.Remote.TellPage;

            if (!tell.Enabled || tell.DeviceName.empty() ||
                (tell.Kind != MessageKind::ProgramChange && tell.Kind != MessageKind::ControlChange))
            {
                return 0;
            }

            for (size_t device = 0; device < document.Devices.size(); ++device)
            {
                if (document.Devices[device].Name == tell.DeviceName)
                {
                    deviceIndex = static_cast<int32_t>(device);
                    break;
                }
            }

            auto const position = PagePosition(document, pageIndex);

            if (deviceIndex < 0 || position < 0)
            {
                deviceIndex = -1;
                return 0;
            }

            auto const value = tell.FirstValue + position;

            if (value < 0 || value > 127)
            {
                deviceIndex = -1;
                return 0;
            }

            auto const group = static_cast<uint32_t>(std::clamp(tell.GroupIndex, 0, 15));
            auto const channel = static_cast<uint32_t>(std::clamp(tell.ChannelIndex, 0, 15));

            // MIDI 1.0 protocol, so the number on the wire is exactly the one the page is.
            if (tell.Kind == MessageKind::ProgramChange)
            {
                words[0] = (0x2u << 28) | (group << 24) | (0xCu << 20) | (channel << 16) |
                    (static_cast<uint32_t>(value) << 8);
            }
            else
            {
                words[0] = (0x2u << 28) | (group << 24) | (0xBu << 20) | (channel << 16) |
                    (static_cast<uint32_t>(std::clamp(tell.Number, 0, 127)) << 8) |
                    static_cast<uint32_t>(value);
            }

            return 1;
        }
        catch (...)
        {
            deviceIndex = -1;
            return 0;
        }
    }
}
