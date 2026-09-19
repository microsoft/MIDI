// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "SettingsAppLauncher.h"
#include "SingleInstance.h"

#include "..\midi-settings\settings_app_launch_defs.h"

#include <pathcch.h>
#include <shlobj.h>
#include <filesystem>

namespace midiapp
{
    namespace
    {
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
                    MIDI_SETTINGS_INSTALLED_FOLDER + L"\\" + MIDI_SETTINGS_EXECUTABLE_NAME;
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

                auto const toolName = std::filesystem::path{ MIDI_SETTINGS_EXECUTABLE_NAME }.stem();

                return (outRoot / toolName / platform / configuration / MIDI_SETTINGS_EXECUTABLE_NAME).wstring();
            }
            catch (...)
            {
            }

            return {};
        }

        std::wstring ResolveSettingsApp() noexcept
        {
            auto const installed = GetInstalledPath();

            if (FileExists(installed))
            {
                return installed;
            }

            auto const folder = GetExecutableFolder();

            if (!folder.empty())
            {
                auto const sibling = folder + L"\\" + MIDI_SETTINGS_EXECUTABLE_NAME;

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
    }

    bool IsSettingsAppAvailable() noexcept
    {
        // Deliberately re-resolved every time, so installing the tools while this app is open
        // does not need a restart to make the button work.
        return !ResolveSettingsApp().empty();
    }

    SettingsAppRequestResult ShowSettingsNotifications() noexcept
    {
        try
        {
            if (auto const existing = SingleInstance::FindExistingWindow(MIDI_SETTINGS_INSTANCE_KEY))
            {
                auto const message =
                    ::RegisterWindowMessageW(MIDI_SETTINGS_SHOW_NOTIFICATIONS_MESSAGE_NAME);

                if (message != 0 && ::PostMessageW(existing, message, 0, 0))
                {
                    ::SetForegroundWindow(existing);

                    return SettingsAppRequestResult::Shown;
                }

                // A running copy which is elevated cannot be posted to from here. Starting a new
                // one below fails the same way, so say so rather than appearing to do nothing.
                return SettingsAppRequestResult::Failed;
            }

            auto const path = ResolveSettingsApp();

            if (path.empty())
            {
                return SettingsAppRequestResult::NotInstalled;
            }

            auto const directory = std::filesystem::path{ path }.parent_path().wstring();
            std::wstring const parameters{ L"--" MIDI_SETTINGS_SWITCH_NOTIFICATIONS };

            SHELLEXECUTEINFOW info{};

            info.cbSize = sizeof(info);
            info.fMask = SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI;
            info.lpVerb = L"runas";
            info.lpFile = path.c_str();
            info.lpParameters = parameters.c_str();
            info.lpDirectory = directory.c_str();
            info.nShow = SW_SHOWNORMAL;

            if (::ShellExecuteExW(&info))
            {
                return SettingsAppRequestResult::Shown;
            }

            return ::GetLastError() == ERROR_CANCELLED ?
                SettingsAppRequestResult::Declined :
                SettingsAppRequestResult::Failed;
        }
        catch (...)
        {
        }

        return SettingsAppRequestResult::Failed;
    }
}
