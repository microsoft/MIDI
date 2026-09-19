// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "CommandLineOptions.h"

namespace midipatchbay
{
    namespace
    {
        bool IsSwitch(_In_ std::wstring_view value, _In_ std::wstring_view name) noexcept
        {
            return ::CompareStringOrdinal(
                value.data(), static_cast<int>(value.size()),
                name.data(), static_cast<int>(name.size()), TRUE) == CSTR_EQUAL;
        }
    }

    CommandLineOptions CommandLineOptions::ParseProcessCommandLine() noexcept
    {
        CommandLineOptions options{};

        try
        {
            int argumentCount{ 0 };

            wil::unique_hlocal_ptr<PWSTR[]> arguments{
                ::CommandLineToArgvW(::GetCommandLineW(), &argumentCount) };

            if (!arguments || argumentCount <= 1)
            {
                return options;
            }

            for (int i = 1; i < argumentCount; i++)
            {
                std::wstring_view const argument{ arguments[i] };

                if (argument.empty())
                {
                    continue;
                }

                if (IsSwitch(argument, L"--help") || IsSwitch(argument, L"-?") || IsSwitch(argument, L"/?"))
                {
                    options.ShowHelp = true;
                }
                else if (IsSwitch(argument, L"--minimized"))
                {
                    options.StartMinimized = true;
                }
                else if (IsSwitch(argument, L"--patch"))
                {
                    if (i + 1 < argumentCount)
                    {
                        options.PatchName = arguments[++i];
                    }
                    else
                    {
                        options.HasError = true;
                        options.ErrorText = L"--patch needs the name of a patch.";
                    }
                }
                else
                {
                    options.HasError = true;
                    options.ErrorText = std::wstring{ L"Unrecognized option: " } + std::wstring{ argument };
                }
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to parse the command line.")

        return options;
    }
}
