// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================
// MIDI Glass phase 0 spike. Nothing here ships.

#include "pch.h"
#include "SpikeOptions.h"

namespace gspike
{
    namespace
    {
        bool TryParseUInt(std::wstring_view text, uint32_t& value)
        {
            if (text.empty())
            {
                return false;
            }

            uint64_t accumulated = 0;

            for (wchar_t c : text)
            {
                if (c < L'0' || c > L'9')
                {
                    return false;
                }

                accumulated = accumulated * 10 + static_cast<uint64_t>(c - L'0');

                if (accumulated > 0xFFFFFFFFull)
                {
                    return false;
                }
            }

            value = static_cast<uint32_t>(accumulated);
            return true;
        }
    }

    SpikeOptions SpikeOptions::ParseProcessCommandLine()
    {
        SpikeOptions options{};

        int count = 0;
        wil::unique_hlocal_ptr<PWSTR[]> argv{ CommandLineToArgvW(GetCommandLineW(), &count) };

        if (!argv)
        {
            return options;
        }

        for (int i = 1; i < count; i++)
        {
            const std::wstring_view arg{ argv[i] };
            const bool hasNext = (i + 1) < count;
            const std::wstring_view next = hasNext ? std::wstring_view{ argv[i + 1] } : std::wstring_view{};

            if (arg == L"--autorun")
            {
                options.AutoRun = true;
            }
            else if (arg == L"--no-midi")
            {
                options.SkipMidi = true;
            }
            else if (arg == L"--mode" && hasNext)
            {
                if (next == L"xaml" || next == L"composition" || next == L"hybrid")
                {
                    options.Mode = next;
                }
                else
                {
                    options.ParseError = std::format(L"Unknown mode '{}'.", next);
                }

                i++;
            }
            else if (arg == L"--controls" && hasNext)
            {
                if (!TryParseUInt(next, options.ControlCount) || options.ControlCount == 0 || options.ControlCount > 5000)
                {
                    options.ParseError = std::format(L"Bad control count '{}'.", next);
                    options.ControlCount = 200;
                }

                i++;
            }
            else if (arg == L"--animate" && hasNext)
            {
                if (!TryParseUInt(next, options.AnimatedCount))
                {
                    options.ParseError = std::format(L"Bad animate count '{}'.", next);
                    options.AnimatedCount = 12;
                }

                i++;
            }
            else if (arg == L"--seconds" && hasNext)
            {
                uint32_t seconds = 0;

                if (TryParseUInt(next, seconds) && seconds > 0 && seconds <= 600)
                {
                    options.Seconds = static_cast<double>(seconds);
                }
                else
                {
                    options.ParseError = std::format(L"Bad duration '{}'.", next);
                }

                i++;
            }
            else if (arg == L"--endpoint" && hasNext)
            {
                options.EndpointDeviceId = next;
                i++;
            }
            else if (arg == L"--out" && hasNext)
            {
                options.OutputPath = next;
                i++;
            }
            else
            {
                options.ParseError = std::format(L"Unknown argument '{}'.", arg);
            }
        }

        if (options.AnimatedCount > options.ControlCount)
        {
            options.AnimatedCount = options.ControlCount;
        }

        return options;
    }
}
