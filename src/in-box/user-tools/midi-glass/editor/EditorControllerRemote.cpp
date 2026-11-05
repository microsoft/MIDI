// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// MIDI remote control in the document: the layout's rules, what a page runs when it opens, and
// how a control starts. Deliberately free of pch.h and XAML, like the rest of the editor layer.

#include "EditorController.h"

#include <algorithm>
#include <cmath>

namespace glass
{
    namespace
    {
        bool SameTrigger(_In_ RemoteTrigger const& left, _In_ RemoteTrigger const& right) noexcept
        {
            return left.DeviceName == right.DeviceName &&
                left.Kind == right.Kind &&
                left.GroupIndex == right.GroupIndex &&
                left.ChannelIndex == right.ChannelIndex &&
                left.Number == right.Number &&
                left.Unknown == right.Unknown;
        }

        bool SameRule(_In_ RemoteRule const& left, _In_ RemoteRule const& right) noexcept
        {
            return left.Enabled == right.Enabled &&
                SameTrigger(left.When, right.When) &&
                left.Action == right.Action &&
                left.TargetPageId == right.TargetPageId &&
                left.TargetControlId == right.TargetControlId &&
                left.TargetGroupId == right.TargetGroupId &&
                left.FromMessage == right.FromMessage &&
                std::abs(left.Value - right.Value) < 0.0001 &&
                left.PadFunction == right.PadFunction &&
                left.PadTargets == right.PadTargets &&
                left.KeyRoot == right.KeyRoot &&
                left.Scale == right.Scale &&
                left.SequenceName == right.SequenceName &&
                left.UnrecognizedAction == right.UnrecognizedAction &&
                left.Unknown == right.Unknown;
        }

        bool SameTellPage(_In_ PageFeedback const& left, _In_ PageFeedback const& right) noexcept
        {
            return left.Enabled == right.Enabled &&
                left.DeviceName == right.DeviceName &&
                left.Kind == right.Kind &&
                left.GroupIndex == right.GroupIndex &&
                left.ChannelIndex == right.ChannelIndex &&
                left.Number == right.Number &&
                left.FirstValue == right.FirstValue &&
                left.Unknown == right.Unknown;
        }

        // What the file can hold, whatever the editor handed over.
        RemoteRule Bounded(_In_ RemoteRule rule)
        {
            rule.When.GroupIndex = std::clamp(rule.When.GroupIndex, 0, MaximumGroupCount - 1);
            rule.When.ChannelIndex = rule.When.ChannelIndex == AnyRemoteChannel
                ? AnyRemoteChannel
                : std::clamp(rule.When.ChannelIndex, 0, 15);
            rule.When.Number = rule.When.Number == AnyRemoteNumber
                ? AnyRemoteNumber
                : std::clamp(rule.When.Number, 0, 127);
            rule.Value = std::isfinite(rule.Value) ? std::clamp(rule.Value, 0.0, 1.0) : 1.0;
            rule.KeyRoot = std::clamp(rule.KeyRoot, 0, 11);

            return rule;
        }
    }

    _Use_decl_annotations_
    bool EditorController::SetRemoteControl(RemoteControl const& remote)
    {
        RemoteControl bounded{};

        for (size_t index = 0; index < remote.Rules.size() && index < MaximumRemoteRules; ++index)
        {
            bounded.Rules.push_back(Bounded(remote.Rules[index]));
        }

        bounded.TellPage = remote.TellPage;
        bounded.TellPage.GroupIndex = std::clamp(bounded.TellPage.GroupIndex, 0, MaximumGroupCount - 1);
        bounded.TellPage.ChannelIndex = std::clamp(bounded.TellPage.ChannelIndex, 0, 15);
        bounded.TellPage.Number = std::clamp(bounded.TellPage.Number, 0, 127);
        bounded.TellPage.FirstValue = std::clamp(bounded.TellPage.FirstValue, 0, 127);
        bounded.Unknown = remote.Unknown;

        auto const& current = m_document.Remote;

        // Writing back what is already there would put a step on the undo stack for an edit
        // nobody made.
        auto same = current.Rules.size() == bounded.Rules.size() &&
            SameTellPage(current.TellPage, bounded.TellPage) &&
            current.Unknown == bounded.Unknown;

        for (size_t index = 0; same && index < bounded.Rules.size(); ++index)
        {
            same = SameRule(current.Rules[index], bounded.Rules[index]);
        }

        if (same)
        {
            return false;
        }

        m_document.Remote = std::move(bounded);

        // A number typed a digit at a time is one step to undo.
        CommitCoalesced(EditNames::RemoteControl, L"remoteControl");

        return true;
    }

    _Use_decl_annotations_
    bool EditorController::SetPageOpenSequence(size_t pageIndex, std::wstring const& sequenceName)
    {
        if (pageIndex >= m_document.Pages.size() || m_document.Pages[pageIndex].OpenSequenceName == sequenceName)
        {
            return false;
        }

        if (!sequenceName.empty() && m_document.FindSequence(sequenceName) == nullptr)
        {
            return false;
        }

        m_document.Pages[pageIndex].OpenSequenceName = sequenceName;
        Commit(EditNames::PageProperties);

        return true;
    }

    _Use_decl_annotations_
    bool EditorController::SetControlStartsDisabled(std::wstring const& id, bool startsDisabled)
    {
        auto* const control = MutableControl(id);

        if (control == nullptr || control->StartsDisabled == startsDisabled)
        {
            return false;
        }

        control->StartsDisabled = startsDisabled;
        Commit(EditNames::Properties);

        return true;
    }

    _Use_decl_annotations_
    bool EditorController::SetControlStartsHidden(std::wstring const& id, bool startsHidden)
    {
        auto* const control = MutableControl(id);

        if (control == nullptr || control->StartsHidden == startsHidden)
        {
            return false;
        }

        control->StartsHidden = startsHidden;
        Commit(EditNames::Properties);

        return true;
    }

    _Use_decl_annotations_
    int32_t EditorController::AddRemoteRuleFor(std::wstring const& controlId)
    {
        if (controlId.empty() ||
            m_document.FindControl(controlId) == nullptr ||
            m_document.Remote.Rules.size() >= MaximumRemoteRules)
        {
            return -1;
        }

        RemoteRule rule{};

        rule.Action = RemoteActionKind::EnableControl;
        rule.TargetControlId = controlId;

        // Most layouts have one controller in their table, and a rule on it is what somebody
        // reaching for this wants. Anything else is one pick away.
        if (!m_document.Devices.empty())
        {
            rule.When.DeviceName = m_document.Devices.front().Name;
        }

        m_document.Remote.Rules.push_back(std::move(rule));
        Commit(EditNames::RemoteControl);

        return static_cast<int32_t>(m_document.Remote.Rules.size() - 1);
    }

    _Use_decl_annotations_
    void EditorController::RenameRemoteDevice(std::wstring const& oldName, std::wstring const& newName)
    {
        for (auto& rule : m_document.Remote.Rules)
        {
            if (rule.When.DeviceName == oldName)
            {
                rule.When.DeviceName = newName;
            }
        }

        if (m_document.Remote.TellPage.DeviceName == oldName)
        {
            m_document.Remote.TellPage.DeviceName = newName;
        }
    }
}
