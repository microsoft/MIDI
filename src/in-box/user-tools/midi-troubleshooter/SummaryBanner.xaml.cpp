// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "SummaryBanner.xaml.h"
#if __has_include("SummaryBanner.g.cpp")
#include "SummaryBanner.g.cpp"
#endif

namespace winrt::miditroubleshooter::implementation
{
    namespace
    {
        // the severity's own glyph, used when no other one was set
        wchar_t const* DefaultGlyph(_In_ int32_t const severity) noexcept
        {
            switch (severity)
            {
            case 0:  return L"\uE73E";
            case 1:  return L"\uE7BA";
            case 2:  return L"\uEA39";
            default: return L"\uE946";
            }
        }

        wchar_t const* StateName(_In_ int32_t const severity) noexcept
        {
            switch (severity)
            {
            case 0:  return L"Ok";
            case 1:  return L"Warning";
            case 2:  return L"Error";
            case 3:  return L"Info";
            default: return L"Neutral";
            }
        }
    }

    SummaryBanner::SummaryBanner()
    {
        InitializeComponent();

        Loaded([weak = get_weak()](auto&&, auto&&)
            {
                if (auto strong = weak.get())
                {
                    strong->ApplyState();
                }
            });
    }

    _Use_decl_annotations_
    void SummaryBanner::Severity(int32_t const value) noexcept
    {
        m_severity = value;
        ApplyState();
    }

    _Use_decl_annotations_
    void SummaryBanner::Title(winrt::hstring const& value) noexcept
    {
        m_title = value;

        try
        {
            TitleText().Text(value);
            xaml::Automation::AutomationProperties::SetName(*this, value);
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to set the summary title.")
    }

    _Use_decl_annotations_
    void SummaryBanner::Message(winrt::hstring const& value) noexcept
    {
        m_message = value;

        try
        {
            MessageText().Text(value);
            MessageText().Visibility(value.empty() ? xaml::Visibility::Collapsed : xaml::Visibility::Visible);
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to set the summary message.")
    }

    _Use_decl_annotations_
    void SummaryBanner::Glyph(winrt::hstring const& value) noexcept
    {
        m_glyph = value;
        ApplyState();
    }

    _Use_decl_annotations_
    void SummaryBanner::Chips(xaml::UIElement const& value) noexcept
    {
        m_chips = value;

        try
        {
            ChipsPresenter().Content(value);
            ChipsPresenter().Visibility(value == nullptr ? xaml::Visibility::Collapsed : xaml::Visibility::Visible);
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to set the summary chips.")
    }

    _Use_decl_annotations_
    void SummaryBanner::Action(xaml::UIElement const& value) noexcept
    {
        m_action = value;

        try
        {
            ActionPresenter().Content(value);
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to set the summary action.")
    }

    void SummaryBanner::ApplyState() noexcept
    {
        try
        {
            BadgeGlyph().Glyph(m_glyph.empty() ? winrt::hstring{ DefaultGlyph(m_severity) } : m_glyph);
            xaml::VisualStateManager::GoToState(*this, StateName(m_severity), false);
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to set the summary state.")
    }
}
