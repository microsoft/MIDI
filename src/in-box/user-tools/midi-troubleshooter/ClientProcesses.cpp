// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "ClientProcesses.h"
#include "SystemInfo.h"

#include <tlhelp32.h>

namespace miditroubleshooter
{
    namespace
    {
        bool StartsWithNoCase(_In_ std::wstring_view const text, _In_ std::wstring_view const prefix) noexcept
        {
            return text.size() >= prefix.size() &&
                ::CompareStringOrdinal(text.data(), static_cast<int>(prefix.size()),
                    prefix.data(), static_cast<int>(prefix.size()), TRUE) == CSTR_EQUAL;
        }

        bool ContainsNoCase(_In_ std::wstring_view const text, _In_ std::wstring_view const part) noexcept
        {
            for (size_t i = 0; i + part.size() <= text.size(); i++)
            {
                if (StartsWithNoCase(text.substr(i), part))
                {
                    return true;
                }
            }

            return false;
        }

        // Windows.Devices.Midi, Windows.Devices.Midi2, the old app SDK, the Midi2.* client
        // transport, midimap, plus WinMM and the wdmaud2 driver it loads.
        bool IsMidiModule(_In_ std::wstring_view const moduleName) noexcept
        {
            return ContainsNoCase(moduleName, L"midi") ||
                StartsWithNoCase(moduleName, L"winmm") ||
                StartsWithNoCase(moduleName, L"wdmaud");
        }

        bool ReadMidiModules(_In_ DWORD const processId, _Inout_ std::vector<ClientModule>& modules) noexcept
        {
            try
            {
                // Toolhelp returns INVALID_HANDLE_VALUE on failure, which unique_hfile expects
                wil::unique_hfile snapshot{};

                // ERROR_BAD_LENGTH means the module list changed while it was being read
                for (int attempt = 0; attempt < 3 && !snapshot; attempt++)
                {
                    snapshot.reset(::CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, processId));

                    if (!snapshot && ::GetLastError() != ERROR_BAD_LENGTH)
                    {
                        return false;
                    }
                }

                if (!snapshot)
                {
                    return false;
                }

                MODULEENTRY32W entry{};
                entry.dwSize = sizeof(entry);

                if (::Module32FirstW(snapshot.get(), &entry))
                {
                    do
                    {
                        if (IsMidiModule(entry.szModule))
                        {
                            ClientModule module{};
                            module.Path = entry.szExePath;
                            module.Version = GetFileVersionString(module.Path);

                            modules.push_back(std::move(module));
                        }
                    }
                    while (::Module32NextW(snapshot.get(), &entry));
                }

                return true;
            }
            MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to read the modules of a MIDI app.")

            return false;
        }
    }

    std::vector<std::wstring> const& InBoxClientProcessNames() noexcept
    {
        // From the file lists in src\installers. midisoundmodule and midi2-console are built
        // but not shipped, so they are not here.
        static std::vector<std::wstring> const names{
            L"midi.exe",
            L"midi1enum.exe",
            L"midi1monitor.exe",
            L"midi1rtmonitor.exe",
            L"midi2monitor.exe",
            L"midibluetoothsetup.exe",
            L"midiclock.exe",
            L"mididiag.exe",
            L"midiglass.exe",
            L"midikeyboard.exe",
            L"midiksinfo.exe",
            L"midiloopbacksetup.exe",
            L"midinetworksetup.exe",
            L"midinotifications.exe",
            L"midipatchbay.exe",
            L"midiplayer.exe",
            L"midiscratchpad.exe",
            L"midisequencer.exe",
            L"midisettings.exe",
            L"midisysextool.exe",
            L"miditroubleshooter.exe",
        };

        return names;
    }

    std::vector<ClientProcess> FindRunningInBoxClients() noexcept
    {
        std::vector<ClientProcess> processes{};

        try
        {
            wil::unique_hfile snapshot{ ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0) };

            if (!snapshot)
            {
                return processes;
            }

            auto const self = ::GetCurrentProcessId();

            PROCESSENTRY32W entry{};
            entry.dwSize = sizeof(entry);

            if (!::Process32FirstW(snapshot.get(), &entry))
            {
                return processes;
            }

            do
            {
                if (entry.th32ProcessID == self || entry.th32ProcessID == 0)
                {
                    continue;
                }

                std::wstring_view const imageName{ entry.szExeFile };

                auto const& names = InBoxClientProcessNames();

                auto const known = std::any_of(names.begin(), names.end(),
                    [&imageName](std::wstring const& name)
                    {
                        return imageName.size() == name.size() && StartsWithNoCase(imageName, name);
                    });

                if (known)
                {
                    ClientProcess process{};
                    process.ProcessId = entry.th32ProcessID;
                    process.ImageName = entry.szExeFile;
                    process.ModulesRead = ReadMidiModules(process.ProcessId, process.MidiModules);

                    processes.push_back(std::move(process));
                }
            }
            while (::Process32NextW(snapshot.get(), &entry));
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to list the running MIDI apps.")

        return processes;
    }

    _Use_decl_annotations_
    std::wstring DescribeClientProcesses(std::vector<ClientProcess> const& processes) noexcept
    {
        std::wstring report{};

        try
        {
            // Not localized: support engineers and scripts read this file.
            report += L"Windows MIDI Services apps running when the capture started\r\n";
            report += L"===========================================================\r\n\r\n";

            if (processes.empty())
            {
                report += L"none\r\n";
                return report;
            }

            for (auto const& process : processes)
            {
                report += std::format(L"{} (process {})\r\n", process.ImageName, process.ProcessId);

                if (!process.ModulesRead)
                {
                    report += L"  modules could not be read\r\n";
                }
                else if (process.MidiModules.empty())
                {
                    report += L"  no MIDI modules loaded\r\n";
                }

                for (auto const& module : process.MidiModules)
                {
                    report += std::format(L"  {}  {}\r\n",
                        module.Path, module.Version.empty() ? std::wstring{ L"(no version)" } : module.Version);
                }

                report += L"\r\n";
            }
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to describe the running MIDI apps.")

        return report;
    }
}
