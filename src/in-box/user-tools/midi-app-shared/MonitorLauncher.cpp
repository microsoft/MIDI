// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MonitorLauncher.h"

#include <pathcch.h>
#include <shlobj.h>
#include <filesystem>

namespace midiapp
{
    namespace
    {
        constexpr wchar_t MonitorExecutableName[] = L"midi2monitor.exe";

        // %ProgramFiles%\Windows MIDI Services\Tools\Monitor. The folder name is a contract with
        // the installers, so it must match build\build-sdk.ps1 $GuiTools.
        constexpr wchar_t MonitorInstalledFolder[] = L"Monitor";

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
            catch (...)
            {
            }

            return {};
        }

        std::wstring GetInstalledPath() noexcept
        {
            try
            {
                wil::unique_cotaskmem_string folder;

                // KF_FLAG_DEFAULT gives the native Program Files for this process bitness
                if (FAILED(::SHGetKnownFolderPath(FOLDERID_ProgramFiles, KF_FLAG_DEFAULT, nullptr, folder.put())))
                {
                    return {};
                }

                return std::wstring{ folder.get() } + LR"(\Windows MIDI Services\Tools\)" +
                    MonitorInstalledFolder + L"\\" + MonitorExecutableName;
            }
            catch (...)
            {
            }

            return {};
        }

        // Development layout: every tool builds to
        // vsfiles-sdk\out\<tool>\<platform>\<configuration>\<tool>.exe, so a sibling tool is
        // this app's own path with the tool name swapped in.
        std::wstring GetBuildOutputSiblingPath() noexcept
        {
            try
            {
                auto const folder = GetExecutableFolder();

                if (folder.empty())
                {
                    return {};
                }

                std::filesystem::path const here{ folder };

                auto const configuration = here.filename();
                auto const platform = here.parent_path().filename();
                auto const outRoot = here.parent_path().parent_path().parent_path();

                if (configuration.empty() || platform.empty() || outRoot.empty())
                {
                    return {};
                }

                auto const toolName = std::filesystem::path{ MonitorExecutableName }.stem();

                return (outRoot / toolName / platform / configuration / MonitorExecutableName).wstring();
            }
            catch (...)
            {
            }

            return {};
        }

        std::wstring ResolveMonitor() noexcept
        {
            auto const installed = GetInstalledPath();

            if (FileExists(installed))
            {
                return installed;
            }

            auto const folder = GetExecutableFolder();

            if (!folder.empty())
            {
                auto const sibling = folder + L"\\" + MonitorExecutableName;

                if (FileExists(sibling))
                {
                    return sibling;
                }
            }

            auto const buildOutput = GetBuildOutputSiblingPath();

            if (FileExists(buildOutput))
            {
                return buildOutput;
            }

            return {};
        }

        bool StartProcess(std::wstring const& path, std::wstring const& arguments) noexcept
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
                    0,
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
            catch (...)
            {
            }

            return false;
        }
    }

    bool IsMonitorAvailable() noexcept
    {
        // Deliberately re-resolved every time, so installing the tools while this app is open
        // does not need a restart to make the button work.
        return !ResolveMonitor().empty();
    }

    _Use_decl_annotations_
    bool LaunchMonitorForEndpoint(winrt::hstring const& endpointDeviceId) noexcept
    {
        if (endpointDeviceId.empty())
        {
            return false;
        }

        // The monitor takes the endpoint device id as its one positional argument.
        return StartProcess(ResolveMonitor(), L"\"" + std::wstring{ endpointDeviceId } + L"\"");
    }
}
