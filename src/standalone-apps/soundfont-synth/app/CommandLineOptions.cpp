// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "CommandLineOptions.h"

namespace midisoundfontsynth
{
    namespace
    {
        bool IsSwitch(_In_ std::wstring_view argument, _In_ std::wstring_view name) noexcept
        {
            return ::CompareStringOrdinal(
                argument.data(), static_cast<int>(argument.size()),
                name.data(), static_cast<int>(name.size()), TRUE) == CSTR_EQUAL;
        }

        // A packaged app started by its startup task gets no arguments, so the activation kind
        // is the only way it can tell.
        bool WasStartedByStartupTask() noexcept
        {
            try
            {
                UINT32 length{ 0 };

                if (::GetCurrentPackageFullName(&length, nullptr) == APPMODEL_ERROR_NO_PACKAGE)
                {
                    return false;
                }

                auto const args = winrt::Microsoft::Windows::AppLifecycle::AppInstance::GetCurrent().GetActivatedEventArgs();

                return args != nullptr &&
                    args.Kind() == winrt::Microsoft::Windows::AppLifecycle::ExtendedActivationKind::StartupTask;
            }
            catch (...)
            {
                return false;
            }
        }
    }

    CommandLineOptions CommandLineOptions::ParseProcessCommandLine() noexcept
    {
        CommandLineOptions options{};

        try
        {
            int count{ 0 };

            wil::unique_hlocal_ptr<PWSTR[]> arguments{ ::CommandLineToArgvW(::GetCommandLineW(), &count) };

            if (arguments)
            {
                for (int i = 1; i < count; i++)
                {
                    if (IsSwitch(arguments.get()[i], L"--minimized"))
                    {
                        options.StartMinimized = true;
                    }
                }
            }

            options.StartMinimized = options.StartMinimized || WasStartedByStartupTask();
        }
        MIDI_SF2SYNTH_CATCH_AND_LOG(L"Unable to read the command line.")

        return options;
    }
}
