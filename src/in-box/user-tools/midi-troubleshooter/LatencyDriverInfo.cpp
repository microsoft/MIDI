// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "LatencyDriverInfo.h"
#include "LatencyDriverNames.h"

#include <windows.h>
#include <winver.h>
#include <setupapi.h>
#include <initguid.h>
#include <devpkey.h>

#include <algorithm>
#include <cstring>
#include <cwchar>
#include <cwctype>
#include <format>
#include <string_view>
#include <unordered_map>
#include <vector>

#pragma comment(lib, "version.lib")
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "advapi32.lib")

namespace miditroubleshooter::latency
{
    namespace
    {
        // a driver used by a lot of devices, such as a hub driver, lists only the first few
        constexpr size_t MaximumDevicesPerDriver{ 8 };

        // enough of the start of a driver file for its headers
        constexpr DWORD HeaderBytes{ 4096 };

        std::wstring Lower(_In_ std::wstring_view const text)
        {
            std::wstring lower{ text };

            for (auto& character : lower)
            {
                character = static_cast<wchar_t>(std::towlower(character));
            }

            return lower;
        }

        bool ContainsNoCase(_In_ std::wstring_view const text, _In_ std::wstring_view const part)
        {
            return Lower(text).find(Lower(part)) != std::wstring::npos;
        }

        std::wstring FileNamePart(_In_ std::wstring_view const path)
        {
            auto const separator = path.find_last_of(L"\\/");

            return std::wstring{ separator == std::wstring_view::npos ? path : path.substr(separator + 1) };
        }

        std::wstring WindowsDirectory()
        {
            wchar_t buffer[MAX_PATH]{};
            auto const length = GetWindowsDirectoryW(buffer, MAX_PATH);

            return length > 0 && length < MAX_PATH ? std::wstring{ buffer, length } : std::wstring{ L"C:\\Windows" };
        }

        std::wstring SystemDirectory()
        {
            wchar_t buffer[MAX_PATH]{};
            auto const length = GetSystemDirectoryW(buffer, MAX_PATH);

            return length > 0 && length < MAX_PATH ? std::wstring{ buffer, length } : WindowsDirectory() + L"\\System32";
        }

        // \Device\HarddiskVolume3 and the drive letter it is, for every drive
        std::vector<std::pair<std::wstring, std::wstring>> DeviceNamesOfDrives()
        {
            std::vector<std::pair<std::wstring, std::wstring>> drives{};

            auto const mask = GetLogicalDrives();

            for (wchar_t letter = L'A'; letter <= L'Z'; ++letter)
            {
                if ((mask & (1u << (letter - L'A'))) == 0)
                {
                    continue;
                }

                wchar_t const drive[]{ letter, L':', L'\0' };
                wchar_t target[MAX_PATH]{};

                if (QueryDosDeviceW(drive, target, MAX_PATH) != 0)
                {
                    drives.emplace_back(target, drive);
                }
            }

            return drives;
        }

        // A path from a trace can name anything, including a share on another computer, and this
        // app runs as administrator. Only a plain file on one of this PC's own disks is opened.
        bool IsLocalFixedPath(_In_ std::wstring const& path)
        {
            if (path.size() < 4 || path[1] != L':' || path[2] != L'\\' || !std::iswalpha(path[0]))
            {
                return false;
            }

            // no stepping out of the folder it names, no wildcards, no alternate streams
            if (path.find(L"..") != std::wstring::npos || path.find_first_of(L"*?\"<>|") != std::wstring::npos ||
                path.find(L':', 2) != std::wstring::npos)
            {
                return false;
            }

            wchar_t const root[]{ path[0], L':', L'\\', L'\0' };

            return GetDriveTypeW(root) == DRIVE_FIXED;
        }

        struct ImageIdentity
        {
            bool Valid{ false };
            uint32_t TimeDateStamp{ 0 };
            uint32_t CheckSum{ 0 };
            uint32_t SizeOfImage{ 0 };
        };

        ImageIdentity ReadImageIdentity(_In_ std::wstring const& path)
        {
            ImageIdentity identity{};

            if (path.empty())
            {
                return identity;
            }

            // a link is left unfollowed, so it can't lead anywhere IsLocalFixedPath ruled out
            auto const file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);

            if (file == INVALID_HANDLE_VALUE)
            {
                return identity;
            }

            std::vector<uint8_t> bytes(HeaderBytes);
            DWORD read{ 0 };
            auto const ok = ReadFile(file, bytes.data(), HeaderBytes, &read, nullptr);

            CloseHandle(file);

            if (!ok || read < sizeof(IMAGE_DOS_HEADER))
            {
                return identity;
            }

            IMAGE_DOS_HEADER dos{};
            std::memcpy(&dos, bytes.data(), sizeof(dos));

            if (dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew < 0)
            {
                return identity;
            }

            auto const ntOffset = static_cast<size_t>(dos.e_lfanew);

            // signature, file header, then the optional header's magic
            if (ntOffset + sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER) + sizeof(WORD) > read)
            {
                return identity;
            }

            DWORD signature{ 0 };
            std::memcpy(&signature, bytes.data() + ntOffset, sizeof(signature));

            if (signature != IMAGE_NT_SIGNATURE)
            {
                return identity;
            }

            IMAGE_FILE_HEADER fileHeader{};
            std::memcpy(&fileHeader, bytes.data() + ntOffset + sizeof(DWORD), sizeof(fileHeader));

            auto const optionalOffset = ntOffset + sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER);

            WORD magic{ 0 };
            std::memcpy(&magic, bytes.data() + optionalOffset, sizeof(magic));

            if (magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC && optionalOffset + sizeof(IMAGE_OPTIONAL_HEADER64) <= read)
            {
                IMAGE_OPTIONAL_HEADER64 optional{};
                std::memcpy(&optional, bytes.data() + optionalOffset, sizeof(optional));

                identity.CheckSum = optional.CheckSum;
                identity.SizeOfImage = optional.SizeOfImage;
            }
            else if (magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC && optionalOffset + sizeof(IMAGE_OPTIONAL_HEADER32) <= read)
            {
                IMAGE_OPTIONAL_HEADER32 optional{};
                std::memcpy(&optional, bytes.data() + optionalOffset, sizeof(optional));

                identity.CheckSum = optional.CheckSum;
                identity.SizeOfImage = optional.SizeOfImage;
            }
            else
            {
                return identity;
            }

            identity.TimeDateStamp = fileHeader.TimeDateStamp;
            identity.Valid = true;

            return identity;
        }

        // the same file the trace saw, rather than one with the same name
        bool SameImage(_In_ ModuleInfo const& module, _In_ ImageIdentity const& identity) noexcept
        {
            if (!identity.Valid)
            {
                return false;
            }

            if (module.TimeDateStamp != 0 && identity.TimeDateStamp != module.TimeDateStamp)
            {
                return false;
            }

            if (module.Checksum != 0 && identity.CheckSum != 0 && identity.CheckSum != module.Checksum)
            {
                return false;
            }

            return module.Size == 0 || identity.SizeOfImage == module.Size;
        }

        struct VersionDetails
        {
            std::wstring Company{};
            std::wstring Description{};
            std::wstring Version{};
        };

        VersionDetails ReadVersionDetails(_In_ std::wstring const& path)
        {
            VersionDetails details{};

            DWORD ignored{ 0 };
            auto const size = GetFileVersionInfoSizeExW(FILE_VER_GET_LOCALISED, path.c_str(), &ignored);

            if (size == 0)
            {
                return details;
            }

            std::vector<uint8_t> data(size);

            if (!GetFileVersionInfoExW(FILE_VER_GET_LOCALISED, path.c_str(), 0, size, data.data()))
            {
                return details;
            }

            void* fixedPointer{ nullptr };
            UINT fixedLength{ 0 };

            if (VerQueryValueW(data.data(), L"\\", &fixedPointer, &fixedLength) && fixedPointer != nullptr && fixedLength >= sizeof(VS_FIXEDFILEINFO))
            {
                VS_FIXEDFILEINFO fixed{};
                std::memcpy(&fixed, fixedPointer, sizeof(fixed));

                details.Version = std::format(L"{}.{}.{}.{}",
                    HIWORD(fixed.dwFileVersionMS), LOWORD(fixed.dwFileVersionMS),
                    HIWORD(fixed.dwFileVersionLS), LOWORD(fixed.dwFileVersionLS));
            }

            struct Translation
            {
                WORD Language;
                WORD CodePage;
            };

            void* translationPointer{ nullptr };
            UINT translationLength{ 0 };

            std::vector<Translation> translations{};

            if (VerQueryValueW(data.data(), L"\\VarFileInfo\\Translation", &translationPointer, &translationLength) &&
                translationPointer != nullptr && translationLength >= sizeof(Translation))
            {
                translations.resize(translationLength / sizeof(Translation));
                std::memcpy(translations.data(), translationPointer, translations.size() * sizeof(Translation));
            }

            // the usual ones when a file lists none: US English, then neutral, in Unicode
            translations.push_back(Translation{ 0x0409, 0x04B0 });
            translations.push_back(Translation{ 0x0000, 0x04B0 });

            auto const readString = [&data, &translations](wchar_t const* const name) -> std::wstring
                {
                    for (auto const& translation : translations)
                    {
                        auto const query = std::format(L"\\StringFileInfo\\{:04x}{:04x}\\{}", translation.Language, translation.CodePage, name);

                        void* value{ nullptr };
                        UINT length{ 0 };

                        if (VerQueryValueW(data.data(), query.c_str(), &value, &length) && value != nullptr && length > 0)
                        {
                            std::wstring text{ static_cast<wchar_t const*>(value), length };

                            while (!text.empty() && (text.back() == L'\0' || text.back() == L' '))
                            {
                                text.pop_back();
                            }

                            if (!text.empty())
                            {
                                return text;
                            }
                        }
                    }

                    return {};
                };

            details.Company = readString(L"CompanyName");
            details.Description = readString(L"FileDescription");

            return details;
        }

        std::wstring DeviceStringProperty(_In_ HDEVINFO const set, _In_ SP_DEVINFO_DATA& device, _In_ DEVPROPKEY const& key)
        {
            DEVPROPTYPE type{ DEVPROP_TYPE_EMPTY };
            DWORD size{ 0 };

            SetupDiGetDevicePropertyW(set, &device, &key, &type, nullptr, 0, &size, 0);

            if (type != DEVPROP_TYPE_STRING || size < sizeof(wchar_t))
            {
                return {};
            }

            std::vector<wchar_t> buffer((size / sizeof(wchar_t)) + 1);

            if (!SetupDiGetDevicePropertyW(set, &device, &key, &type, reinterpret_cast<PBYTE>(buffer.data()), size, nullptr, 0))
            {
                return {};
            }

            return std::wstring{ buffer.data() };
        }

        std::vector<std::wstring> DeviceStringListProperty(_In_ HDEVINFO const set, _In_ SP_DEVINFO_DATA& device, _In_ DEVPROPKEY const& key)
        {
            DEVPROPTYPE type{ DEVPROP_TYPE_EMPTY };
            DWORD size{ 0 };

            SetupDiGetDevicePropertyW(set, &device, &key, &type, nullptr, 0, &size, 0);

            if (type != DEVPROP_TYPE_STRING_LIST || size < sizeof(wchar_t))
            {
                return {};
            }

            std::vector<wchar_t> buffer((size / sizeof(wchar_t)) + 2);

            if (!SetupDiGetDevicePropertyW(set, &device, &key, &type, reinterpret_cast<PBYTE>(buffer.data()), size, nullptr, 0))
            {
                return {};
            }

            std::vector<std::wstring> values{};

            for (auto const* item = buffer.data(); *item != L'\0'; item += std::wcslen(item) + 1)
            {
                values.emplace_back(item);
            }

            return values;
        }

        // The driver file a service runs, from its ImagePath, or the usual name when it has none.
        std::wstring DriverFileOfService(_In_ std::wstring const& service)
        {
            auto const key = L"SYSTEM\\CurrentControlSet\\Services\\" + service;

            wchar_t buffer[MAX_PATH * 2]{};
            DWORD size{ sizeof(buffer) };

            if (RegGetValueW(HKEY_LOCAL_MACHINE, key.c_str(), L"ImagePath", RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ | RRF_NOEXPAND,
                nullptr, buffer, &size) == ERROR_SUCCESS && buffer[0] != L'\0')
            {
                return Lower(FileNamePart(buffer));
            }

            return Lower(service + L".sys");
        }

        struct DeviceUse
        {
            std::wstring Name{};
            std::wstring Class{};
            bool Filter{ false };
        };

        // Every present device, by the lower-case file name of each driver it uses.
        std::unordered_map<std::wstring, std::vector<DeviceUse>> DevicesByDriverFile()
        {
            std::unordered_map<std::wstring, std::vector<DeviceUse>> devices{};
            std::unordered_map<std::wstring, std::wstring> serviceFiles{};

            auto const set = SetupDiGetClassDevsW(nullptr, nullptr, nullptr, DIGCF_ALLCLASSES | DIGCF_PRESENT);

            if (set == INVALID_HANDLE_VALUE)
            {
                return devices;
            }

            try
            {
                SP_DEVINFO_DATA device{};
                device.cbSize = sizeof(device);

                for (DWORD index = 0; SetupDiEnumDeviceInfo(set, index, &device); ++index)
                {
                    auto name = DeviceStringProperty(set, device, DEVPKEY_Device_FriendlyName);

                    if (name.empty())
                    {
                        name = DeviceStringProperty(set, device, DEVPKEY_Device_DeviceDesc);
                    }

                    auto const className = DeviceStringProperty(set, device, DEVPKEY_Device_Class);

                    auto const add = [&](std::wstring const& service, bool const filter)
                        {
                            if (service.empty())
                            {
                                return;
                            }

                            auto found = serviceFiles.find(service);

                            if (found == serviceFiles.end())
                            {
                                found = serviceFiles.emplace(service, DriverFileOfService(service)).first;
                            }

                            auto& uses = devices[found->second];

                            if (uses.size() < MaximumDevicesPerDriver)
                            {
                                uses.push_back(DeviceUse{ name, className, filter });
                            }
                        };

                    add(DeviceStringProperty(set, device, DEVPKEY_Device_Service), false);

                    for (auto const& filter : DeviceStringListProperty(set, device, DEVPKEY_Device_UpperFilters))
                    {
                        add(filter, true);
                    }

                    for (auto const& filter : DeviceStringListProperty(set, device, DEVPKEY_Device_LowerFilters))
                    {
                        add(filter, true);
                    }
                }
            }
            catch (...)
            {
                // whatever was found so far is still worth having
            }

            SetupDiDestroyDeviceInfoList(set);

            return devices;
        }

        // For a driver the name list doesn't know: what kind of device it drives.
        DriverKind KindFromDevice(_In_ DeviceUse const& device)
        {
            auto const& className = device.Class;

            if (names::EqualsNoCase(className, L"Net"))
            {
                if (ContainsNoCase(device.Name, L"Wi-Fi") || ContainsNoCase(device.Name, L"WiFi") ||
                    ContainsNoCase(device.Name, L"Wireless") || ContainsNoCase(device.Name, L"WLAN") ||
                    ContainsNoCase(device.Name, L"802.11"))
                {
                    return DriverKind::WiFi;
                }

                if (ContainsNoCase(device.Name, L"Bluetooth"))
                {
                    return DriverKind::Bluetooth;
                }

                if (ContainsNoCase(device.Name, L"Hyper-V") || ContainsNoCase(device.Name, L"Virtual"))
                {
                    return DriverKind::Hypervisor;
                }

                return DriverKind::Ethernet;
            }

            if (names::EqualsNoCase(className, L"Bluetooth"))
            {
                return DriverKind::Bluetooth;
            }

            if (names::EqualsNoCase(className, L"Display"))
            {
                return DriverKind::Graphics;
            }

            if (names::EqualsNoCase(className, L"DiskDrive") || names::EqualsNoCase(className, L"SCSIAdapter") ||
                names::EqualsNoCase(className, L"HDC") || names::EqualsNoCase(className, L"Volume") ||
                names::EqualsNoCase(className, L"SDHost"))
            {
                return DriverKind::Storage;
            }

            if (names::EqualsNoCase(className, L"USB"))
            {
                return DriverKind::UsbController;
            }

            if (names::EqualsNoCase(className, L"MEDIA") || names::EqualsNoCase(className, L"AudioEndpoint"))
            {
                return DriverKind::Audio;
            }

            if (names::EqualsNoCase(className, L"HIDClass") || names::EqualsNoCase(className, L"Keyboard") ||
                names::EqualsNoCase(className, L"Mouse"))
            {
                return DriverKind::Input;
            }

            if (names::EqualsNoCase(className, L"Processor"))
            {
                return DriverKind::Power;
            }

            return DriverKind::Unknown;
        }
    }

    _Use_decl_annotations_
    std::wstring Win32PathFromTracePath(std::wstring const& tracePath)
    {
        if (tracePath.empty())
        {
            return {};
        }

        auto const lower = Lower(tracePath);

        auto const startsWith = [&lower](std::wstring_view const prefix)
            {
                return lower.size() >= prefix.size() && std::wstring_view{ lower }.substr(0, prefix.size()) == prefix;
            };

        if (startsWith(L"\\systemroot\\"))
        {
            return WindowsDirectory() + tracePath.substr(11);
        }

        if (startsWith(L"\\??\\") || startsWith(L"\\\\?\\"))
        {
            return tracePath.substr(4);
        }

        if (startsWith(L"\\device\\"))
        {
            for (auto const& [deviceName, drive] : DeviceNamesOfDrives())
            {
                auto const prefix = Lower(deviceName) + L"\\";

                if (startsWith(prefix))
                {
                    return drive + tracePath.substr(deviceName.size());
                }
            }

            return {};
        }

        // a path with no drive, on the Windows drive
        if (startsWith(L"\\windows\\"))
        {
            return WindowsDirectory().substr(0, 2) + tracePath;
        }

        // relative to the Windows folder, as a service's ImagePath often is
        if (startsWith(L"system32\\"))
        {
            return WindowsDirectory() + L"\\" + tracePath;
        }

        if (tracePath.size() > 2 && tracePath[1] == L':')
        {
            return tracePath;
        }

        // just a file name: a driver is almost always in the drivers folder
        if (tracePath.find_first_of(L"\\/") == std::wstring::npos)
        {
            return SystemDirectory() + L"\\drivers\\" + tracePath;
        }

        return {};
    }

    _Use_decl_annotations_
    void AddDriverDetails(Results& results) noexcept
    {
        try
        {
            std::vector<int32_t> wanted{};

            for (auto const& driver : results.Drivers)
            {
                wanted.push_back(driver.Module);
                wanted.insert(wanted.end(), driver.Frameworks.begin(), driver.Frameworks.end());
            }

            for (auto const& routine : results.Routines)
            {
                wanted.push_back(routine.Module);
                wanted.push_back(routine.ClientModule);
                wanted.push_back(routine.Driver);
            }

            std::sort(wanted.begin(), wanted.end());
            wanted.erase(std::unique(wanted.begin(), wanted.end()), wanted.end());

            std::unordered_map<std::wstring, std::vector<DeviceUse>> devices{};
            bool devicesRead{ false };

            for (auto const index : wanted)
            {
                if (index < 0 || static_cast<size_t>(index) >= results.Modules.size())
                {
                    continue;
                }

                auto& module = results.Modules[static_cast<size_t>(index)];

                auto const path = Win32PathFromTracePath(module.TracePath.empty() ? module.FileName : module.TracePath);

                if (!IsLocalFixedPath(path))
                {
                    continue;
                }

                module.LocalFileMatches = SameImage(module, ReadImageIdentity(path));

                if (!module.LocalFileMatches)
                {
                    continue;
                }

                module.LocalPath = path;

                auto const version = ReadVersionDetails(path);
                module.Company = version.Company;
                module.Description = version.Description;
                module.Version = version.Version;
                module.IsMicrosoft = names::StartsWithNoCase(module.Company, L"Microsoft");

                // only read once something on this PC matched
                if (!devicesRead)
                {
                    devices = DevicesByDriverFile();
                    devicesRead = true;
                }

                if (auto const found = devices.find(Lower(module.FileName)); found != devices.end())
                {
                    DeviceUse const* first{ nullptr };

                    for (auto const& use : found->second)
                    {
                        if (std::find(module.Devices.begin(), module.Devices.end(), use.Name) == module.Devices.end() && !use.Name.empty())
                        {
                            module.Devices.push_back(use.Name);
                        }

                        // the device it drives, rather than one it only filters
                        if (first == nullptr || (first->Filter && !use.Filter))
                        {
                            first = &use;
                        }
                    }

                    if (first != nullptr)
                    {
                        module.DeviceClass = first->Class;

                        if (module.Kind == DriverKind::Unknown)
                        {
                            module.Kind = KindFromDevice(*first);
                        }
                    }
                }
            }
        }
        catch (...)
        {
            // the check is still useful with only the names
        }
    }
}
