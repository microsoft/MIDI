// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MainWindow.xaml.h"

#include "StringResources.h"

namespace native = ::miditroubleshooter;
namespace res = ::miditroubleshooter::resources;

namespace winrt::miditroubleshooter::implementation
{
    namespace
    {
        winrt::hstring ApiModeName(_In_ native::ApiMode const mode) noexcept
        {
            switch (mode)
            {
            case native::ApiMode::Legacy:   return res::GetString(L"ApiModeLegacyShortName");
            case native::ApiMode::Hybrid:   return res::GetString(L"ApiModeHybridShortName");
            default:                        return res::GetString(L"ApiModeServicesShortName");
            }
        }
    }

    void MainWindow::LoadApiMode() noexcept
    {
        try
        {
            auto const mode = native::GetCurrentApiMode();

            // The Checked handler is what enables Apply, and setting IsChecked raises it.
            m_settingApiModeSelection = true;

            ApiModeServicesRadio().IsChecked(mode == native::ApiMode::WindowsMidiServices);
            ApiModeLegacyRadio().IsChecked(mode == native::ApiMode::Legacy);
            ApiModeHybridRadio().IsChecked(mode == native::ApiMode::Hybrid);

            m_settingApiModeSelection = false;

            SystemInfoApiModeValue().Text(ApiModeName(mode));

            ApiModeStatusText().Text(res::FormatString(L"ApiModeCurrentFormat", ApiModeName(mode)));

            ApplyApiModeButton().IsEnabled(false);
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to read the current API mode.")
    }

    _Use_decl_annotations_
    void MainWindow::OnApiModeSelectionChanged(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            if (!m_loaded || m_settingApiModeSelection)
            {
                return;
            }

            auto const current = native::GetCurrentApiMode();

            auto const selected =
                ApiModeLegacyRadio().IsChecked().Value() ? native::ApiMode::Legacy :
                ApiModeHybridRadio().IsChecked().Value() ? native::ApiMode::Hybrid :
                native::ApiMode::WindowsMidiServices;

            ApplyApiModeButton().IsEnabled(selected != current);

            ApiModeStatusText().Text(selected == current ?
                res::FormatString(L"ApiModeCurrentFormat", ApiModeName(current)) :
                res::FormatString(L"ApiModePendingFormat", ApiModeName(selected)));
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to handle the API mode selection.")
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnApplyApiModeClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        try
        {
            if (!RequireElevation())
            {
                co_return;
            }

            auto const selected =
                ApiModeLegacyRadio().IsChecked().Value() ? native::ApiMode::Legacy :
                ApiModeHybridRadio().IsChecked().Value() ? native::ApiMode::Hybrid :
                native::ApiMode::WindowsMidiServices;

            auto const confirmed = co_await ConfirmAsync(
                res::GetString(L"ApiModeConfirmTitle"),
                res::FormatString(L"ApiModeConfirmMessageFormat", ApiModeName(selected)));

            if (!confirmed)
            {
                co_return;
            }

            if (!native::TrySetApiMode(selected))
            {
                ApiModeStatusText().Text(res::GetString(L"ApiModeWriteFailed"));
                co_return;
            }

            LoadApiMode();

            ApiModeStatusText().Text(res::GetString(L"ApiModeChangedRestartNeeded"));

            co_await OfferRestartAsync(res::FormatString(L"ApiModeRestartMessageFormat", ApiModeName(selected)));
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to change the API mode.")
    }
}
