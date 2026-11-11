// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "StatusPill.xaml.h"
#if __has_include("StatusPill.g.cpp")
#include "StatusPill.g.cpp"
#endif

namespace winrt::miditroubleshooter::implementation
{
    StatusPill::StatusPill()
    {
        InitializeComponent();

        // the state is applied once the template is in place, and again on every change
        Loaded([weak = get_weak()](auto&&, auto&&)
            {
                if (auto strong = weak.get())
                {
                    strong->ApplyState();
                }
            });
    }

    _Use_decl_annotations_
    void StatusPill::Severity(int32_t const value) noexcept
    {
        m_severity = value;
        ApplyState();
    }

    _Use_decl_annotations_
    void StatusPill::Text(winrt::hstring const& value) noexcept
    {
        m_text = value;

        try
        {
            PillText().Text(value);
            xaml::Automation::AutomationProperties::SetName(*this, value);
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to set the status pill text.")
    }

    void StatusPill::ApplyState() noexcept
    {
        try
        {
            wchar_t const* state{ L"Neutral" };

            switch (static_cast<PillSeverity>(m_severity))
            {
            case PillSeverity::Ok:      state = L"Ok"; break;
            case PillSeverity::Warning: state = L"Warning"; break;
            case PillSeverity::Error:   state = L"Error"; break;
            case PillSeverity::Info:    state = L"Info"; break;
            case PillSeverity::Preview: state = L"Preview"; break;
            default:                    state = L"Neutral"; break;
            }

            xaml::VisualStateManager::GoToState(*this, state, false);
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to set the status pill state.")
    }
}
