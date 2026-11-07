// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "OverviewTile.xaml.h"
#if __has_include("OverviewTile.g.cpp")
#include "OverviewTile.g.cpp"
#endif

#include "StatusPill.xaml.h"
#include "StringResources.h"

namespace res = ::miditroubleshooter::resources;

namespace winrt::miditroubleshooter::implementation
{
    OverviewTile::OverviewTile()
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
    void OverviewTile::Severity(int32_t const value) noexcept
    {
        m_severity = value;
        ApplyState();
    }

    _Use_decl_annotations_
    void OverviewTile::Glyph(winrt::hstring const& value) noexcept
    {
        m_glyph = value;

        try
        {
            IconGlyph().Glyph(value);
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to set the tile glyph.")
    }

    _Use_decl_annotations_
    void OverviewTile::Label(winrt::hstring const& value) noexcept
    {
        m_label = value;

        try
        {
            LabelText().Text(value);
            ApplyAccessibleName();
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to set the tile label.")
    }

    _Use_decl_annotations_
    void OverviewTile::PillSeverity(int32_t const value) noexcept
    {
        m_pillSeverity = value;

        try
        {
            Pill().Severity(value);
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to set the tile pill severity.")
    }

    _Use_decl_annotations_
    void OverviewTile::PillText(winrt::hstring const& value) noexcept
    {
        m_pillText = value;

        try
        {
            Pill().Text(value);
            Pill().Visibility(value.empty() ? xaml::Visibility::Collapsed : xaml::Visibility::Visible);
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to set the tile pill text.")
    }

    _Use_decl_annotations_
    void OverviewTile::Value(winrt::hstring const& value) noexcept
    {
        m_value = value;

        try
        {
            ValueRun().Text(value);
            ApplyAccessibleName();
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to set the tile value.")
    }

    _Use_decl_annotations_
    void OverviewTile::ValueSuffix(winrt::hstring const& value) noexcept
    {
        m_valueSuffix = value;

        try
        {
            // the space keeps the suffix apart from the number without a second text block
            ValueSuffixRun().Text(value.empty() ? winrt::hstring{} : winrt::hstring{ L" " + std::wstring{ value } });
            ApplyAccessibleName();
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to set the tile value suffix.")
    }

    _Use_decl_annotations_
    void OverviewTile::CompactValue(bool const value) noexcept
    {
        m_compactValue = value;

        try
        {
            ValueRun().FontSize(value ? 20.0 : 28.0);
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to set the tile value size.")
    }

    _Use_decl_annotations_
    void OverviewTile::Detail(winrt::hstring const& value) noexcept
    {
        m_detail = value;

        try
        {
            DetailText().Text(value);
            DetailText().Visibility(value.empty() ? xaml::Visibility::Collapsed : xaml::Visibility::Visible);
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to set the tile detail.")
    }

    _Use_decl_annotations_
    void OverviewTile::LinkText(winrt::hstring const& value) noexcept
    {
        m_linkText = value;

        try
        {
            LinkTextBlock().Text(value);
            xaml::Automation::AutomationProperties::SetName(LinkButton(), value);
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to set the tile link.")
    }

    _Use_decl_annotations_
    void OverviewTile::Extra(xaml::UIElement const& value) noexcept
    {
        m_extra = value;

        try
        {
            ExtraPresenter().Content(value);
            ExtraPresenter().Visibility(value == nullptr ? xaml::Visibility::Collapsed : xaml::Visibility::Visible);
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to set the tile content.")
    }

    _Use_decl_annotations_
    winrt::event_token OverviewTile::LinkClick(
        foundation::TypedEventHandler<miditroubleshooter::OverviewTile, foundation::IInspectable> const& handler)
    {
        return m_linkClick.add(handler);
    }

    _Use_decl_annotations_
    void OverviewTile::LinkClick(winrt::event_token const& token) noexcept
    {
        m_linkClick.remove(token);
    }

    _Use_decl_annotations_
    void OverviewTile::OnLinkClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            m_linkClick(*this, nullptr);
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to follow a tile link.")
    }

    void OverviewTile::ApplyState() noexcept
    {
        try
        {
            wchar_t const* state{ L"Info" };

            switch (m_severity)
            {
            case 0:  state = L"Ok"; break;
            case 1:  state = L"Warning"; break;
            case 2:  state = L"Error"; break;
            case 4:  state = L"Neutral"; break;
            case 5:  state = L"NotChecked"; break;
            default: state = L"Info"; break;
            }

            xaml::VisualStateManager::GoToState(*this, state, false);
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to set the tile state.")
    }

    // A screen reader hears the card as one sentence, such as "Transports, 11 loaded".
    void OverviewTile::ApplyAccessibleName() noexcept
    {
        try
        {
            auto const value = m_valueSuffix.empty() ?
                m_value :
                res::FormatString(L"OverviewTileValueFormat", m_value, m_valueSuffix);

            xaml::Automation::AutomationProperties::SetName(*this,
                res::FormatString(L"OverviewTileAccessibleNameFormat", m_label, value));
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to set the tile name.")
    }
}
