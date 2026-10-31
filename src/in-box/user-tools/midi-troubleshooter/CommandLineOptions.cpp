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

        // A process id written in decimal and nothing else. Zero means "not given".
        DWORD ParseProcessId(_In_ std::wstring const& value) noexcept
        {
            if (value.empty() || value.size() > 10)
            {
                return 0;
            }

            uint64_t result{ 0 };

            for (auto const character : value)
            {
                if (character < L'0' || character > L'9')
                {
                    return 0;
                }

                result = result * 10 + static_cast<uint64_t>(character - L'0');
            }

            return result <= MAXDWORD ? static_cast<DWORD>(result) : 0;
        }

        // The name of an exe, and nothing that could make it a path: it is shown on screen and
        // used to find a file in the dump folder.
        bool IsPlainExecutableName(_In_ std::wstring const& value) noexcept
        {
            constexpr size_t MaximumLength = 64;
            constexpr std::wstring_view Extension{ L".exe" };

            if (value.size() <= Extension.size() || value.size() > MaximumLength)
            {
                return false;
            }

            if (Lowered(value.substr(value.size() - Extension.size())) != Extension)
            {
                return false;
            }

            return std::all_of(value.begin(), value.end(), [](wchar_t const c)
                {
                    return (c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') || (c >= L'0' && c <= L'9') ||
                        c == L'.' || c == L'-' || c == L'_';
                }) && value.front() != L'.';
        }
    }

    std::vector<std::wstring> CommandLineOptions::CrashArguments() const
    {
        std::vector<std::wstring> arguments{};

        if (!HasCrashReport())
        {
            return arguments;
        }

        arguments.emplace_back(L"--crashed-pid");
        arguments.emplace_back(std::to_wstring(CrashedProcessId));
        arguments.emplace_back(L"--crashed-exe");
        arguments.emplace_back(CrashedExecutable);

        if (WerFaultProcessId != 0)
        {
            arguments.emplace_back(L"--werfault-pid");
            arguments.emplace_back(std::to_wstring(WerFaultProcessId));
        }

        return arguments;
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
                else if (argument == L"--latency-trace" && i + 1 < arguments.size())
                {
                    // a path, so its case is kept
                    options.LatencyTracePath = arguments[++i];
                }
                else if (argument == L"--crashed-pid" && i + 1 < arguments.size())
                {
                    options.CrashedProcessId = ParseProcessId(arguments[++i]);
                }
                else if (argument == L"--crashed-exe" && i + 1 < arguments.size())
                {
                    auto const& name = arguments[++i];

                    if (IsPlainExecutableName(name))
                    {
                        options.CrashedExecutable = name;
                    }
                }
                else if (argument == L"--werfault-pid" && i + 1 < arguments.size())
                {
                    options.WerFaultProcessId = ParseProcessId(arguments[++i]);
                }
                else if (argument == L"--capture-service-dump")
                {
                    options.CaptureServiceDump = true;
                }
            }

            // a crash report that's missing either half isn't one
            if (!options.HasCrashReport())
            {
                options.CrashedProcessId = 0;
                options.CrashedExecutable.clear();
                options.WerFaultProcessId = 0;
            }
            else
            {
                options.StartPage = L"crashdumps";
            }

            if (options.CaptureServiceDump)
            {
                options.StartPage = L"crashdumps";
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
