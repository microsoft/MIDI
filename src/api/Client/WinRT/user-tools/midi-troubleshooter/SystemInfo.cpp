// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "SystemInfo.h"
#include "ToolPaths.h"

namespace miditroubleshooter
{
    namespace
    {
        std::wstring ArchitectureName(_In_ WORD const architecture) noexcept
        {
            switch (architecture)
            {
            case PROCESSOR_ARCHITECTURE_AMD64:  return L"x64";
            case PROCESSOR_ARCHITECTURE_ARM64:  return L"Arm64";
            case PROCESSOR_ARCHITECTURE_ARM:    return L"Arm32";
            case PROCESSOR_ARCHITECTURE_INTEL:  return L"x86";
            default:                            return L"Unknown";
            }
        }

        // The Win32 version APIs lie to an unmanifested caller, so this goes to ntdll for the
        // real build number, exactly as mididiag does.
        std::wstring OperatingSystemVersion() noexcept
        {
            try
            {
                RTL_OSVERSIONINFOW versionInfo{};
                versionInfo.dwOSVersionInfoSize = sizeof(versionInfo);

                auto const ntdll = ::GetModuleHandleW(L"ntdll.dll");

                if (ntdll != nullptr)
                {
                    using RtlGetVersionFunction = NTSTATUS(WINAPI*)(PRTL_OSVERSIONINFOW);

                    auto const rtlGetVersion =
                        reinterpret_cast<RtlGetVersionFunction>(
                            reinterpret_cast<void*>(::GetProcAddress(ntdll, "RtlGetVersion")));

                    if (rtlGetVersion != nullptr && rtlGetVersion(&versionInfo) == 0)
                    {
                        // The UBR is the fourth part of what Windows itself calls the build,
                        // and support cases turn on it, so it is worth the extra registry read.
                        auto const ubr = wil::reg::try_get_value_dword(
                            HKEY_LOCAL_MACHINE,
                            LR"(SOFTWARE\Microsoft\Windows NT\CurrentVersion)",
                            L"UBR");

                        if (ubr.has_value())
                        {
                            return std::format(L"{}.{}.{}.{}",
                                versionInfo.dwMajorVersion,
                                versionInfo.dwMinorVersion,
                                versionInfo.dwBuildNumber,
                                ubr.value());
                        }

                        return std::format(L"{}.{}.{}",
                            versionInfo.dwMajorVersion,
                            versionInfo.dwMinorVersion,
                            versionInfo.dwBuildNumber);
                    }
                }
            }
            MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to read the operating system version.")

            return L"Unknown";
        }

        std::wstring OperatingSystemEdition() noexcept
        {
            try
            {
                auto const productName = wil::reg::try_get_value_string(
                    HKEY_LOCAL_MACHINE,
                    LR"(SOFTWARE\Microsoft\Windows NT\CurrentVersion)",
                    L"ProductName");

                auto const displayVersion = wil::reg::try_get_value_string(
                    HKEY_LOCAL_MACHINE,
                    LR"(SOFTWARE\Microsoft\Windows NT\CurrentVersion)",
                    L"DisplayVersion");

                std::wstring edition = productName.has_value() ? productName.value() : std::wstring{};

                // Windows 11 still writes "Windows 10" here, which reads as a fault to a
                // customer. The edition and the build number below it are the useful parts.
                for (auto const* const prefix : { L"Windows 11 ", L"Windows 10 ", L"Windows " })
                {
                    std::wstring_view const view{ prefix };

                    if (edition.starts_with(view))
                    {
                        edition.erase(0, view.size());
                        break;
                    }
                }

                if (displayVersion.has_value() && !displayVersion.value().empty())
                {
                    edition += edition.empty() ? L"" : L" ";
                    edition += displayVersion.value();
                }

                return edition;
            }
            MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to read the Windows edition.")

            return {};
        }

        std::wstring CurrentCultureName() noexcept
        {
            wchar_t buffer[LOCALE_NAME_MAX_LENGTH]{};

            if (::GetUserDefaultLocaleName(buffer, ARRAYSIZE(buffer)) > 0)
            {
                return buffer;
            }

            return L"Unknown";
        }

        // The Windows App SDK version people quote is the framework package version, and the
        // package folder name is the only place it appears verbatim. Microsoft.UI.Xaml.dll's own
        // file version is a different, internal number.
        std::wstring WindowsAppSdkVersion() noexcept
        {
            try
            {
                for (auto const* const moduleName : {
                        L"Microsoft.UI.Xaml.dll",
                        L"Microsoft.WindowsAppRuntime.dll",
                        L"Microsoft.WindowsAppRuntime.Bootstrap.dll" })
                {
                    auto const module = ::GetModuleHandleW(moduleName);

                    if (module == nullptr)
                    {
                        continue;
                    }

                    wchar_t modulePath[MAX_PATH]{};

                    if (::GetModuleFileNameW(module, modulePath, ARRAYSIZE(modulePath)) == 0)
                    {
                        continue;
                    }

                    // ...\Microsoft.WindowsAppRuntime.1.8_8000.946.1701.0_x64__8wekyb3d8bbwe\...
                    std::wstring const path{ modulePath };

                    constexpr std::wstring_view packagePrefix{ L"Microsoft.WindowsAppRuntime." };

                    auto const start = path.find(packagePrefix);

                    if (start != std::wstring::npos)
                    {
                        auto const end = path.find(L'\\', start);

                        auto const folder = path.substr(
                            start + packagePrefix.size(),
                            end == std::wstring::npos ? std::wstring::npos : end - start - packagePrefix.size());

                        auto const underscore = folder.find(L'_');

                        if (underscore != std::wstring::npos)
                        {
                            auto const framework = folder.substr(0, underscore);
                            auto const rest = folder.substr(underscore + 1);
                            auto const versionEnd = rest.find(L'_');

                            return framework + L" (" +
                                (versionEnd == std::wstring::npos ? rest : rest.substr(0, versionEnd)) + L")";
                        }
                    }

                    auto const version = GetFileVersionString(path);

                    if (!version.empty())
                    {
                        return version;
                    }
                }
            }
            MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to read the Windows App SDK version.")

            return L"Not loaded";
        }

        std::wstring MidiSdkVersion() noexcept
        {
            try
            {
                // The copy this tool will actually bind to, which is the honest answer
                if (auto const module = ::GetModuleHandleW(L"Windows.Devices.Midi2.dll"))
                {
                    wchar_t modulePath[MAX_PATH]{};

                    if (::GetModuleFileNameW(module, modulePath, ARRAYSIZE(modulePath)) > 0)
                    {
                        auto const version = GetFileVersionString(modulePath);

                        if (!version.empty())
                        {
                            return version;
                        }
                    }
                }

                std::vector<std::wstring> folders{ GetExecutableFolder() };

                // The registered value is the install FOLDER, not a version number.
                auto const installed = wil::reg::try_get_value_string(
                    HKEY_LOCAL_MACHINE,
                    LR"(SOFTWARE\Microsoft\Windows MIDI Services\Desktop App SDK Runtime)",
                    L"Installed");

                if (installed.has_value())
                {
                    folders.push_back(installed.value());
                }

                for (auto const& folder : folders)
                {
                    if (folder.empty())
                    {
                        continue;
                    }

                    auto normalized = folder;

                    if (normalized.back() != L'\\')
                    {
                        normalized += L'\\';
                    }

                    // the current name first, then the name earlier runtimes shipped under
                    for (auto const* const fileName : { L"Windows.Devices.Midi2.dll", L"Microsoft.Windows.Devices.Midi2.dll" })
                    {
                        auto const version = GetFileVersionString(normalized + fileName);

                        if (!version.empty())
                        {
                            return version;
                        }
                    }
                }
            }
            MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to read the MIDI SDK version.")

            return L"Not installed";
        }

        bool DeveloperModeEnabled() noexcept
        {
            try
            {
                auto const value = wil::reg::try_get_value_dword(
                    HKEY_LOCAL_MACHINE,
                    LR"(SOFTWARE\Microsoft\Windows\CurrentVersion\AppModelUnlock)",
                    L"AllowDevelopmentWithoutDevLicense");

                return value.has_value() && value.value() > 0;
            }
            MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to read the developer mode setting.")

            return false;
        }
    }

    _Use_decl_annotations_
    std::wstring GetFileVersionString(std::wstring const& filePath) noexcept
    {
        try
        {
            if (filePath.empty())
            {
                return {};
            }

            DWORD handle{ 0 };
            auto const size = ::GetFileVersionInfoSizeW(filePath.c_str(), &handle);

            if (size == 0)
            {
                return {};
            }

            std::vector<std::byte> buffer(size);

            if (!::GetFileVersionInfoW(filePath.c_str(), 0, size, buffer.data()))
            {
                return {};
            }

            VS_FIXEDFILEINFO* fixedInfo{ nullptr };
            UINT fixedInfoSize{ 0 };

            if (!::VerQueryValueW(buffer.data(), L"\\", reinterpret_cast<void**>(&fixedInfo), &fixedInfoSize) ||
                fixedInfo == nullptr)
            {
                return {};
            }

            return std::format(L"{}.{}.{}.{}",
                HIWORD(fixedInfo->dwFileVersionMS),
                LOWORD(fixedInfo->dwFileVersionMS),
                HIWORD(fixedInfo->dwFileVersionLS),
                LOWORD(fixedInfo->dwFileVersionLS));
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to read a file version.")

        return {};
    }

    SystemInformation GatherSystemInformation() noexcept
    {
        SystemInformation information{};

        try
        {
            information.WindowsVersion = OperatingSystemVersion();
            information.WindowsEdition = OperatingSystemEdition();
            information.CultureName = CurrentCultureName();
            information.WindowsAppSdkVersion = WindowsAppSdkVersion();
            information.MidiSdkVersion = MidiSdkVersion();
            information.DeveloperModeEnabled = DeveloperModeEnabled();

            SYSTEM_INFO nativeSystemInfo{};
            ::GetNativeSystemInfo(&nativeSystemInfo);

            SYSTEM_INFO processSystemInfo{};
            ::GetSystemInfo(&processSystemInfo);

            information.WindowsArchitecture = ArchitectureName(nativeSystemInfo.wProcessorArchitecture);

            // A tool running emulated on Arm64 has caused enough confusion to be worth calling
            // out rather than showing one architecture and hoping.
            if (processSystemInfo.wProcessorArchitecture != nativeSystemInfo.wProcessorArchitecture)
            {
                information.WindowsArchitecture += L" (app is " +
                    ArchitectureName(processSystemInfo.wProcessorArchitecture) + L")";
            }

            wchar_t modulePath[MAX_PATH]{};

            if (::GetModuleFileNameW(nullptr, modulePath, ARRAYSIZE(modulePath)) > 0)
            {
                information.ToolVersion = GetFileVersionString(modulePath);
            }

            wchar_t computerName[MAX_COMPUTERNAME_LENGTH + 1]{};
            DWORD computerNameLength{ ARRAYSIZE(computerName) };

            if (::GetComputerNameW(computerName, &computerNameLength))
            {
                information.ComputerName = computerName;
            }
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to gather system information.")

        return information;
    }
}
