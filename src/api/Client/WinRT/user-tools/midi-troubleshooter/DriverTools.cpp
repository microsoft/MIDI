// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "DriverTools.h"
#include "ProcessRunner.h"
#include "StringResources.h"
#include "ToolPaths.h"

// INITGUID has to be in effect before devpkey.h and devguid.h or the property keys and class
// GUIDs are only declared, not defined, and the link fails.
#include <initguid.h>
#include <devpkey.h>
#include <devguid.h>

#include <setupapi.h>
#include <newdev.h>
#include <cfgmgr32.h>

namespace miditroubleshooter
{
    namespace
    {
        namespace res
        {
            inline std::wstring GetString(std::wstring_view resourceKey) noexcept
            {
                return std::wstring{ ::miditroubleshooter::resources::GetString(resourceKey) };
            }

            template <typename... TArgs>
            inline std::wstring FormatString(std::wstring_view resourceKey, TArgs&&... args) noexcept
            {
                return std::wstring{ ::miditroubleshooter::resources::FormatString(
                    resourceKey, std::forward<TArgs>(args)...) };
            }
        }

        // The two in-box class drivers a class compliant USB MIDI device can use. See
        // https://microsoft.github.io/MIDI/kb/how-to-switch-between-class-drivers/
        constexpr wchar_t UmpDriverInfName[] = L"usbmidi2.inf";
        constexpr wchar_t ClassicDriverInfName[] = L"wdma_usb.inf";
        constexpr wchar_t UsbAudio2InfName[] = L"usbaudio2.inf";

        std::wstring Lowered(_In_ std::wstring const& value) noexcept
        {
            std::wstring copy{ value };

            std::transform(copy.begin(), copy.end(), copy.begin(),
                [](wchar_t c) { return static_cast<wchar_t>(::towlower(c)); });

            return copy;
        }

        std::wstring FileNameOnly(_In_ std::wstring const& path) noexcept
        {
            auto const separator = path.find_last_of(L"\\/");

            return separator == std::wstring::npos ? path : path.substr(separator + 1);
        }

        DeviceDriverKind KindFromInfName(_In_ std::wstring const& infFileName) noexcept
        {
            auto const lowered = Lowered(FileNameOnly(infFileName));

            if (lowered == UmpDriverInfName)
            {
                return DeviceDriverKind::UniversalMidiPacket;
            }

            if (lowered == ClassicDriverInfName)
            {
                return DeviceDriverKind::ClassicUsbAudio;
            }

            if (lowered == UsbAudio2InfName)
            {
                return DeviceDriverKind::UsbAudio2;
            }

            return lowered.starts_with(L"oem") ? DeviceDriverKind::Vendor : DeviceDriverKind::Unknown;
        }

        std::wstring GetDeviceStringProperty(
            _In_ HDEVINFO const deviceInfoSet,
            _In_ SP_DEVINFO_DATA& deviceInfoData,
            _In_ DEVPROPKEY const& key) noexcept
        {
            try
            {
                DEVPROPTYPE propertyType{ 0 };
                DWORD requiredSize{ 0 };

                ::SetupDiGetDevicePropertyW(
                    deviceInfoSet, &deviceInfoData, &key, &propertyType, nullptr, 0, &requiredSize, 0);

                if (requiredSize == 0 || propertyType != DEVPROP_TYPE_STRING)
                {
                    return {};
                }

                std::vector<std::byte> buffer(requiredSize);

                if (!::SetupDiGetDevicePropertyW(
                    deviceInfoSet, &deviceInfoData, &key, &propertyType,
                    reinterpret_cast<PBYTE>(buffer.data()), requiredSize, &requiredSize, 0))
                {
                    return {};
                }

                return std::wstring{ reinterpret_cast<wchar_t const*>(buffer.data()) };
            }
            catch (...)
            {
                return {};
            }
        }

        std::vector<std::wstring> GetDeviceStringListProperty(
            _In_ HDEVINFO const deviceInfoSet,
            _In_ SP_DEVINFO_DATA& deviceInfoData,
            _In_ DEVPROPKEY const& key) noexcept
        {
            std::vector<std::wstring> values{};

            try
            {
                DEVPROPTYPE propertyType{ 0 };
                DWORD requiredSize{ 0 };

                ::SetupDiGetDevicePropertyW(
                    deviceInfoSet, &deviceInfoData, &key, &propertyType, nullptr, 0, &requiredSize, 0);

                if (requiredSize == 0 || propertyType != DEVPROP_TYPE_STRING_LIST)
                {
                    return values;
                }

                std::vector<std::byte> buffer(requiredSize);

                if (!::SetupDiGetDevicePropertyW(
                    deviceInfoSet, &deviceInfoData, &key, &propertyType,
                    reinterpret_cast<PBYTE>(buffer.data()), requiredSize, &requiredSize, 0))
                {
                    return values;
                }

                auto const* current = reinterpret_cast<wchar_t const*>(buffer.data());

                while (current != nullptr && *current != L'\0')
                {
                    values.emplace_back(current);
                    current += wcslen(current) + 1;
                }
            }
            catch (...)
            {
            }

            return values;
        }

        std::wstring GetDeviceInstanceId(
            _In_ HDEVINFO const deviceInfoSet,
            _In_ SP_DEVINFO_DATA& deviceInfoData) noexcept
        {
            wchar_t instanceId[MAX_DEVICE_ID_LEN]{};
            DWORD requiredSize{ 0 };

            if (::SetupDiGetDeviceInstanceIdW(
                deviceInfoSet, &deviceInfoData, instanceId, ARRAYSIZE(instanceId), &requiredSize))
            {
                return instanceId;
            }

            return {};
        }

        // A USB audio class interface is what both class drivers bind to. Anything else in the
        // media class is a sound card or a vendor device and is not ours to switch.
        //
        // The compatible ids describe the interface or function this device node covers, so
        // they can rule a node OUT but cannot prove one is MIDI. Measured on real hardware:
        // MIDI streaming (subclass 3) is only how some devices appear - an Iridium and a Moog
        // One are subclass 3, but a Marshall CODE and a SoftStep are subclass 1 (audio control)
        // and a KOMPLETE KONTROL M32 is subclass 0 protocol 0 (a USB Audio 1.0 function), and
        // all four are real MIDI devices. Only two shapes can be excluded with confidence:
        // an audio streaming interface, which is audio by definition, and a USB Audio 2.0
        // function, whose MIDI streaming interfaces are not part of the audio function and so
        // get a device node of their own.
        bool LooksLikeUsbAudioClassDevice(
            _In_ HDEVINFO const deviceInfoSet,
            _In_ SP_DEVINFO_DATA& deviceInfoData,
            _In_ std::wstring const& instanceId,
            _In_ DeviceDriverKind const currentDriver) noexcept
        {
            if (!Lowered(instanceId).starts_with(L"usb\\"))
            {
                return false;
            }

            // already on the MIDI class driver, so there is nothing to work out
            if (currentDriver == DeviceDriverKind::UniversalMidiPacket)
            {
                return true;
            }

            auto ids = GetDeviceStringListProperty(deviceInfoSet, deviceInfoData, DEVPKEY_Device_CompatibleIds);

            for (auto const& id : GetDeviceStringListProperty(deviceInfoSet, deviceInfoData, DEVPKEY_Device_HardwareIds))
            {
                ids.push_back(id);
            }

            bool isAudioClass{ false };
            bool isMidiStreaming{ false };
            bool isAudioStreaming{ false };
            bool isUsbAudio2Function{ false };

            for (auto const& id : ids)
            {
                auto const lowered = Lowered(id);

                // USB audio class is 01; the MIDI streaming interface is a subclass of it
                if (lowered.find(L"class_01") == std::wstring::npos)
                {
                    continue;
                }

                isAudioClass = true;

                if (lowered.find(L"subclass_03") != std::wstring::npos)
                {
                    isMidiStreaming = true;
                }
                else if (lowered.find(L"subclass_02") != std::wstring::npos)
                {
                    isAudioStreaming = true;
                }
                else if (lowered.find(L"subclass_00") != std::wstring::npos &&
                    lowered.find(L"prot_20") != std::wstring::npos)
                {
                    isUsbAudio2Function = true;
                }
            }

            if (!isAudioClass)
            {
                return false;
            }

            if (isMidiStreaming)
            {
                return true;
            }

            return !isAudioStreaming && !isUsbAudio2Function;
        }

        struct DeviceInfoSet
        {
            HDEVINFO Handle{ INVALID_HANDLE_VALUE };

            ~DeviceInfoSet()
            {
                if (Handle != INVALID_HANDLE_VALUE)
                {
                    ::SetupDiDestroyDeviceInfoList(Handle);
                }
            }

            DeviceInfoSet() = default;
            DeviceInfoSet(DeviceInfoSet const&) = delete;
            DeviceInfoSet& operator=(DeviceInfoSet const&) = delete;
        };

        std::vector<DeviceDriverChoice> BuildDriverChoices(
            _In_ HDEVINFO const deviceInfoSet,
            _In_ SP_DEVINFO_DATA& deviceInfoData) noexcept
        {
            std::vector<DeviceDriverChoice> choices{};

            try
            {
                if (!::SetupDiBuildDriverInfoList(deviceInfoSet, &deviceInfoData, SPDIT_COMPATDRIVER))
                {
                    return choices;
                }

                auto const cleanup = wil::scope_exit([&]()
                    {
                        ::SetupDiDestroyDriverInfoList(deviceInfoSet, &deviceInfoData, SPDIT_COMPATDRIVER);
                    });

                for (DWORD index = 0;; index++)
                {
                    SP_DRVINFO_DATA_W driverInfo{};
                    driverInfo.cbSize = sizeof(driverInfo);

                    if (!::SetupDiEnumDriverInfoW(
                        deviceInfoSet, &deviceInfoData, SPDIT_COMPATDRIVER, index, &driverInfo))
                    {
                        break;
                    }

                    // two calls: the first only reports how much room the detail needs
                    DWORD requiredSize{ 0 };

                    ::SetupDiGetDriverInfoDetailW(
                        deviceInfoSet, &deviceInfoData, &driverInfo, nullptr, 0, &requiredSize);

                    if (requiredSize < sizeof(SP_DRVINFO_DETAIL_DATA_W))
                    {
                        requiredSize = sizeof(SP_DRVINFO_DETAIL_DATA_W);
                    }

                    std::vector<std::byte> buffer(requiredSize);

                    auto* const detail = reinterpret_cast<SP_DRVINFO_DETAIL_DATA_W*>(buffer.data());
                    detail->cbSize = sizeof(SP_DRVINFO_DETAIL_DATA_W);

                    if (!::SetupDiGetDriverInfoDetailW(
                        deviceInfoSet, &deviceInfoData, &driverInfo, detail, requiredSize, &requiredSize))
                    {
                        continue;
                    }

                    DeviceDriverChoice choice{};

                    choice.InfPath = detail->InfFileName;
                    choice.InfFileName = FileNameOnly(choice.InfPath);
                    choice.Description = driverInfo.Description;
                    choice.Manufacturer = driverInfo.MfgName;
                    choice.Kind = KindFromInfName(choice.InfFileName);

                    choice.Version = std::format(L"{}.{}.{}.{}",
                        HIWORD(driverInfo.DriverVersion >> 32),
                        LOWORD(driverInfo.DriverVersion >> 32),
                        HIWORD(driverInfo.DriverVersion & 0xFFFFFFFF),
                        LOWORD(driverInfo.DriverVersion & 0xFFFFFFFF));

                    choices.push_back(choice);
                }
            }
            MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to build the driver list for a device.")

            return choices;
        }

        bool OpenDevice(
            _In_ std::wstring const& instanceId,
            _Inout_ DeviceInfoSet& deviceInfoSet,
            _Out_ SP_DEVINFO_DATA& deviceInfoData) noexcept
        {
            deviceInfoData = SP_DEVINFO_DATA{};
            deviceInfoData.cbSize = sizeof(deviceInfoData);

            deviceInfoSet.Handle = ::SetupDiCreateDeviceInfoList(nullptr, nullptr);

            if (deviceInfoSet.Handle == INVALID_HANDLE_VALUE)
            {
                return false;
            }

            return ::SetupDiOpenDeviceInfoW(
                deviceInfoSet.Handle, instanceId.c_str(), nullptr, 0, &deviceInfoData) != FALSE;
        }

        // The INF's own text, used to tell one vendor package from another by the driver file
        // it installs. The published oemNN.inf name is assigned in install order and says
        // nothing about which driver it is.
        std::wstring ReadFileText(_In_ std::wstring const& path) noexcept
        {
            try
            {
                std::ifstream stream{ path, std::ios::binary };

                if (!stream.is_open())
                {
                    return {};
                }

                std::string bytes{ std::istreambuf_iterator<char>{ stream }, std::istreambuf_iterator<char>{} };

                if (bytes.empty())
                {
                    return {};
                }

                auto const required = ::MultiByteToWideChar(
                    CP_ACP, 0, bytes.data(), static_cast<int>(bytes.size()), nullptr, 0);

                if (required <= 0)
                {
                    return {};
                }

                std::wstring text(static_cast<size_t>(required), L'\0');

                ::MultiByteToWideChar(
                    CP_ACP, 0, bytes.data(), static_cast<int>(bytes.size()), text.data(), required);

                return text;
            }
            catch (...)
            {
                return {};
            }
        }

        // INF files are INI shaped, but the interesting values are usually %token% references
        // that have to be resolved through the [Strings] section.
        std::wstring ReadInfValue(
            _In_ std::wstring const& infPath,
            _In_ wchar_t const* const section,
            _In_ wchar_t const* const key) noexcept
        {
            try
            {
                std::array<wchar_t, 512> buffer{};

                ::GetPrivateProfileStringW(
                    section, key, L"", buffer.data(), static_cast<DWORD>(buffer.size()), infPath.c_str());

                std::wstring value{ buffer.data() };

                if (value.size() >= 2 && value.front() == L'%' && value.back() == L'%')
                {
                    auto const token = value.substr(1, value.size() - 2);

                    std::array<wchar_t, 512> resolved{};

                    ::GetPrivateProfileStringW(
                        L"Strings", token.c_str(), L"", resolved.data(),
                        static_cast<DWORD>(resolved.size()), infPath.c_str());

                    std::wstring resolvedValue{ resolved.data() };

                    if (!resolvedValue.empty())
                    {
                        value = resolvedValue;
                    }
                }

                // quoted strings are common in the [Strings] section
                if (value.size() >= 2 && value.front() == L'"' && value.back() == L'"')
                {
                    value = value.substr(1, value.size() - 2);
                }

                return value;
            }
            catch (...)
            {
                return {};
            }
        }
    }

    std::vector<HardwareDeviceInfo> EnumerateMidiHardwareDevices() noexcept
    {
        std::vector<HardwareDeviceInfo> devices{};

        try
        {
            DeviceInfoSet deviceInfoSet{};

            // The media class is where both class drivers and the vendor MIDI drivers live.
            deviceInfoSet.Handle = ::SetupDiGetClassDevsW(
                &GUID_DEVCLASS_MEDIA, nullptr, nullptr, DIGCF_PRESENT);

            if (deviceInfoSet.Handle == INVALID_HANDLE_VALUE)
            {
                return devices;
            }

            for (DWORD index = 0;; index++)
            {
                SP_DEVINFO_DATA deviceInfoData{};
                deviceInfoData.cbSize = sizeof(deviceInfoData);

                if (!::SetupDiEnumDeviceInfo(deviceInfoSet.Handle, index, &deviceInfoData))
                {
                    break;
                }

                auto const instanceId = GetDeviceInstanceId(deviceInfoSet.Handle, deviceInfoData);

                if (instanceId.empty())
                {
                    continue;
                }

                HardwareDeviceInfo device{};

                device.DriverInfName = FileNameOnly(GetDeviceStringProperty(
                    deviceInfoSet.Handle, deviceInfoData, DEVPKEY_Device_DriverInfPath));

                device.CurrentDriver = KindFromInfName(device.DriverInfName);

                if (!LooksLikeUsbAudioClassDevice(
                    deviceInfoSet.Handle, deviceInfoData, instanceId, device.CurrentDriver))
                {
                    continue;
                }

                device.InstanceId = instanceId;

                device.Name = GetDeviceStringProperty(
                    deviceInfoSet.Handle, deviceInfoData, DEVPKEY_Device_FriendlyName);

                if (device.Name.empty())
                {
                    device.Name = GetDeviceStringProperty(
                        deviceInfoSet.Handle, deviceInfoData, DEVPKEY_Device_DeviceDesc);
                }

                device.Manufacturer = GetDeviceStringProperty(
                    deviceInfoSet.Handle, deviceInfoData, DEVPKEY_Device_Manufacturer);

                device.DriverVersion = GetDeviceStringProperty(
                    deviceInfoSet.Handle, deviceInfoData, DEVPKEY_Device_DriverVersion);

                device.DriverProvider = GetDeviceStringProperty(
                    deviceInfoSet.Handle, deviceInfoData, DEVPKEY_Device_DriverProvider);

                device.ServiceName = GetDeviceStringProperty(
                    deviceInfoSet.Handle, deviceInfoData, DEVPKEY_Device_Service);

                // A device with a problem code is exactly the case the customer came here for,
                // so it is worth showing rather than filtering out.
                ULONG status{ 0 };
                ULONG problem{ 0 };

                if (::CM_Get_DevNode_Status(&status, &problem, deviceInfoData.DevInst, 0) == CR_SUCCESS)
                {
                    device.HasProblem = (status & DN_HAS_PROBLEM) != 0;
                    device.ProblemCode = problem;
                }

                for (auto const& choice : BuildDriverChoices(deviceInfoSet.Handle, deviceInfoData))
                {
                    if (choice.Kind == DeviceDriverKind::UniversalMidiPacket)
                    {
                        device.CanUseUniversalMidiPacketDriver = true;
                    }
                    else if (choice.Kind == DeviceDriverKind::ClassicUsbAudio)
                    {
                        device.CanUseClassicDriver = true;
                    }
                }

                devices.push_back(device);
            }

            std::sort(devices.begin(), devices.end(),
                [](HardwareDeviceInfo const& left, HardwareDeviceInfo const& right)
                {
                    return _wcsicmp(left.Name.c_str(), right.Name.c_str()) < 0;
                });
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to enumerate the MIDI hardware devices.")

        return devices;
    }

    _Use_decl_annotations_
    std::vector<DeviceDriverChoice> GetDriverChoicesForDevice(std::wstring const& instanceId) noexcept
    {
        DeviceInfoSet deviceInfoSet{};
        SP_DEVINFO_DATA deviceInfoData{};

        if (!OpenDevice(instanceId, deviceInfoSet, deviceInfoData))
        {
            return {};
        }

        return BuildDriverChoices(deviceInfoSet.Handle, deviceInfoData);
    }

    _Use_decl_annotations_
    DriverOperationResult SetDeviceDriver(std::wstring const& instanceId, DeviceDriverKind kind) noexcept
    {
        DriverOperationResult result{};

        try
        {
            DeviceInfoSet deviceInfoSet{};
            SP_DEVINFO_DATA deviceInfoData{};

            if (!OpenDevice(instanceId, deviceInfoSet, deviceInfoData))
            {
                result.Message = res::GetString(L"DriversErrorDeviceNotFound");
                return result;
            }

            if (!::SetupDiBuildDriverInfoList(deviceInfoSet.Handle, &deviceInfoData, SPDIT_COMPATDRIVER))
            {
                result.Message = res::GetString(L"DriversErrorNoDriverList");
                return result;
            }

            auto const cleanup = wil::scope_exit([&]()
                {
                    ::SetupDiDestroyDriverInfoList(deviceInfoSet.Handle, &deviceInfoData, SPDIT_COMPATDRIVER);
                });

            SP_DRVINFO_DATA_W selected{};
            bool found{ false };

            for (DWORD index = 0; !found; index++)
            {
                SP_DRVINFO_DATA_W driverInfo{};
                driverInfo.cbSize = sizeof(driverInfo);

                if (!::SetupDiEnumDriverInfoW(
                    deviceInfoSet.Handle, &deviceInfoData, SPDIT_COMPATDRIVER, index, &driverInfo))
                {
                    break;
                }

                DWORD requiredSize{ 0 };

                ::SetupDiGetDriverInfoDetailW(
                    deviceInfoSet.Handle, &deviceInfoData, &driverInfo, nullptr, 0, &requiredSize);

                if (requiredSize < sizeof(SP_DRVINFO_DETAIL_DATA_W))
                {
                    requiredSize = sizeof(SP_DRVINFO_DETAIL_DATA_W);
                }

                std::vector<std::byte> buffer(requiredSize);

                auto* const detail = reinterpret_cast<SP_DRVINFO_DETAIL_DATA_W*>(buffer.data());
                detail->cbSize = sizeof(SP_DRVINFO_DETAIL_DATA_W);

                if (!::SetupDiGetDriverInfoDetailW(
                    deviceInfoSet.Handle, &deviceInfoData, &driverInfo, detail, requiredSize, &requiredSize))
                {
                    continue;
                }

                if (KindFromInfName(detail->InfFileName) == kind)
                {
                    selected = driverInfo;
                    found = true;
                }
            }

            if (!found)
            {
                result.Message = res::GetString(L"DriversErrorDriverNotAvailable");
                return result;
            }

            if (!::SetupDiSetSelectedDriverW(deviceInfoSet.Handle, &deviceInfoData, &selected))
            {
                result.Message = res::FormatString(
                    L"DriversErrorSelectFailedFormat", static_cast<uint32_t>(::GetLastError()));

                return result;
            }

            BOOL rebootRequired{ FALSE };

            // The customer is deliberately overriding the ranking here, which is why this is
            // DiInstallDevice with a selected driver rather than a driver search.
            if (!::DiInstallDevice(nullptr, deviceInfoSet.Handle, &deviceInfoData, &selected, 0, &rebootRequired))
            {
                result.Message = res::FormatString(
                    L"DriversErrorInstallFailedFormat", static_cast<uint32_t>(::GetLastError()));

                return result;
            }

            result.Succeeded = true;
            result.RebootRequired = rebootRequired != FALSE;
            result.Message = res::GetString(L"DriversDriverChanged");
        }
        catch (winrt::hresult_error const& ex)
        {
            result.Message = std::wstring{ ex.message() };
            MIDI_TSHOOT_LOG_HRESULT_EXCEPTION(ex, L"Unable to change a device driver.");
        }
        catch (...)
        {
            result.Message = res::GetString(L"DriversErrorUnexpected");
            MIDI_TSHOOT_LOG_GENERAL_EXCEPTION(L"Unable to change a device driver.");
        }

        return result;
    }

    _Use_decl_annotations_
    std::vector<DriverPackageInfo> FindKorgDriverPackages(KorgDriverKind kind) noexcept
    {
        std::vector<DriverPackageInfo> packages{};

        try
        {
            wchar_t windowsFolder[MAX_PATH]{};

            if (::GetWindowsDirectoryW(windowsFolder, ARRAYSIZE(windowsFolder)) == 0)
            {
                return packages;
            }

            auto const searchPattern = std::wstring{ windowsFolder } + L"\\INF\\oem*.inf";

            WIN32_FIND_DATAW findData{};

            wil::unique_hfind findHandle{ ::FindFirstFileW(searchPattern.c_str(), &findData) };

            if (!findHandle)
            {
                return packages;
            }

            do
            {
                auto const infPath = std::wstring{ windowsFolder } + L"\\INF\\" + findData.cFileName;

                auto const provider = ReadInfValue(infPath, L"Version", L"Provider");

                if (Lowered(provider).find(L"korg") == std::wstring::npos)
                {
                    continue;
                }

                // Which KORG driver this is comes from the file it installs, not from the
                // published name, which is assigned by Windows in install order.
                auto const contents = Lowered(ReadFileText(infPath));

                auto const isBle = contents.find(L"korgbm") != std::wstring::npos;

                auto const isUsb =
                    contents.find(L"korgum") != std::wstring::npos ||
                    contents.find(L"korgusb") != std::wstring::npos;

                if (kind == KorgDriverKind::BleMidi ? !isBle : !isUsb)
                {
                    continue;
                }

                DriverPackageInfo package{};

                package.PublishedName = findData.cFileName;
                package.OriginalName = infPath;
                package.Provider = provider;
                package.ClassName = ReadInfValue(infPath, L"Version", L"Class");
                package.Kind = kind;

                // the name the package gives itself, which is what the customer recognizes
                package.DisplayName = ReadInfValue(infPath, L"SourceDisksNames", L"1");

                auto const comma = package.DisplayName.find(L',');

                if (comma != std::wstring::npos)
                {
                    package.DisplayName = package.DisplayName.substr(0, comma);
                }

                if (package.DisplayName.size() >= 2 &&
                    package.DisplayName.front() == L'"' && package.DisplayName.back() == L'"')
                {
                    package.DisplayName = package.DisplayName.substr(1, package.DisplayName.size() - 2);
                }

                packages.push_back(package);
            }
            while (::FindNextFileW(findHandle.get(), &findData));
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to enumerate the installed driver packages.")

        return packages;
    }

    _Use_decl_annotations_
    DriverOperationResult RemoveDriverPackages(std::vector<DriverPackageInfo> const& packages) noexcept
    {
        DriverOperationResult result{};

        try
        {
            if (packages.empty())
            {
                result.Succeeded = true;
                result.Message = res::GetString(L"DriversNoKorgPackages");
                return result;
            }

            auto const pnpUtil = GetToolLocations().PnpUtil;

            if (pnpUtil.empty())
            {
                result.Message = res::GetString(L"DriversErrorNoPnpUtil");
                return result;
            }

            bool allSucceeded{ true };

            for (auto const& package : packages)
            {
                // pnputil is the only supported way to remove a published driver package
                // together with the devices that are using it.
                auto const arguments = std::format(
                    L"/delete-driver {} /uninstall /force", package.PublishedName);

                auto const run = RunCapture(pnpUtil, arguments, std::chrono::seconds{ 120 });

                if (run.Started && run.ExitCode == 0)
                {
                    result.Details.push_back(res::FormatString(
                        L"DriversRemovedPackageFormat", winrt::hstring{ package.PublishedName }));
                }
                else if (run.Started && run.ExitCode == 3010)
                {
                    // ERROR_SUCCESS_REBOOT_REQUIRED
                    result.RebootRequired = true;

                    result.Details.push_back(res::FormatString(
                        L"DriversRemovedPackageFormat", winrt::hstring{ package.PublishedName }));
                }
                else
                {
                    allSucceeded = false;

                    result.Details.push_back(res::FormatString(
                        L"DriversRemoveFailedFormat",
                        winrt::hstring{ package.PublishedName },
                        static_cast<uint32_t>(run.ExitCode)));

                    if (!run.Output.empty())
                    {
                        result.Details.push_back(run.Output);
                    }
                }
            }

            result.Succeeded = allSucceeded;

            result.Message = allSucceeded ?
                res::GetString(L"DriversKorgRemoved") :
                res::GetString(L"DriversKorgRemovalIncomplete");
        }
        catch (winrt::hresult_error const& ex)
        {
            result.Message = std::wstring{ ex.message() };
            MIDI_TSHOOT_LOG_HRESULT_EXCEPTION(ex, L"Unable to remove a driver package.");
        }
        catch (...)
        {
            result.Message = res::GetString(L"DriversErrorUnexpected");
            MIDI_TSHOOT_LOG_GENERAL_EXCEPTION(L"Unable to remove a driver package.");
        }

        return result;
    }
}
