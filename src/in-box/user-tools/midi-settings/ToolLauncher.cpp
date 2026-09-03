// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "ToolLauncher.h"

namespace midisettings
{
    namespace
    {
        struct ToolDefinition
        {
            MidiTool Tool;
            wchar_t const* FolderName;
            wchar_t const* ExecutableName;
        };

        // %ProgramFiles%\Windows MIDI Services\Tools\<folder>\<exe>. The folder names are a
        // contract with the installers, so they must match build\build-sdk.ps1 $GuiTools and
        // the transport packages that carry their own setup app.
        constexpr ToolDefinition ToolDefinitions[] =
        {
            { MidiTool::LoopbackSetup,  L"LoopSetup",       L"midiloopbacksetup.exe" },
            { MidiTool::BluetoothSetup, L"BluetoothSetup",  L"midibluetoothsetup.exe" },
            { MidiTool::NetworkSetup,   L"NetworkSetup",    L"midinetworksetup.exe" },
            { MidiTool::SysEx,          L"SysEx",           L"midisysextool.exe" },
            { MidiTool::ScratchPad,     L"ScratchPad",      L"midiscratchpad.exe" },
            { MidiTool::Keyboard,       L"Keyboard",        L"midikeyboard.exe" },
            { MidiTool::Troubleshooter, L"Troubleshooter",  L"miditroubleshooter.exe" },
            { MidiTool::Monitor,        L"Monitor",         L"midi2monitor.exe" },
        };

        constexpr size_t ToolCount = ARRAYSIZE(ToolDefinitions);

        std::array<MidiToolLocation, ToolCount> g_locations{};
        std::wstring g_consolePath{};
        std::once_flag g_firstResolve;

        bool FileExists(std::wstring const& path) noexcept
        {
            return !path.empty() && ::PathFileExistsW(path.c_str());
        }

        std::wstring GetExecutableFolder() noexcept
        {
            try
            {
                wchar_t modulePath[MAX_PATH]{};

                if (::GetModuleFileNameW(nullptr, modulePath, ARRAYSIZE(modulePath)) == 0)
                {
                    return {};
                }

                ::PathCchRemoveFileSpec(modulePath, ARRAYSIZE(modulePath));

                return std::wstring{ modulePath };
            }
            MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to resolve the folder this app is running from.")

            return {};
        }

        std::wstring GetProgramFilesToolsFolder() noexcept
        {
            try
            {
                wil::unique_cotaskmem_string folder;

                // KF_FLAG_DEFAULT gives the native Program Files for this process bitness
                if (FAILED(::SHGetKnownFolderPath(FOLDERID_ProgramFiles, KF_FLAG_DEFAULT, nullptr, folder.put())))
                {
                    return {};
                }

                return std::wstring{ folder.get() } + LR"(\Windows MIDI Services\Tools)";
            }
            MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to resolve the tools folder.")

            return {};
        }

        // Development layout: every tool builds to
        // vsfiles-sdk\out\<tool>\<platform>\<configuration>\<tool>.exe, so a sibling tool is
        // this app's own path with the tool name swapped in. Without this, nothing on the
        // toolbar would work on a machine where the installer has never been run.
        std::wstring BuildOutputSiblingPath(std::wstring const& executableName) noexcept
        {
            try
            {
                auto const folder = GetExecutableFolder();

                if (folder.empty())
                {
                    return {};
                }

                std::filesystem::path const here{ folder };

                // <out>\<tool>\<platform>\<configuration>
                auto const configuration = here.filename();
                auto const platform = here.parent_path().filename();
                auto const outRoot = here.parent_path().parent_path().parent_path();

                if (configuration.empty() || platform.empty() || outRoot.empty())
                {
                    return {};
                }

                auto const toolName = std::filesystem::path{ executableName }.stem();

                auto const candidate = outRoot / toolName / platform / configuration / executableName;

                return candidate.wstring();
            }
            MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to build a development path for a tool.")

            return {};
        }

        std::wstring ResolveTool(ToolDefinition const& definition) noexcept
        {
            auto const toolsFolder = GetProgramFilesToolsFolder();

            if (!toolsFolder.empty())
            {
                auto const installed =
                    toolsFolder + L"\\" + definition.FolderName + L"\\" + definition.ExecutableName;

                if (FileExists(installed))
                {
                    return installed;
                }
            }

            auto const folder = GetExecutableFolder();

            if (!folder.empty())
            {
                auto const sibling = folder + L"\\" + definition.ExecutableName;

                if (FileExists(sibling))
                {
                    return sibling;
                }
            }

            auto const buildOutput = BuildOutputSiblingPath(definition.ExecutableName);

            if (FileExists(buildOutput))
            {
                return buildOutput;
            }

            return {};
        }

        std::wstring ResolveConsole() noexcept
        {
            try
            {
                // The console is the one tool with an installer written registry override.
                auto const registered = wil::reg::try_get_value_string(
                    HKEY_LOCAL_MACHINE,
                    LR"(SOFTWARE\Microsoft\Windows MIDI Services\Desktop App SDK Runtime)",
                    L"MidiConsole");

                if (registered.has_value() && FileExists(registered.value()))
                {
                    return registered.value();
                }
            }
            MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to read the MIDI console registration.")

            auto const toolsFolder = GetProgramFilesToolsFolder();

            if (!toolsFolder.empty())
            {
                auto const installed = toolsFolder + LR"(\Console\midi.exe)";

                if (FileExists(installed))
                {
                    return installed;
                }
            }

            auto const folder = GetExecutableFolder();

            if (!folder.empty())
            {
                auto const sibling = folder + L"\\midi.exe";

                if (FileExists(sibling))
                {
                    return sibling;
                }
            }

            return {};
        }

        bool StartProcess(std::wstring const& path, std::wstring const& arguments, bool const newConsole) noexcept
        {
            try
            {
                if (!FileExists(path))
                {
                    return false;
                }

                std::wstring commandLine{ L"\"" + path + L"\"" };

                if (!arguments.empty())
                {
                    commandLine += L" ";
                    commandLine += arguments;
                }

                std::wstring workingDirectory{ path };
                workingDirectory.resize(workingDirectory.find_last_of(L'\\'));

                STARTUPINFOW startupInfo{};
                startupInfo.cb = sizeof(startupInfo);

                PROCESS_INFORMATION processInfo{};

                // CreateProcess writes to the command line buffer, so it cannot be a literal
                if (!::CreateProcessW(
                    path.c_str(),
                    commandLine.data(),
                    nullptr,
                    nullptr,
                    FALSE,
                    newConsole ? CREATE_NEW_CONSOLE : 0,
                    nullptr,
                    workingDirectory.c_str(),
                    &startupInfo,
                    &processInfo))
                {
                    LOG_LAST_ERROR();
                    return false;
                }

                ::CloseHandle(processInfo.hThread);
                ::CloseHandle(processInfo.hProcess);

                return true;
            }
            MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to start a MIDI tool.")

            return false;
        }
    }

    void RefreshToolLocations() noexcept
    {
        try
        {
            for (size_t i = 0; i < ToolCount; i++)
            {
                auto const& definition = ToolDefinitions[i];

                g_locations[i].Tool = definition.Tool;
                g_locations[i].FullPath = ResolveTool(definition);
                g_locations[i].Installed = !g_locations[i].FullPath.empty();
            }

            g_consolePath = ResolveConsole();
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to locate the MIDI tools.")
    }

    _Use_decl_annotations_
    MidiToolLocation const& GetToolLocation(MidiTool tool) noexcept
    {
        std::call_once(g_firstResolve, []() noexcept { RefreshToolLocations(); });

        auto const index = static_cast<size_t>(tool);

        static MidiToolLocation const empty{};

        return index < ToolCount ? g_locations[index] : empty;
    }

    _Use_decl_annotations_
    bool LaunchTool(MidiTool tool) noexcept
    {
        auto const& location = GetToolLocation(tool);

        return StartProcess(location.FullPath, {}, false);
    }

    std::wstring GetMidiConsolePath() noexcept
    {
        std::call_once(g_firstResolve, []() noexcept { RefreshToolLocations(); });

        return g_consolePath;
    }

    bool IsMonitoringAvailable() noexcept
    {
        return GetToolLocation(MidiTool::Monitor).Installed || !GetMidiConsolePath().empty();
    }

    _Use_decl_annotations_
    bool LaunchMonitorForEndpoint(winrt::hstring const& endpointDeviceId) noexcept
    {
        if (endpointDeviceId.empty())
        {
            return false;
        }

        auto const& monitor = GetToolLocation(MidiTool::Monitor);

        std::wstring const quotedId = L"\"" + std::wstring{ endpointDeviceId } + L"\"";

        if (monitor.Installed && StartProcess(monitor.FullPath, quotedId, false))
        {
            return true;
        }

        // The console monitor is interactive, so it needs a console window of its own.
        return StartProcess(GetMidiConsolePath(), L"endpoint " + quotedId + L" monitor", true);
    }
}
