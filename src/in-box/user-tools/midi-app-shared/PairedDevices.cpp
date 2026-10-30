// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "PairedDevices.h"
#include "DeviceLinkPairing.h"

// windows.h defines this as GetObjectW, which turns IJsonValue::GetObject() into a compile error.
#undef GetObject

#include <algorithm>
#include <cwchar>
#include <filesystem>
#include <fstream>
#include <mutex>

namespace midiapp
{
    namespace
    {
        namespace mjson = winrt::Windows::Data::Json;

        constexpr wchar_t KeyFormatVersion[] = L"formatVersion";
        constexpr wchar_t KeyDevices[] = L"devices";
        constexpr wchar_t KeyKey[] = L"key";
        constexpr wchar_t KeyName[] = L"name";
        constexpr wchar_t KeyPairedAt[] = L"pairedAt";
        constexpr wchar_t KeyLastUsed[] = L"lastUsed";

        constexpr int32_t FormatVersion = 1;

        // Far more than 64 entries ever take.
        constexpr uintmax_t MaximumFileBytes = 256 * 1024;

        // One writer at a time in this process. Another process (a second copy can't run, but a
        // tool could) at worst loses an update, never half a file: writes replace the file whole.
        std::mutex g_lock{};

        std::wstring ReadText(_In_ std::wstring const& path)
        {
            std::error_code ec{};

            auto const size = std::filesystem::file_size(path, ec);

            if (ec || size == 0 || size > MaximumFileBytes)
            {
                return {};
            }

            std::ifstream file{ std::filesystem::path{ path }, std::ios::binary };

            if (!file)
            {
                return {};
            }

            std::string bytes{ std::istreambuf_iterator<char>{ file }, std::istreambuf_iterator<char>{} };

            bool valid{ false };
            return DeviceLinkFromUtf8(std::span<uint8_t const>{ reinterpret_cast<uint8_t const*>(bytes.data()), bytes.size() }, valid);
        }

        // A FILETIME is past what a JSON number holds exactly, so it's kept as decimal text.
        int64_t TimeOf(_In_ mjson::JsonObject const& value, _In_ wchar_t const* key)
        {
            if (!value.HasKey(key) || value.GetNamedValue(key).ValueType() != mjson::JsonValueType::String)
            {
                return 0;
            }

            auto const text = value.GetNamedString(key);

            if (text.empty() || text.size() > 19 ||
                !std::all_of(text.begin(), text.end(), [](wchar_t c) { return c >= L'0' && c <= L'9'; }))
            {
                return 0;
            }

            return static_cast<int64_t>(std::wcstoll(text.c_str(), nullptr, 10));
        }

        std::vector<PairedDevice> LoadLocked(_In_ std::wstring const& path)
        {
            std::vector<PairedDevice> devices{};

            auto const text = ReadText(path);

            if (text.empty())
            {
                return devices;
            }

            mjson::JsonObject root{ nullptr };

            if (!mjson::JsonObject::TryParse(text, root) || root == nullptr)
            {
                return devices;
            }

            if (!root.HasKey(KeyDevices) || root.GetNamedValue(KeyDevices).ValueType() != mjson::JsonValueType::Array)
            {
                return devices;
            }

            for (auto const& entry : root.GetNamedArray(KeyDevices))
            {
                if (devices.size() >= MaximumPairedDevices)
                {
                    break;
                }

                if (entry.ValueType() != mjson::JsonValueType::Object)
                {
                    continue;
                }

                auto const value = entry.GetObject();

                if (!value.HasKey(KeyKey) || value.GetNamedValue(KeyKey).ValueType() != mjson::JsonValueType::String)
                {
                    continue;
                }

                auto const key = DeviceKeyHashFromHex(value.GetNamedString(KeyKey));

                if (!key || std::any_of(devices.begin(), devices.end(), [&key](PairedDevice const& d) { return SameDeviceKey(d.Key, *key); }))
                {
                    continue;
                }

                PairedDevice device{};
                device.Key = *key;

                if (value.HasKey(KeyName) && value.GetNamedValue(KeyName).ValueType() == mjson::JsonValueType::String)
                {
                    device.Name = CleanDeviceLinkName(value.GetNamedString(KeyName));
                }

                device.PairedAt = TimeOf(value, KeyPairedAt);
                device.LastUsed = TimeOf(value, KeyLastUsed);

                devices.push_back(std::move(device));
            }

            std::stable_sort(devices.begin(), devices.end(), [](PairedDevice const& a, PairedDevice const& b)
                {
                    return std::max(a.LastUsed, a.PairedAt) > std::max(b.LastUsed, b.PairedAt);
                });

            return devices;
        }
    }

    int64_t CurrentFileTime() noexcept
    {
        FILETIME now{};
        ::GetSystemTimeAsFileTime(&now);

        return static_cast<int64_t>((static_cast<uint64_t>(now.dwHighDateTime) << 32) | now.dwLowDateTime);
    }

    std::vector<PairedDevice> PairedDeviceList::Load() const noexcept
    {
        try
        {
            std::scoped_lock guard{ g_lock };
            return LoadLocked(m_filePath);
        }
        catch (...)
        {
            return {};
        }
    }

    _Use_decl_annotations_
    std::optional<PairedDevice> PairedDeviceList::Find(DeviceKeyHash const& key) const noexcept
    {
        for (auto const& device : Load())
        {
            if (SameDeviceKey(device.Key, key))
            {
                return device;
            }
        }

        return std::nullopt;
    }

    _Use_decl_annotations_
    bool PairedDeviceList::Remember(DeviceKeyHash const& key, std::wstring const& name) noexcept
    {
        try
        {
            std::scoped_lock guard{ g_lock };

            auto devices = LoadLocked(m_filePath);

            std::erase_if(devices, [&key](PairedDevice const& d) { return SameDeviceKey(d.Key, key); });

            PairedDevice device{};
            device.Key = key;
            device.Name = CleanDeviceLinkName(name);
            device.PairedAt = CurrentFileTime();
            device.LastUsed = device.PairedAt;

            devices.insert(devices.begin(), std::move(device));

            if (devices.size() > MaximumPairedDevices)
            {
                devices.resize(MaximumPairedDevices);
            }

            return Save(devices);
        }
        catch (...)
        {
            return false;
        }
    }

    _Use_decl_annotations_
    bool PairedDeviceList::Touch(DeviceKeyHash const& key, std::wstring const& name) noexcept
    {
        try
        {
            std::scoped_lock guard{ g_lock };

            auto devices = LoadLocked(m_filePath);

            auto const found = std::find_if(devices.begin(), devices.end(), [&key](PairedDevice const& d) { return SameDeviceKey(d.Key, key); });

            if (found == devices.end())
            {
                return false;
            }

            found->LastUsed = CurrentFileTime();

            if (auto const clean = CleanDeviceLinkName(name); !clean.empty())
            {
                found->Name = clean;
            }

            return Save(devices);
        }
        catch (...)
        {
            return false;
        }
    }

    _Use_decl_annotations_
    bool PairedDeviceList::Forget(DeviceKeyHash const& key) noexcept
    {
        try
        {
            std::scoped_lock guard{ g_lock };

            auto devices = LoadLocked(m_filePath);

            if (std::erase_if(devices, [&key](PairedDevice const& d) { return SameDeviceKey(d.Key, key); }) == 0)
            {
                return true;
            }

            return Save(devices);
        }
        catch (...)
        {
            return false;
        }
    }

    _Use_decl_annotations_
    bool PairedDeviceList::Save(std::vector<PairedDevice> const& devices) const noexcept
    {
        try
        {
            mjson::JsonArray list{};

            for (auto const& device : devices)
            {
                mjson::JsonObject entry{};
                entry.SetNamedValue(KeyKey, mjson::JsonValue::CreateStringValue(DeviceKeyHashToHex(device.Key)));
                entry.SetNamedValue(KeyName, mjson::JsonValue::CreateStringValue(device.Name));
                entry.SetNamedValue(KeyPairedAt, mjson::JsonValue::CreateStringValue(std::to_wstring(device.PairedAt)));
                entry.SetNamedValue(KeyLastUsed, mjson::JsonValue::CreateStringValue(std::to_wstring(device.LastUsed)));

                list.Append(entry);
            }

            mjson::JsonObject root{};
            root.SetNamedValue(KeyFormatVersion, mjson::JsonValue::CreateNumberValue(FormatVersion));
            root.SetNamedValue(KeyDevices, list);

            auto const utf8 = DeviceLinkUtf8(root.Stringify());

            std::filesystem::path const path{ m_filePath };

            std::error_code ec{};
            std::filesystem::create_directories(path.parent_path(), ec);

            auto temporary = path;
            temporary += L".writing";

            {
                std::ofstream file{ temporary, std::ios::binary | std::ios::trunc };

                if (!file)
                {
                    return false;
                }

                file.write(utf8.data(), static_cast<std::streamsize>(utf8.size()));

                if (!file.good())
                {
                    file.close();
                    std::filesystem::remove(temporary, ec);
                    return false;
                }
            }

            if (!::MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            {
                std::filesystem::remove(temporary, ec);
                return false;
            }

            return true;
        }
        catch (...)
        {
            return false;
        }
    }
}
