// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Learn: filling in what a control sends by touching the hardware instead of typing numbers.
//
// Touching a control on a controller says five things, not one — which endpoint it arrived on,
// which group, which channel, which kind of message and which number — and a row of check boxes
// decides which of them this capture is allowed to write. Somebody remapping within one device
// locks the endpoint and takes only the number.
//
// Bank learn is the reason this is worth building. Touching eight knobs in order fills eight
// controls in keyboard order, which turns an hour of typing into about a minute.

#include "pch.h"
#include "EditorWindow.xaml.h"

#include "StringResources.h"
#include "ControlFactory.h"

namespace resources = ::midiglass::resources;

namespace winrt::midiglass::implementation
{
    namespace
    {
        // Two captures closer together than this are one gesture. A knob swept across its travel
        // arrives as a hundred messages, and a bank learn must not fill a hundred controls.
        constexpr uint64_t SameGestureMilliseconds = 400;

        bool IsOn(_In_ controls::Primitives::ToggleButton const& toggle) noexcept
        {
            try
            {
                if (toggle == nullptr)
                {
                    return false;
                }

                auto const checked = toggle.IsChecked();

                return checked != nullptr && checked.Value();
            }
            catch (...)
            {
                return false;
            }
        }

        bool IsChecked(_In_ controls::CheckBox const& box) noexcept
        {
            try
            {
                if (box == nullptr)
                {
                    return false;
                }

                auto const checked = box.IsChecked();

                return checked != nullptr && checked.Value();
            }
            catch (...)
            {
                return false;
            }
        }
    }

    _Use_decl_annotations_
    void EditorWindow::SetLearnMode(LearnMode mode)
    {
        if (!m_loaded || m_learnMode == mode)
        {
            return;
        }

        try
        {
            m_learnMode = mode;
            m_learnFilled = 0;
            m_learnHasLast = false;

            auto const previous = m_updatingInspector;
            m_updatingInspector = true;

            LearnOneToggle().IsChecked(mode == LearnMode::One);
            LearnBankToggle().IsChecked(mode == LearnMode::Bank);

            m_updatingInspector = previous;

            LearnAcceptPanel().Visibility(
                mode == LearnMode::Off ? xaml::Visibility::Collapsed : xaml::Visibility::Visible);

            if (mode == LearnMode::Off)
            {
                if (m_player != nullptr)
                {
                    m_player->SetLearning(false);
                }

                UpdateLearnStatus();
                return;
            }

            // Learning needs the connections open, which is what the player does. It does not
            // need Try mode: nothing is sent, and design time and run time stay apart.
            if (m_player == nullptr)
            {
                StartPlayerForLearning();
            }

            if (m_player != nullptr)
            {
                m_player->SetLearning(true);
            }

            if (mode == LearnMode::Bank)
            {
                m_learnBankOrder = ControlsInKeyboardOrder();
            }

            UpdateLearnStatus();
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to change the learn mode.")
    }

    std::vector<std::wstring> EditorWindow::ControlsInKeyboardOrder() const
    {
        std::vector<std::wstring> ids{};

        auto const* const page = m_editor.CurrentPage();

        if (page == nullptr)
        {
            return ids;
        }

        std::vector<glass::Control const*> ordered{};

        for (auto const& control : page->Controls)
        {
            // A control that sends nothing has nothing to learn into.
            if (glass::SendsAnything(control.Kind))
            {
                ordered.push_back(&control);
            }
        }

        std::stable_sort(ordered.begin(), ordered.end(),
            [](glass::Control const* left, glass::Control const* right)
            { return left->KeyboardOrder < right->KeyboardOrder; });

        for (auto const* const control : ordered)
        {
            ids.push_back(control->Id);
        }

        return ids;
    }

    void EditorWindow::UpdateLearnStatus()
    {
        if (!m_loaded)
        {
            return;
        }

        try
        {
            switch (m_learnMode)
            {
            case LearnMode::Off:
                LearnStatusText().Text(resources::GetString(L"LearnIdle"));
                break;

            case LearnMode::One:
                LearnStatusText().Text(resources::GetString(L"LearnWaitingOne"));
                break;

            case LearnMode::Bank:
                LearnStatusText().Text(resources::FormatString(
                    L"LearnWaitingBankFormat",
                    static_cast<int32_t>(m_learnFilled),
                    static_cast<int32_t>(m_learnBankOrder.size())));
                break;
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to describe what learn is doing.")
    }

    _Use_decl_annotations_
    void EditorWindow::OnLearned(glass::LearnedBinding const& learned)
    {
        if (m_learnMode == LearnMode::Off)
        {
            return;
        }

        try
        {
            auto const now = static_cast<uint64_t>(::GetTickCount64());

            // A knob swept across its travel is one gesture, however many messages it sent.
            if (m_learnHasLast &&
                glass::IsSameBinding(m_learnLast, learned) &&
                now - m_learnLastTimestamp < SameGestureMilliseconds)
            {
                m_learnLastTimestamp = now;
                return;
            }

            m_learnLast = learned;
            m_learnLastTimestamp = now;
            m_learnHasLast = true;

            glass::LearnAcceptance accept{};

            accept.Device = IsChecked(LearnAcceptDevice());
            accept.Group = IsChecked(LearnAcceptGroup());
            accept.Channel = IsChecked(LearnAcceptChannel());
            accept.Kind = IsChecked(LearnAcceptKind());
            accept.Number = IsChecked(LearnAcceptNumber());

            // A device that is not in the layout's table cannot be named, and writing an empty
            // name would silently unbind the row.
            if (learned.DeviceName.empty())
            {
                accept.Device = false;
            }

            if (m_learnMode == LearnMode::One)
            {
                if (ApplyLearnedToControl(SelectedLearnTargetId(), learned, accept))
                {
                    SetLearnMode(LearnMode::Off);
                    LearnStatusText().Text(resources::GetString(L"LearnDoneOne"));
                }

                return;
            }

            // Bank: each touch fills the next control in keyboard order, which is the order
            // somebody can see in the outline and fix.
            while (m_learnFilled < m_learnBankOrder.size())
            {
                auto const id = m_learnBankOrder[m_learnFilled];

                ++m_learnFilled;

                if (ApplyLearnedToControl(id, learned, accept))
                {
                    break;
                }
            }

            if (m_learnFilled >= m_learnBankOrder.size())
            {
                SetLearnMode(LearnMode::Off);
                LearnStatusText().Text(resources::GetString(L"LearnDoneBank"));
                return;
            }

            UpdateLearnStatus();
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to use what arrived.")
    }

    std::wstring EditorWindow::SelectedLearnTargetId() const
    {
        auto const* const control = SingleSelectedControl();

        return control == nullptr ? std::wstring{} : control->Id;
    }

    _Use_decl_annotations_
    bool EditorWindow::ApplyLearnedToControl(
        std::wstring const& controlId,
        glass::LearnedBinding const& learned,
        glass::LearnAcceptance const& accept)
    {
        if (controlId.empty())
        {
            return false;
        }

        auto const* const control = m_editor.Document().FindControl(controlId);

        if (control == nullptr || control->Messages.empty())
        {
            return false;
        }

        // The row being edited, or the first one. A control with two rows is unusual and the
        // selected one is what somebody is looking at.
        auto const index = (m_learnMode == LearnMode::One &&
            m_messageIndex >= 0 &&
            m_messageIndex < static_cast<int32_t>(control->Messages.size()))
            ? static_cast<size_t>(m_messageIndex)
            : size_t{ 0 };

        auto message = control->Messages[index];

        glass::ApplyLearned(learned, accept, message);

        if (!m_editor.SetMessage(controlId, index, message))
        {
            return false;
        }

        RefreshMessageList();
        RebuildOutline();
        MarkChanged();

        return true;
    }

    _Use_decl_annotations_
    void EditorWindow::OnLearnOneToggled(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(args);

        if (m_updatingInspector)
        {
            return;
        }

        SetLearnMode(IsOn(sender.try_as<controls::Primitives::ToggleButton>())
            ? LearnMode::One
            : LearnMode::Off);
    }

    _Use_decl_annotations_
    void EditorWindow::OnLearnBankToggled(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(args);

        if (m_updatingInspector)
        {
            return;
        }

        SetLearnMode(IsOn(sender.try_as<controls::Primitives::ToggleButton>())
            ? LearnMode::Bank
            : LearnMode::Off);
    }
}
