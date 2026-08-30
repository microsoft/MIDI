// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "ToolPaths.h"

namespace miditroubleshooter
{
    namespace
    {
        constexpr wchar_t SdkRuntimeRegistryKey[] = LR"(SOFTWARE\Microsoft\Windows MIDI Services\Desktop App SDK Runtime)";

        std::mutex g_locationsLock;
        ToolLocations g_locations{};
        bool g_locationsResolved{ false };

        std::wstring ExpandEnvironment(_In_ std::wstring const& value) noexcept
        {
            try
            {
                if (value.empty())
                {
                    return {};
                }

                auto const required = ::ExpandEnvironmentStringsW(value.c_str(), nullptr, 0);

                if (required == 0)
                {
                    return value;
                }

                std::wstring expanded(required, L'\0');

                auto const written = ::ExpandEnvironmentStringsW(value.c_str(), expanded.data(), required);

                if (written == 0)
                {
                    return value;
                }

                // the returned count includes the terminator
                expanded.resize(written - 1);

                return expanded;
            }
            catch (...)
            {
                return value;
            }
        }

        std::wstring CombinePath(_In_ std::wstring const& folder, _In_ std::wstring const& leaf) noexcept
        {
            if (folder.empty())
            {
                return leaf;
            }

            auto combined = folder;

            if (combined.back() != L'\\' && combined.back() != L'/')
            {
                combined += L'\\';
            }

            return combined + leaf;
        }

        std::wstring ProgramFilesToolsFolder() noexcept
        {
            try
            {
                wil::unique_cotaskmem_string programFiles;

                if (SUCCEEDED(::SHGetKnownFolderPath(FOLDERID_ProgramFiles, 0, nullptr, programFiles.put())))
                {
                    return CombinePath(programFiles.get(), LR"(Windows MIDI Services\Tools)");
                }
            }
            MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to resolve the Program Files tools folder.")

            return {};
        }

        std::wstring ProgramFilesCollectLogsFolder() noexcept
        {
            try
            {
                wil::unique_cotaskmem_string programFiles;

                if (SUCCEEDED(::SHGetKnownFolderPath(FOLDERID_ProgramFiles, 0, nullptr, programFiles.put())))
                {
                    return CombinePath(programFiles.get(), LR"(Windows MIDI Services\Collect MIDI Logs)");
                }
            }
            MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to resolve the Collect MIDI Logs folder.")

            return {};
        }

        std::wstring RegisteredPath(_In_ wchar_t const* const valueName) noexcept
        {
            try
            {
                auto const value = wil::reg::try_get_value_string(
                    HKEY_LOCAL_MACHINE, SdkRuntimeRegistryKey, valueName);

                if (value.has_value())
                {
                    return ExpandEnvironment(value.value());
                }
            }
            MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to read a registered tool path.")

            return {};
        }

        std::wstring ParentFolder(_In_ std::wstring const& path) noexcept
        {
            auto const separator = path.find_last_of(L"\\/");

            return separator == std::wstring::npos ? std::wstring{} : path.substr(0, separator);
        }

        // Registered path first, then the install convention, then next to this executable so
        // a developer build with everything in one output folder works too.
        std::wstring ResolveTool(
            _In_ wchar_t const* const registryValueName,
            _In_ std::wstring const& fileName) noexcept
        {
            auto const registered = RegisteredPath(registryValueName);

            if (FileExists(registered))
            {
                return registered;
            }

            auto const installed = CombinePath(ProgramFilesToolsFolder(), fileName);

            if (FileExists(installed))
            {
                return installed;
            }

            auto const local = CombinePath(GetExecutableFolder(), fileName);

            if (FileExists(local))
            {
                return local;
            }

            return {};
        }

        void Resolve() noexcept
        {
            ToolLocations locations{};

            try
            {
                locations.MidiDiag = ResolveTool(L"MidiDiag", L"mididiag.exe");
                locations.MidiKsInfo = ResolveTool(L"MidiKsInfo", L"midiksinfo.exe");

                // The registered CollectMidiLogs value points at the script; the profile this
                // app needs lives beside it.
                auto const scriptPath = RegisteredPath(L"CollectMidiLogs");
                auto const scriptFolder = scriptPath.empty() ? std::wstring{} : ParentFolder(scriptPath);

                for (auto const& folder : { scriptFolder, ProgramFilesCollectLogsFolder(), GetExecutableFolder() })
                {
                    auto const candidate = CombinePath(folder, L"providers.wprp");

                    if (FileExists(candidate))
                    {
                        locations.ReproProfile = candidate;
                        break;
                    }
                }

                auto const system32 = GetNativeSystem32Folder();

                for (auto const& pair : {
                        std::pair<std::wstring*, std::wstring>{ &locations.WindowsPerformanceRecorder, L"wpr.exe" },
                        std::pair<std::wstring*, std::wstring>{ &locations.TimeTravelTracer, L"tttracer.exe" },
                        std::pair<std::wstring*, std::wstring>{ &locations.PnpUtil, L"pnputil.exe" },
                        std::pair<std::wstring*, std::wstring>{ &locations.DdoDiag, L"ddodiag.exe" },
                        std::pair<std::wstring*, std::wstring>{ &locations.DxDiag, L"dxdiag.exe" } })
                {
                    auto const candidate = CombinePath(system32, pair.second);

                    if (FileExists(candidate))
                    {
                        *pair.first = candidate;
                    }
                }
            }
            MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to resolve the external tool paths.")

            g_locations = locations;
            g_locationsResolved = true;
        }
    }

    _Use_decl_annotations_
    bool FileExists(std::wstring const& path) noexcept
    {
        if (path.empty())
        {
            return false;
        }

        auto const attributes = ::GetFileAttributesW(path.c_str());

        return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
    }

    std::wstring GetExecutableFolder() noexcept
    {
        try
        {
            wchar_t modulePath[MAX_PATH]{};

            if (::GetModuleFileNameW(nullptr, modulePath, ARRAYSIZE(modulePath)) > 0)
            {
                std::wstring path{ modulePath };

                auto const separator = path.find_last_of(L'\\');

                if (separator != std::wstring::npos)
                {
                    return path.substr(0, separator);
                }
            }
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to resolve the executable folder.")

        return {};
    }

    std::wstring GetNativeSystem32Folder() noexcept
    {
        try
        {
            wchar_t windowsFolder[MAX_PATH]{};

            if (::GetWindowsDirectoryW(windowsFolder, ARRAYSIZE(windowsFolder)) == 0)
            {
                return {};
            }

            // A 32 bit build would be redirected to SysWOW64, and none of the tools this app
            // runs exist there.
            BOOL isWow64{ FALSE };

            if (::IsWow64Process(::GetCurrentProcess(), &isWow64) && isWow64)
            {
                return CombinePath(windowsFolder, L"Sysnative");
            }

            return CombinePath(windowsFolder, L"System32");
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to resolve the system folder.")

        return {};
    }

    ToolLocations GetToolLocations() noexcept
    {
        std::scoped_lock lock{ g_locationsLock };

        if (!g_locationsResolved)
        {
            Resolve();
        }

        return g_locations;
    }

    void RefreshToolLocations() noexcept
    {
        std::scoped_lock lock{ g_locationsLock };

        Resolve();
    }
}
