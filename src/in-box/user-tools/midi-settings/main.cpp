// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "App.xaml.h"

namespace
{
    // The only switch this app takes. Another MIDI tool uses it to send the customer straight to
    // the notifications dialog rather than describing where to find it.
    bool WantsNotificationsDialog() noexcept
    {
        int count{ 0 };

        wil::unique_hlocal_ptr<PWSTR[]> arguments{ ::CommandLineToArgvW(::GetCommandLineW(), &count) };

        if (!arguments)
        {
            return false;
        }

        constexpr std::wstring_view name{ MIDI_SETTINGS_SWITCH_NOTIFICATIONS };

        for (int i = 1; i < count; i++)
        {
            std::wstring_view argument{ arguments[i] };

            if (argument.size() < 2 || (argument[0] != L'-' && argument[0] != L'/'))
            {
                continue;
            }

            argument.remove_prefix(1);

            if (!argument.empty() && argument[0] == L'-')
            {
                argument.remove_prefix(1);
            }

            if (argument.size() == name.size() &&
                ::CompareStringOrdinal(
                    argument.data(), static_cast<int>(argument.size()),
                    name.data(), static_cast<int>(name.size()),
                    TRUE) == CSTR_EQUAL)
            {
                return true;
            }
        }

        return false;
    }
}

// The XAML compiler emits its own wWinMain; we supply this one so startup stays under our
// control. The apartment must stay STA: an MTA UI thread makes UI Automation fail with
// E_UNEXPECTED and then faults, which would leave the app inaccessible to screen readers.
// Everything that blocks on the MIDI service is run through BackgroundWork instead.
int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    winrt::init_apartment(winrt::apartment_type::single_threaded);

    auto const showNotifications = WantsNotificationsDialog();

    // A second copy would show the customer two views of one machine-wide configuration, which
    // can disagree.
    if (!::midiapp::SingleInstance::AcquireOrActivateExisting(MIDI_SETTINGS_INSTANCE_KEY))
    {
        // The copy which was already running has just been brought forward. A request to open a
        // dialog is handed over to it rather than dropped, or the customer clicks a button in
        // another tool and nothing appears to happen.
        if (showNotifications)
        {
            if (auto const existing = ::midiapp::SingleInstance::FindExistingWindow(MIDI_SETTINGS_INSTANCE_KEY))
            {
                ::PostMessageW(
                    existing,
                    ::RegisterWindowMessageW(MIDI_SETTINGS_SHOW_NOTIFICATIONS_MESSAGE_NAME),
                    0,
                    0);
            }
        }

        return 0;
    }

    ::winrt::midisettings::implementation::App::ShowNotificationsOnLaunch(showNotifications);

    ::winrt::Microsoft::UI::Xaml::Application::Start([](auto&&)
        {
            ::winrt::make<::winrt::midisettings::implementation::App>();
        });

    ::midiapp::SingleInstance::Release();

    return 0;
}
