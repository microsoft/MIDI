// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "CommandLineOptions.h"
#include "loopback_setup_protocol_defs.h"

namespace midiloopbacksetup
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

        // Recognizes "ms-midi-loopback-setup:<path>" and hands back the path. Anything which is
        // not this app's scheme is left for the ordinary switch parsing to reject.
        bool TryGetProtocolPath(std::wstring const& argument, std::wstring& path) noexcept
        {
            constexpr std::wstring_view scheme{ MIDI_LOOPBACK_SETUP_PROTOCOL_SCHEME L":" };

            if (argument.size() <= scheme.size())
            {
                return false;
            }

            if (::CompareStringOrdinal(
                    argument.data(), static_cast<int>(scheme.size()),
                    scheme.data(), static_cast<int>(scheme.size()),
                    TRUE) != CSTR_EQUAL)
            {
                return false;
            }

            path = argument.substr(scheme.size());

            // Shells commonly append a trailing slash to a bare scheme.
            while (!path.empty() && (path.back() == L'/' || path.back() == L'\\'))
            {
                path.pop_back();
            }

            return true;
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

                std::wstring protocolPath{};

                if (TryGetProtocolPath(argument, protocolPath))
                {
                    // An unrecognized path only means a newer notification asked for a page this
                    // build does not have. Opening the app is still the useful thing to do.
                    if (::CompareStringOrdinal(
                            protocolPath.data(), static_cast<int>(protocolPath.size()),
                            MIDI_LOOPBACK_SETUP_PROTOCOL_PATH_FEEDBACK,
                            static_cast<int>(wcslen(MIDI_LOOPBACK_SETUP_PROTOCOL_PATH_FEEDBACK)),
                            TRUE) == CSTR_EQUAL)
                    {
                        options.ShowFeedback = true;
                    }

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
