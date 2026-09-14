// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MainWindow.xaml.h"

#include "StringResources.h"

namespace native = ::midisettings;
namespace res = ::midisettings::resources;

namespace winrt::midisettings::implementation
{
    void MainWindow::RefreshNotificationSettings() noexcept
    {
        try
        {
            // Set without raising Toggled, which would write back what was just read.
            m_updatingNotificationToggles = true;

            auto const enabled = native::NotificationSettings::NotificationsEnabled();

            NotificationsEnabledToggle().IsOn(enabled);
            NotificationsNetworkToggle().IsOn(native::NotificationSettings::NetworkApprovalEnabled());

            // The categories are meaningless while nothing is being shown at all.
            NotificationsNetworkToggle().IsEnabled(enabled);

            auto const forEveryone = native::NotificationSettings::StartsForAllUsers();

            NotificationsMachineStartupBar().IsOpen(forEveryone);

            NotificationsStartupToggle().IsOn(
                forEveryone || native::NotificationSettings::StartsAtSignIn());

            // Nothing here can remove an administrator's machine wide entry.
            NotificationsStartupToggle().IsEnabled(!forEveryone);

            auto const installed = !native::NotificationSettings::AppPath().empty();

            if (!installed)
            {
                NotificationsEnabledToggle().IsEnabled(false);
                NotificationsNetworkToggle().IsEnabled(false);
                NotificationsStartupToggle().IsEnabled(false);

                NotificationsStatusText().Text(
                    res::GetString(L"NotificationsAppMissing"));
            }
            else
            {
                NotificationsEnabledToggle().IsEnabled(true);
                NotificationsStatusText().Text(L"");
            }

            m_updatingNotificationToggles = false;
        }
        catch (...)
        {
            m_updatingNotificationToggles = false;
            LOG_CAUGHT_EXCEPTION();
        }
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnNotificationsClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        try
        {
            RefreshNotificationSettings();

            NotificationsDialog().XamlRoot(Content().XamlRoot());

            co_await NotificationsDialog().ShowAsync();
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to show the notification settings.")
    }

    _Use_decl_annotations_
    void MainWindow::OnNotificationsEnabledToggled(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        if (m_updatingNotificationToggles)
        {
            return;
        }

        try
        {
            auto const enabled = NotificationsEnabledToggle().IsOn();

            native::NotificationSettings::NotificationsEnabled(enabled);

            NotificationsNetworkToggle().IsEnabled(enabled);

            if (enabled)
            {
                // It exited when this was turned off, so it has to be started again.
                native::NotificationSettings::EnsureAppRunning();
            }
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to change the notification setting.")
    }

    _Use_decl_annotations_
    void MainWindow::OnNotificationsNetworkToggled(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        if (m_updatingNotificationToggles)
        {
            return;
        }

        try
        {
            native::NotificationSettings::NetworkApprovalEnabled(NotificationsNetworkToggle().IsOn());
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to change the notification setting.")
    }

    _Use_decl_annotations_
    void MainWindow::OnNotificationsStartupToggled(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        if (m_updatingNotificationToggles)
        {
            return;
        }

        try
        {
            auto const wanted = NotificationsStartupToggle().IsOn();

            if (native::NotificationSettings::TrySetStartsAtSignIn(wanted))
            {
                NotificationsStatusText().Text(L"");

                if (wanted)
                {
                    native::NotificationSettings::EnsureAppRunning();
                }

                return;
            }

            // Put the switch back rather than leave it showing something that did not happen.
            m_updatingNotificationToggles = true;
            NotificationsStartupToggle().IsOn(!wanted);
            m_updatingNotificationToggles = false;

            NotificationsStatusText().Text(
                res::GetString(L"NotificationsStartupChangeFailed"));
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to change the startup setting.")
    }
}
