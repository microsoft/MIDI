// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Deliberately free of pch.h and XAML, like the rest of the document layer.

#include "SignedItems.h"
#include "ContentPack.h"

#include <windows.h>
#include <shlobj_core.h>

// windows.h defines this as GetObjectW, which turns IJsonValue::GetObject() into a compile error.
#undef GetObject

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <mutex>

namespace glass
{
    namespace
    {
        namespace mjson = winrt::Windows::Data::Json;

        constexpr wchar_t KeyItems[] = L"items";
        constexpr wchar_t KeyPath[] = L"path";
        constexpr wchar_t KeySigner[] = L"signer";
        constexpr wchar_t KeyIssuer[] = L"issuer";
        constexpr wchar_t KeyThumbprint[] = L"thumbprint";
        constexpr wchar_t KeySignedAt[] = L"signedAt";
        constexpr wchar_t KeyFiles[] = L"files";
        constexpr wchar_t KeySize[] = L"size";
        constexpr wchar_t KeySha256[] = L"sha256";

        constexpr size_t MaximumItems = 512;

        // A video is checked by its size alone. Hashing gigabytes every time the library is
        // drawn would cost more than the check is worth on the customer's own PC.
        constexpr uint64_t MaximumHashedBytes = 64ull * 1024 * 1024;

        std::mutex g_lock{};
        std::wstring g_overridePath{};

        std::wstring ListPath()
        {
            if (!g_overridePath.empty())
            {
                return g_overridePath;
            }

            PWSTR raw{ nullptr };

            if (FAILED(::SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DEFAULT, nullptr, &raw)))
            {
                return {};
            }

            std::filesystem::path folder{ raw };
            ::CoTaskMemFree(raw);

            folder /= L"Microsoft";
            folder /= L"MIDI Glass";

            std::error_code ignored{};
            std::filesystem::create_directories(folder, ignored);

            return (folder / L"SignedItems.json").wstring();
        }

        bool SamePath(_In_ std::wstring const& left, _In_ std::wstring const& right) noexcept
        {
            return ::CompareStringOrdinal(left.c_str(), -1, right.c_str(), -1, TRUE) == CSTR_EQUAL;
        }

        std::wstring TextOf(_In_ mjson::JsonObject const& object, _In_ wchar_t const* key)
        {
            auto const value = object.TryLookup(key);

            return (value != nullptr && value.ValueType() == mjson::JsonValueType::String)
                ? std::wstring{ value.GetString() }
                : std::wstring{};
        }

        double NumberOf(_In_ mjson::JsonObject const& object, _In_ wchar_t const* key)
        {
            auto const value = object.TryLookup(key);

            if (value == nullptr || value.ValueType() != mjson::JsonValueType::Number)
            {
                return 0;
            }

            auto const number = value.GetNumber();

            return std::isfinite(number) && number >= 0 ? number : 0;
        }

        std::vector<SignedItem> Load()
        {
            std::vector<SignedItem> items{};

            auto const path = ListPath();

            if (path.empty())
            {
                return items;
            }

            std::ifstream file{ std::filesystem::path{ path }, std::ios::binary };

            if (!file)
            {
                return items;
            }

            std::string bytes{ std::istreambuf_iterator<char>{ file }, std::istreambuf_iterator<char>{} };

            if (bytes.empty() || bytes.size() > 16 * 1024 * 1024)
            {
                return items;
            }

            auto const needed = ::MultiByteToWideChar(
                CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(), static_cast<int>(bytes.size()), nullptr, 0);

            if (needed <= 0)
            {
                return items;
            }

            std::wstring text(static_cast<size_t>(needed), L'\0');
            ::MultiByteToWideChar(
                CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(), static_cast<int>(bytes.size()), text.data(), needed);

            mjson::JsonObject root{ nullptr };

            if (!mjson::JsonObject::TryParse(winrt::hstring{ text }, root) || root == nullptr)
            {
                return items;
            }

            auto const list = root.TryLookup(KeyItems);

            if (list == nullptr || list.ValueType() != mjson::JsonValueType::Array)
            {
                return items;
            }

            for (auto const& entry : list.GetArray())
            {
                if (entry.ValueType() != mjson::JsonValueType::Object || items.size() >= MaximumItems)
                {
                    continue;
                }

                auto const object = entry.GetObject();

                SignedItem item{};
                item.FilePath = TextOf(object, KeyPath);
                item.SignerName = TextOf(object, KeySigner);
                item.IssuerName = TextOf(object, KeyIssuer);
                item.Thumbprint = TextOf(object, KeyThumbprint);
                item.SignedAt = static_cast<int64_t>(NumberOf(object, KeySignedAt));

                auto const files = object.TryLookup(KeyFiles);

                if (files != nullptr && files.ValueType() == mjson::JsonValueType::Array)
                {
                    for (auto const& fileEntry : files.GetArray())
                    {
                        if (fileEntry.ValueType() != mjson::JsonValueType::Object)
                        {
                            continue;
                        }

                        auto const fileObject = fileEntry.GetObject();

                        SignedItem::InstalledFile installed{};
                        installed.Path = TextOf(fileObject, KeyPath);
                        installed.Size = static_cast<uint64_t>(NumberOf(fileObject, KeySize));
                        installed.Sha256 = TextOf(fileObject, KeySha256);

                        if (!installed.Path.empty())
                        {
                            item.Files.push_back(std::move(installed));
                        }
                    }
                }

                if (!item.FilePath.empty() && !item.Files.empty())
                {
                    items.push_back(std::move(item));
                }
            }

            return items;
        }

        void Save(_In_ std::vector<SignedItem> const& items)
        {
            auto const path = ListPath();

            if (path.empty())
            {
                return;
            }

            mjson::JsonArray list{};

            for (auto const& item : items)
            {
                mjson::JsonObject object{};
                object.Insert(KeyPath, mjson::JsonValue::CreateStringValue(item.FilePath));
                object.Insert(KeySigner, mjson::JsonValue::CreateStringValue(item.SignerName));
                object.Insert(KeyIssuer, mjson::JsonValue::CreateStringValue(item.IssuerName));
                object.Insert(KeyThumbprint, mjson::JsonValue::CreateStringValue(item.Thumbprint));
                object.Insert(KeySignedAt, mjson::JsonValue::CreateNumberValue(static_cast<double>(item.SignedAt)));

                mjson::JsonArray files{};

                for (auto const& installed : item.Files)
                {
                    mjson::JsonObject fileObject{};
                    fileObject.Insert(KeyPath, mjson::JsonValue::CreateStringValue(installed.Path));
                    fileObject.Insert(KeySize, mjson::JsonValue::CreateNumberValue(static_cast<double>(installed.Size)));
                    fileObject.Insert(KeySha256, mjson::JsonValue::CreateStringValue(installed.Sha256));
                    files.Append(fileObject);
                }

                object.Insert(KeyFiles, files);
                list.Append(object);
            }

            mjson::JsonObject root{};
            root.Insert(KeyItems, list);

            auto const text = std::wstring{ root.Stringify() };

            auto const needed = ::WideCharToMultiByte(
                CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);

            if (needed <= 0)
            {
                return;
            }

            std::string bytes(static_cast<size_t>(needed), '\0');
            ::WideCharToMultiByte(
                CP_UTF8, 0, text.data(), static_cast<int>(text.size()), bytes.data(), needed, nullptr, nullptr);

            auto temporary = std::filesystem::path{ path };
            temporary += L".writing";

            {
                std::ofstream file{ temporary, std::ios::binary | std::ios::trunc };

                if (!file)
                {
                    return;
                }

                file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));

                if (!file.good())
                {
                    file.close();
                    std::error_code ignored{};
                    std::filesystem::remove(temporary, ignored);
                    return;
                }
            }

            std::error_code ec{};
            std::filesystem::rename(temporary, std::filesystem::path{ path }, ec);

            if (ec)
            {
                std::filesystem::remove(temporary, ec);
            }
        }

        bool Measure(_In_ std::wstring const& path, _Out_ uint64_t& size, _Out_ std::wstring& sha256)
        {
            size = 0;
            sha256.clear();

            std::error_code ec{};
            size = std::filesystem::file_size(std::filesystem::path{ path }, ec);

            if (ec)
            {
                size = 0;
                return false;
            }

            if (size > MaximumHashedBytes)
            {
                return true;
            }

            std::ifstream file{ std::filesystem::path{ path }, std::ios::binary };

            if (!file)
            {
                return false;
            }

            std::vector<uint8_t> bytes{ std::istreambuf_iterator<char>{ file }, std::istreambuf_iterator<char>{} };

            if (bytes.size() != size)
            {
                return false;
            }

            sha256 = midiapp::Sha256Hex(bytes.data(), bytes.size());

            return !sha256.empty();
        }
    }

    _Use_decl_annotations_
    void RememberSignedItem(SignedItem const& item, std::vector<std::wstring> const& installedFiles) noexcept
    {
        try
        {
            SignedItem remembered = item;
            remembered.Files.clear();

            for (auto const& path : installedFiles)
            {
                SignedItem::InstalledFile installed{};
                installed.Path = path;

                if (!Measure(path, installed.Size, installed.Sha256))
                {
                    return;
                }

                remembered.Files.push_back(std::move(installed));
            }

            if (remembered.FilePath.empty() || remembered.Files.empty())
            {
                return;
            }

            std::lock_guard const guard{ g_lock };

            auto items = Load();

            items.erase(std::remove_if(items.begin(), items.end(),
                [&remembered](SignedItem const& existing) { return SamePath(existing.FilePath, remembered.FilePath); }),
                items.end());

            items.push_back(std::move(remembered));

            // The oldest go first.
            if (items.size() > MaximumItems)
            {
                items.erase(items.begin(), items.begin() + static_cast<std::ptrdiff_t>(items.size() - MaximumItems));
            }

            Save(items);
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    void ForgetSignedItem(std::wstring const& filePath) noexcept
    {
        try
        {
            std::lock_guard const guard{ g_lock };

            auto items = Load();
            auto const before = items.size();

            items.erase(std::remove_if(items.begin(), items.end(),
                [&filePath](SignedItem const& existing) { return SamePath(existing.FilePath, filePath); }),
                items.end());

            if (items.size() != before)
            {
                Save(items);
            }
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    std::optional<SignedItem> SignedItemFor(std::wstring const& filePath) noexcept
    {
        try
        {
            if (filePath.empty())
            {
                return std::nullopt;
            }

            std::optional<SignedItem> found{};

            {
                std::lock_guard const guard{ g_lock };

                for (auto& item : Load())
                {
                    if (SamePath(item.FilePath, filePath))
                    {
                        found = std::move(item);
                        break;
                    }
                }
            }

            if (!found.has_value())
            {
                return std::nullopt;
            }

            for (auto const& installed : found->Files)
            {
                uint64_t size{ 0 };
                std::wstring sha256{};

                if (!Measure(installed.Path, size, sha256) || size != installed.Size || sha256 != installed.Sha256)
                {
                    return std::nullopt;
                }
            }

            return found;
        }
        catch (...)
        {
            return std::nullopt;
        }
    }

    _Use_decl_annotations_
    void UseSignedItemsFile(std::wstring const& path) noexcept
    {
        try
        {
            std::lock_guard const guard{ g_lock };
            g_overridePath = path;
        }
        catch (...)
        {
        }
    }
}
