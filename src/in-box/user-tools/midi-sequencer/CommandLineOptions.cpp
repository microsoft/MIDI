// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "CommandLineOptions.h"

namespace midisequencer
{
    _Use_decl_annotations_
    CommandLineOptions CommandLineOptions::Parse(std::vector<std::wstring> const& arguments) noexcept
    {
        CommandLineOptions options{};

        try
        {
            for (auto const& argument : arguments)
            {
                if (_wcsicmp(argument.c_str(), L"--sample") == 0)
                {
                    options.Sample = true;
                    continue;
                }

                if (argument.empty() || argument.front() == L'-' || argument.front() == L'/')
                {
                    continue;
                }

                if (!options.File.empty())
                {
                    // One sequence per window. Explorer starts one copy per file when several
                    // are opened at once.
                    break;
                }

                try
                {
                    options.File = std::filesystem::absolute(std::filesystem::path{ argument }).wstring();
                }
                catch (...)
                {
                    options.File = argument;
                }
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to read the command line.")

        return options;
    }

    CommandLineOptions CommandLineOptions::ParseProcessCommandLine() noexcept
    {
        std::vector<std::wstring> arguments{};

        try
        {
            int count = 0;

            wil::unique_hlocal_ptr<PWSTR[]> parsed{ ::CommandLineToArgvW(::GetCommandLineW(), &count) };

            if (parsed)
            {
                for (int index = 1; index < count; ++index)
                {
                    arguments.emplace_back(parsed[index]);
                }
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to read the process command line.")

        return Parse(arguments);
    }
}
