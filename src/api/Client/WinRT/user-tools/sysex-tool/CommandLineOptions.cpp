// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "CommandLineOptions.h"

namespace midisysextool
{
    namespace
    {
        bool IsSwitch(std::wstring const& argument) noexcept
        {
            return !argument.empty() && (argument[0] == L'-' || argument[0] == L'/');
        }

        std::wstring NormalizeSwitch(std::wstring const& argument) noexcept
        {
            std::wstring value{ argument };

            while (!value.empty() && (value[0] == L'-' || value[0] == L'/'))
            {
                value.erase(0, 1);
            }

            std::transform(value.begin(), value.end(), value.begin(),
                [](wchar_t ch) { return static_cast<wchar_t>(::towlower(ch)); });

            return value;
        }

        bool TryParseNumberInRange(std::wstring const& text, uint8_t low, uint8_t high, uint8_t& result) noexcept
        {
            if (text.empty() || text.size() > 3)
            {
                return false;
            }

            unsigned value{ 0 };

            for (auto const ch : text)
            {
                if (ch < L'0' || ch > L'9')
                {
                    return false;
                }

                value = (value * 10) + static_cast<unsigned>(ch - L'0');
            }

            if (value < low || value > high)
            {
                return false;
            }

            result = static_cast<uint8_t>(value);
            return true;
        }

        CommandLineOptions MakeError(std::wstring_view resourceKey, std::wstring_view argument) noexcept
        {
            CommandLineOptions options{};
            options.HasError = true;
            options.ErrorResourceKey = resourceKey;
            options.ErrorArgument = argument;
            return options;
        }
    }

    _Use_decl_annotations_
    CommandLineOptions CommandLineOptions::Parse(std::vector<std::wstring> const& arguments) noexcept
    {
        CommandLineOptions options{};

        try
        {
            std::optional<uint8_t> group{};
            bool sawPositional{ false };

            for (size_t i = 0; i < arguments.size(); i++)
            {
                auto const& argument = arguments[i];

                if (argument.empty())
                {
                    continue;
                }

                if (!IsSwitch(argument))
                {
                    if (sawPositional)
                    {
                        return MakeError(L"CommandLineErrorTooManyEndpoints", argument);
                    }

                    options.EndpointDeviceId = argument;
                    sawPositional = true;
                    continue;
                }

                auto const name = NormalizeSwitch(argument);

                if (name == L"help" || name == L"h" || name == L"?")
                {
                    options.ShowHelp = true;
                    return options;
                }

                if (name == L"group" || name == L"g")
                {
                    if (i + 1 >= arguments.size())
                    {
                        return MakeError(L"CommandLineErrorMissingValue", argument);
                    }

                    uint8_t value{ 0 };

                    if (!TryParseNumberInRange(arguments[++i], 1, 16, value))
                    {
                        return MakeError(L"CommandLineErrorInvalidGroup", arguments[i]);
                    }

                    group = value;
                    continue;
                }

                if (name == L"file" || name == L"f")
                {
                    if (i + 1 >= arguments.size())
                    {
                        return MakeError(L"CommandLineErrorMissingValue", argument);
                    }

                    options.FilePath = arguments[++i];
                    continue;
                }

                return MakeError(L"CommandLineErrorUnknownOption", argument);
            }

            if (group.has_value() && options.EndpointDeviceId.empty())
            {
                return MakeError(L"CommandLineErrorGroupWithoutEndpoint", L"");
            }

            options.GroupNumber = group;
        }
        catch (...)
        {
            return MakeError(L"CommandLineErrorGeneral", L"");
        }

        return options;
    }

    CommandLineOptions CommandLineOptions::ParseProcessCommandLine() noexcept
    {
        std::vector<std::wstring> arguments{};

        try
        {
            int count{ 0 };

            wil::unique_hlocal_ptr<PWSTR[]> argv{ ::CommandLineToArgvW(::GetCommandLineW(), &count) };

            if (argv)
            {
                // skip argv[0], the executable path
                for (int i = 1; i < count; i++)
                {
                    arguments.emplace_back(argv[i]);
                }
            }
        }
        catch (...)
        {
            return MakeError(L"CommandLineErrorGeneral", L"");
        }

        return Parse(arguments);
    }
}
