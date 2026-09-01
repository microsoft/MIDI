// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "CommandLineOptions.h"

namespace midibluetoothsetup
{
    namespace
    {
        bool IsSwitch(std::wstring const& argument, std::wstring_view const name) noexcept
        {
            if (argument.size() < 2)
            {
                return false;
            }

            if (argument[0] != L'-' && argument[0] != L'/')
            {
                return false;
            }

            auto body = std::wstring_view{ argument }.substr(1);

            if (!body.empty() && body[0] == L'-')
            {
                body = body.substr(1);
            }

            if (body.size() != name.size())
            {
                return false;
            }

            return ::CompareStringOrdinal(
                body.data(), static_cast<int>(body.size()),
                name.data(), static_cast<int>(name.size()),
                TRUE) == CSTR_EQUAL;
        }
    }

    CommandLineOptions CommandLineOptions::Parse(std::vector<std::wstring> const& arguments) noexcept
    {
        CommandLineOptions options{};

        try
        {
            for (size_t i = 0; i < arguments.size(); i++)
            {
                auto const& argument = arguments[i];

                if (argument.empty())
                {
                    continue;
                }

                if (IsSwitch(argument, L"?") || IsSwitch(argument, L"h") || IsSwitch(argument, L"help"))
                {
                    options.ShowHelp = true;
                    continue;
                }

                if (IsSwitch(argument, L"configfile"))
                {
                    if (i + 1 >= arguments.size() || arguments[i + 1].empty())
                    {
                        options.HasError = true;
                        options.ErrorResourceKey = L"CommandLineMissingConfigFilePath";
                        options.ErrorArgument = argument;

                        return options;
                    }

                    options.ConfigFilePath = arguments[++i];
                    continue;
                }

                options.HasError = true;
                options.ErrorResourceKey = L"CommandLineUnrecognizedArgument";
                options.ErrorArgument = argument;

                return options;
            }
        }
        catch (...)
        {
            options.HasError = true;
            options.ErrorResourceKey = L"CommandLineUnrecognizedArgument";
        }

        return options;
    }

    CommandLineOptions CommandLineOptions::ParseProcessCommandLine() noexcept
    {
        std::vector<std::wstring> arguments{};

        try
        {
            int count{ 0 };

            auto const argv = ::CommandLineToArgvW(::GetCommandLineW(), &count);

            if (argv != nullptr)
            {
                // index 0 is the executable
                for (int i = 1; i < count; i++)
                {
                    arguments.emplace_back(argv[i]);
                }

                ::LocalFree(argv);
            }
        }
        catch (...)
        {
        }

        return Parse(arguments);
    }
}
