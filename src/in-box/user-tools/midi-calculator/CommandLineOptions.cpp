// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "CommandLineOptions.h"

namespace midicalculator
{
    namespace
    {
        std::wstring Lower(_In_ std::wstring value) noexcept
        {
            while (!value.empty() && (value[0] == L'-' || value[0] == L'/'))
            {
                value.erase(0, 1);
            }

            std::transform(value.begin(), value.end(), value.begin(),
                [](wchar_t ch) { return static_cast<wchar_t>(::towlower(ch)); });

            return value;
        }

        std::optional<CalculatorPage> PageFromName(_In_ std::wstring const& name) noexcept
        {
            if (name == L"tempo" || name == L"beats") return CalculatorPage::Tempo;
            if (name == L"pitch" || name == L"notes" || name == L"note") return CalculatorPage::Pitch;
            if (name == L"midi2" || name == L"ump") return CalculatorPage::Midi2;
            if (name == L"time" || name == L"timestamp" || name == L"windows") return CalculatorPage::WindowsTime;
            if (name == L"history") return CalculatorPage::History;
            return std::nullopt;
        }
    }

    _Use_decl_annotations_
    CommandLineOptions CommandLineOptions::Parse(std::vector<std::wstring> const& arguments) noexcept
    {
        CommandLineOptions options{};

        try
        {
            for (auto const& argument : arguments)
            {
                if (!options.Page.has_value())
                {
                    if (auto const page = PageFromName(Lower(argument)); page.has_value())
                    {
                        options.Page = page;
                        continue;
                    }
                }

                if (!options.Value.empty())
                {
                    options.Value += L' ';
                }

                options.Value += argument;
            }

            // a value without a page means nothing to fill in
            if (!options.Page.has_value())
            {
                options.Value.clear();
            }
        }
        catch (...)
        {
            options = {};
        }

        return options;
    }

    CommandLineOptions CommandLineOptions::ParseProcessCommandLine() noexcept
    {
        try
        {
            int count{ 0 };
            wil::unique_hlocal_ptr<LPWSTR> argv{ ::CommandLineToArgvW(::GetCommandLineW(), &count) };

            std::vector<std::wstring> arguments{};

            for (int i = 1; argv && i < count; ++i)
            {
                arguments.emplace_back(argv.get()[i]);
            }

            return Parse(arguments);
        }
        catch (...)
        {
            return {};
        }
    }
}
