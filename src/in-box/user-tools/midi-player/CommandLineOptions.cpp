// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "CommandLineOptions.h"

namespace midiplayer
{
    namespace
    {
        bool IsSwitch(std::wstring const& argument, wchar_t const* name) noexcept
        {
            return argument == name;
        }
    }

    _Use_decl_annotations_
    CommandLineOptions CommandLineOptions::Parse(std::vector<std::wstring> const& arguments) noexcept
    {
        CommandLineOptions options{};

        try
        {
            for (size_t index = 0; index < arguments.size(); ++index)
            {
                auto const& argument = arguments[index];

                if (argument.empty())
                {
                    continue;
                }

                if (IsSwitch(argument, L"--help") || IsSwitch(argument, L"-?") || IsSwitch(argument, L"/?"))
                {
                    options.ShowHelp = true;
                    continue;
                }

                if (IsSwitch(argument, L"--no-play"))
                {
                    options.AutoPlay = false;
                    continue;
                }

                if (IsSwitch(argument, L"--endpoint") && index + 1 < arguments.size())
                {
                    options.EndpointDeviceId = arguments[++index];
                    continue;
                }

                if (IsSwitch(argument, L"--group") && index + 1 < arguments.size())
                {
                    auto const& value = arguments[++index];

                    try
                    {
                        auto const number = std::stoi(value);

                        if (number >= 1 && number <= 16)
                        {
                            options.GroupNumber = static_cast<uint8_t>(number - 1);
                        }
                    }
                    catch (...)
                    {
                        // an unreadable group is ignored rather than refusing to open the file
                    }

                    continue;
                }

                if (argument.front() == L'-' || argument.front() == L'/')
                {
                    continue;
                }

                // Anything else is a file. Explorer hands over a full path already, but a path
                // typed at a prompt can be relative.
                std::wstring full{};

                try
                {
                    full = std::filesystem::absolute(std::filesystem::path{ argument }).wstring();
                }
                catch (...)
                {
                    full = argument;
                }

                options.Files.push_back(full);
            }
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to read the command line.")

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
                // the first entry is the executable
                for (int index = 1; index < count; ++index)
                {
                    arguments.emplace_back(parsed[index]);
                }
            }
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to read the process command line.")

        return Parse(arguments);
    }
}
