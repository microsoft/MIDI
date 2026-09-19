// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MainWindow.xaml.h"

#include "App.xaml.h"
#include "Elevation.h"
#include "StringResources.h"

namespace native = ::midisettings;
namespace res = ::midisettings::resources;

namespace
{
    constexpr UINT_PTR LaunchRequestSubclassId{ 1 };

    // A registered message has the same value in every process on the desktop, so both sides
    // resolve it by name rather than agreeing on a number.
    UINT ShowNotificationsMessage() noexcept
    {
        static UINT const message =
            ::RegisterWindowMessageW(MIDI_SETTINGS_SHOW_NOTIFICATIONS_MESSAGE_NAME);

        return message;
    }
}

namespace winrt::midisettings::implementation
{
    void MainWindow::StartListeningForLaunchRequests() noexcept
    {
        try
        {
            auto const window = m_chrome.WindowHandle();

            if (window == nullptr || ShowNotificationsMessage() == 0)
            {
                return;
            }

            if (::SetWindowSubclass(
                    window,
                    &MainWindow::LaunchRequestSubclassProcedure,
                    LaunchRequestSubclassId,
                    reinterpret_cast<DWORD_PTR>(this)))
            {
                m_launchRequestWindow = window;
            }
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to listen for requests from the other MIDI tools.")
    }

    void MainWindow::StopListeningForLaunchRequests() noexcept
    {
        if (m_launchRequestWindow == nullptr)
        {
            return;
        }

        ::RemoveWindowSubclass(
            m_launchRequestWindow,
            &MainWindow::LaunchRequestSubclassProcedure,
            LaunchRequestSubclassId);

        m_launchRequestWindow = nullptr;
    }

    _Use_decl_annotations_
    LRESULT CALLBACK MainWindow::LaunchRequestSubclassProcedure(
        HWND window,
        UINT message,
        WPARAM wparam,
        LPARAM lparam,
        UINT_PTR idSubclass,
        DWORD_PTR referenceData) noexcept
    {
        UNREFERENCED_PARAMETER(idSubclass);

        if (message == ShowNotificationsMessage())
        {
            // The subclass is removed while the window is still closing, so this pointer is only
            // ever reached while the object behind it is alive.
            auto const self = reinterpret_cast<MainWindow*>(referenceData);

            if (self != nullptr && !self->m_closing)
            {
                ::SetForegroundWindow(window);

                self->OnNotificationsClick(nullptr, xaml::RoutedEventArgs{ nullptr });
            }

            return 0;
        }

        return ::DefSubclassProc(window, message, wparam, lparam);
    }

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

            auto const elevated = ::winrt::midisettings::implementation::App::IsElevated();
            auto const forEveryone = native::NotificationSettings::StartsForAllUsers();

            NotificationsAllUsersToggle().IsOn(forEveryone);
            NotificationsAllUsersToggle().IsEnabled(elevated);

            NotificationsMachineStartupBar().IsOpen(!elevated);

            NotificationsStartupToggle().IsOn(
                forEveryone || native::NotificationSettings::StartsAtSignIn());

            // The per-user entry changes nothing while the machine wide one already starts it.
            NotificationsStartupToggle().IsEnabled(!forEveryone);

            auto const installed = !native::NotificationSettings::AppPath().empty();

            if (!installed)
            {
                NotificationsEnabledToggle().IsEnabled(false);
                NotificationsNetworkToggle().IsEnabled(false);
                NotificationsStartupToggle().IsEnabled(false);
                NotificationsAllUsersToggle().IsEnabled(false);

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

    _Use_decl_annotations_
    void MainWindow::OnNotificationsAllUsersToggled(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        if (m_updatingNotificationToggles)
        {
            return;
        }

        try
        {
            auto const wanted = NotificationsAllUsersToggle().IsOn();

            if (native::NotificationSettings::TrySetStartsForAllUsers(wanted))
            {
                NotificationsStatusText().Text(L"");

                if (wanted)
                {
                    native::NotificationSettings::EnsureAppRunning();
                }

                // The per-user switch is only meaningful when the machine wide one is off.
                RefreshNotificationSettings();

                return;
            }

            m_updatingNotificationToggles = true;
            NotificationsAllUsersToggle().IsOn(!wanted);
            m_updatingNotificationToggles = false;

            NotificationsStatusText().Text(
                res::GetString(L"NotificationsAllUsersChangeFailed"));
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to change the startup setting for everyone.")
    }

    _Use_decl_annotations_
    void MainWindow::OnNotificationsRestartElevatedClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            if (native::TryRelaunchElevated())
            {
                Close();
            }
            else
            {
                NotificationsStatusText().Text(res::GetString(L"ElevationDeclined"));
            }
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to relaunch with administrator rights.")
    }
}
