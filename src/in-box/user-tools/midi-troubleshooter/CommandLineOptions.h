// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace miditroubleshooter
{
    // The first string a second copy sends when it hands a crash report to the running one, so
    // the running one never mistakes anything else for a crash report.
    inline constexpr wchar_t CrashReportHandoffMarker[] = L"--crash-report";

    struct CommandLineOptions
    {
        bool ShowHelp{ false };

        // Suppresses the automatic elevation prompt at startup. The tool then runs read-only
        // until the customer elevates from the banner. Used for development and for a customer
        // who only wants to look at diagnostics.
        bool NoElevate{ false };

        // Set on the relaunched copy so it never tries to elevate a second time.
        bool Relaunched{ false };

        // Opens on a specific page, so MIDI Settings or a support engineer can send someone
        // straight to the right place.
        std::wstring StartPage{};

        // Opens the DPC latency check page and reads this saved trace. Reading a trace needs no
        // administrator rights, so the tool doesn't ask for them at startup.
        std::wstring LatencyTracePath{};

        // Set by the WER crash handler (midicrashhandler.dll) when another MIDI tool has crashed:
        // the crashed process, its exe name, and the WerFault.exe that's writing its dump. The
        // tool opens on the Crash dumps page and doesn't ask for administrator rights, because a
        // UAC prompt is the last thing someone wants right after an app crashed. Everything here
        // came from another process, so it is checked before it is used.
        DWORD CrashedProcessId{ 0 };
        std::wstring CrashedExecutable{};
        DWORD WerFaultProcessId{ 0 };

        // Set on the elevated copy the Crash dumps page starts to save a MIDI service dump.
        bool CaptureServiceDump{ false };

        bool HasCrashReport() const noexcept { return CrashedProcessId != 0 && !CrashedExecutable.empty(); }

        // The crash options as arguments again, to hand to a copy that's already running or to
        // carry into an elevated copy.
        std::vector<std::wstring> CrashArguments() const;

        static CommandLineOptions Parse(std::vector<std::wstring> const& arguments) noexcept;
        static CommandLineOptions ParseProcessCommandLine() noexcept;
    };
}
