// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "App.xaml.h"

#include "DocumentHandoff.h"
#include "StringResources.h"

namespace
{
    // What another MIDI tool, the installer's file type or Reopen as administrator asked for.
    struct LaunchRequest
    {
        bool Notifications{ false };

        // a page of the Configurations window, or a package for its Import page
        bool Configurations{ false };
        std::wstring Page{};
        std::wstring Package{};
    };

    bool IsSwitch(_In_ std::wstring_view const argument) noexcept
    {
        return argument.size() >= 2 && (argument[0] == L'-' || argument[0] == L'/');
    }

    bool SameName(_In_ std::wstring_view const left, _In_ std::wstring_view const right) noexcept
    {
        return left.size() == right.size() &&
            ::CompareStringOrdinal(
                left.data(), static_cast<int>(left.size()),
                right.data(), static_cast<int>(right.size()),
                TRUE) == CSTR_EQUAL;
    }

    bool EndsWith(_In_ std::wstring_view const text, _In_ std::wstring_view const suffix) noexcept
    {
        return text.size() >= suffix.size() && SameName(text.substr(text.size() - suffix.size()), suffix);
    }

    // Only the names the window has, so nothing else on a command line reaches it.
    std::wstring KnownPage(_In_ std::wstring_view const page)
    {
        for (auto const known : MidiSettingsConfigurationsPages)
        {
            if (SameName(page, known))
            {
                return std::wstring{ known };
            }
        }

        return {};
    }

    // Relative to where this copy was started, which a copy that's already running can't know.
    std::wstring FullPath(_In_ std::wstring const& path)
    {
        auto const length = ::GetFullPathNameW(path.c_str(), 0, nullptr, nullptr);

        if (length == 0)
        {
            return {};
        }

        std::wstring full(length, L'\0');

        auto const written = ::GetFullPathNameW(path.c_str(), length, full.data(), nullptr);

        if (written == 0 || written >= length)
        {
            return {};
        }

        full.resize(written);

        return full;
    }

    // Switches are -name, --name or /name. A package may also come on its own, the way a file
    // type's open command passes it.
    LaunchRequest ReadLaunchRequest() noexcept
    {
        LaunchRequest request{};

        try
        {
            int count{ 0 };

            wil::unique_hlocal_ptr<PWSTR[]> arguments{ ::CommandLineToArgvW(::GetCommandLineW(), &count) };

            if (!arguments)
            {
                return request;
            }

            auto const importPage = std::wstring{ MidiSettingsConfigurationsPages[3] };

            for (int i = 1; i < count; i++)
            {
                std::wstring_view argument{ arguments[i] };

                if (!IsSwitch(argument))
                {
                    if (EndsWith(argument, L".midiconfigpack") || EndsWith(argument, L".midiconfig.json"))
                    {
                        request.Configurations = true;
                        request.Page = importPage;
                        request.Package = FullPath(std::wstring{ argument });
                    }

                    continue;
                }

                argument.remove_prefix(1);

                if (!argument.empty() && argument[0] == L'-')
                {
                    argument.remove_prefix(1);
                }

                // the value after a switch, when there is one
                auto const nextValue = [&]()
                    {
                        return i + 1 < count && !IsSwitch(arguments[i + 1]) ? std::wstring{ arguments[++i] } : std::wstring{};
                    };

                if (SameName(argument, MIDI_SETTINGS_SWITCH_NOTIFICATIONS))
                {
                    request.Notifications = true;
                }
                else if (SameName(argument, MIDI_SETTINGS_SWITCH_CONFIGURATIONS))
                {
                    request.Configurations = true;
                    request.Page = KnownPage(nextValue());
                }
                else if (SameName(argument, MIDI_SETTINGS_SWITCH_IMPORT))
                {
                    auto const path = nextValue();

                    request.Configurations = true;
                    request.Page = importPage;
                    request.Package = path.empty() ? std::wstring{} : FullPath(path);
                }
            }
        }
        catch (...)
        {
            request = {};
        }

        return request;
    }

    WPARAM PageNumber(_In_ std::wstring_view const page) noexcept
    {
        for (size_t index = 0; index < std::size(MidiSettingsConfigurationsPages); index++)
        {
            if (SameName(page, MidiSettingsConfigurationsPages[index]))
            {
                return static_cast<WPARAM>(index);
            }
        }

        return 0;
    }
}

// The XAML compiler emits its own wWinMain; we supply this one so startup stays under our
// control. The apartment must stay STA: an MTA UI thread makes UI Automation fail with
// E_UNEXPECTED and then faults, which would leave the app inaccessible to screen readers.
// Everything that blocks on the MIDI service is run through BackgroundWork instead.
int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    winrt::init_apartment(winrt::apartment_type::single_threaded);

    auto const request = ReadLaunchRequest();

    // A second copy would show the customer two views of one machine-wide configuration, which
    // can disagree.
    if (!::midiapp::SingleInstance::AcquireOrActivateExisting(MIDI_SETTINGS_INSTANCE_KEY))
    {
        // The copy which was already running has just been brought forward. A request to open a
        // dialog is handed over to it rather than dropped, or the customer clicks a button in
        // another tool and nothing appears to happen.
        if (auto const existing = ::midiapp::SingleInstance::FindExistingWindow(MIDI_SETTINGS_INSTANCE_KEY))
        {
            if (request.Notifications)
            {
                ::PostMessageW(
                    existing,
                    ::RegisterWindowMessageW(MIDI_SETTINGS_SHOW_NOTIFICATIONS_MESSAGE_NAME),
                    0,
                    0);
            }

            if (!request.Package.empty())
            {
                // A copy running as administrator doesn't take files from one that isn't, so the
                // customer is told where to open it rather than it quietly going nowhere.
                if (!::midiapp::SendDocumentsToExistingInstance(MIDI_SETTINGS_INSTANCE_KEY, { request.Package }))
                {
                    auto const message = ::midisettings::resources::GetString(L"CfgHandoffFailedMessage");
                    auto const title = ::midisettings::resources::GetString(L"AppTitle");

                    ::MessageBoxW(nullptr, message.c_str(), title.c_str(), MB_OK | MB_ICONINFORMATION | MB_SETFOREGROUND);
                }
            }
            else if (request.Configurations)
            {
                ::PostMessageW(
                    existing,
                    ::RegisterWindowMessageW(MIDI_SETTINGS_SHOW_CONFIGURATIONS_MESSAGE_NAME),
                    PageNumber(request.Page),
                    0);
            }
        }

        return 0;
    }

    ::winrt::midisettings::implementation::App::ShowNotificationsOnLaunch(request.Notifications);

    if (request.Configurations)
    {
        ::winrt::midisettings::implementation::App::ConfigurationsOnLaunch(
            request.Page.empty() ? std::wstring{ MidiSettingsConfigurationsPages[0] } : request.Page,
            request.Package);
    }

    ::winrt::Microsoft::UI::Xaml::Application::Start([](auto&&)
        {
            ::winrt::make<::winrt::midisettings::implementation::App>();
        });

    ::midiapp::SingleInstance::Release();

    return 0;
}
