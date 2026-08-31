// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "CommandLineOptions.h"

namespace miditroubleshooter
{
    namespace
    {
        std::wstring Lowered(_In_ std::wstring const& value) noexcept
        {
            std::wstring copy{ value };

            std::transform(copy.begin(), copy.end(), copy.begin(),
                [](wchar_t c) { return static_cast<wchar_t>(::towlower(c)); });

            return copy;
        }
    }

    _Use_decl_annotations_
    CommandLineOptions CommandLineOptions::Parse(std::vector<std::wstring> const& arguments) noexcept
    {
        CommandLineOptions options{};

        try
        {
            for (size_t i = 0; i < arguments.size(); i++)
            {
                auto const argument = Lowered(arguments[i]);

                if (argument == L"--help" || argument == L"-h" || argument == L"/?")
                {
                    options.ShowHelp = true;
                }
                else if (argument == L"--noelevate")
                {
                    options.NoElevate = true;
                }
                else if (argument == L"--relaunched")
                {
                    options.Relaunched = true;
                }
                else if (argument == L"--page" && i + 1 < arguments.size())
                {
                    options.StartPage = Lowered(arguments[++i]);
                }
            }
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to parse the command line.")

        return options;
    }

    CommandLineOptions CommandLineOptions::ParseProcessCommandLine() noexcept
    {
        try
        {
            int argumentCount{ 0 };
            wil::unique_hlocal_ptr<PWSTR[]> argumentValues{ ::CommandLineToArgvW(::GetCommandLineW(), &argumentCount) };

            if (argumentValues == nullptr)
            {
                return CommandLineOptions{};
            }

            std::vector<std::wstring> arguments{};

            // index 0 is the executable path
            for (int i = 1; i < argumentCount; i++)
            {
                arguments.emplace_back(argumentValues[i]);
            }

            return Parse(arguments);
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to read the process command line.")

        return CommandLineOptions{};
    }
}
