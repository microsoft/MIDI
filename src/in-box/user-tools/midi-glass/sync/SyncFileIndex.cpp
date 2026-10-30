// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Before windows.h, so the JSON projection is declared without the GetObject macro.
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Data.Json.h>

#include "SyncFileIndex.h"

#include "ContentPack.h"
#include "DeviceLinkFrames.h"

#include <windows.h>
#include <bcrypt.h>

#undef GetObject

#include <algorithm>
#include <cwchar>
#include <cwctype>
#include <filesystem>
#include <format>
#include <fstream>

namespace glass::sync
{
    namespace
    {
        namespace mjson = winrt::Windows::Data::Json;

        constexpr wchar_t KeyFormatVersion[] = L"formatVersion";
        constexpr wchar_t KeyFiles[] = L"files";
        constexpr wchar_t KeyPath[] = L"path";
        constexpr wchar_t KeySize[] = L"size";
        constexpr wchar_t KeyWritten[] = L"written";
        constexpr wchar_t KeySha256[] = L"sha256";

        constexpr size_t MaximumIndexedFiles = 4096;
        constexpr uintmax_t MaximumCacheBytes = 4 * 1024 * 1024;
        constexpr DWORD ReadChunkBytes = 1024 * 1024;

        std::wstring Lower(_In_ std::wstring value)
        {
            std::transform(value.begin(), value.end(), value.begin(),
                [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });

            return value;
        }

        bool IsSha256Hex(_In_ std::wstring const& value) noexcept
        {
            return value.size() == 64 && std::all_of(value.begin(), value.end(),
                [](wchar_t c) { return (c >= L'0' && c <= L'9') || (c >= L'a' && c <= L'f'); });
        }

        int64_t ParseDecimal(_In_ std::wstring const& text) noexcept
        {
            if (text.empty() || text.size() > 19 ||
                !std::all_of(text.begin(), text.end(), [](wchar_t c) { return c >= L'0' && c <= L'9'; }))
            {
                return -1;
            }

            return static_cast<int64_t>(std::wcstoll(text.c_str(), nullptr, 10));
        }

        std::wstring StringOf(_In_ mjson::JsonObject const& value, _In_ wchar_t const* key)
        {
            if (!value.HasKey(key) || value.GetNamedValue(key).ValueType() != mjson::JsonValueType::String)
            {
                return {};
            }

            return std::wstring{ value.GetNamedString(key) };
        }

        bool ReadAttributes(_In_ std::wstring const& path, _Out_ uint64_t& size, _Out_ int64_t& written) noexcept
        {
            size = 0;
            written = 0;

            WIN32_FILE_ATTRIBUTE_DATA data{};

            if (!::GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data) ||
                (data.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) != 0)
            {
                return false;
            }

            size = (static_cast<uint64_t>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
            written = static_cast<int64_t>((static_cast<uint64_t>(data.ftLastWriteTime.dwHighDateTime) << 32) | data.ftLastWriteTime.dwLowDateTime);

            return true;
        }
    }

    _Use_decl_annotations_
    std::wstring HashFileSha256(std::wstring const& path, uint64_t maximumBytes) noexcept
    {
        BCRYPT_HASH_HANDLE hash{ nullptr };
        HANDLE file{ INVALID_HANDLE_VALUE };
        std::wstring result{};

        try
        {
            file = ::CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);

            if (file == INVALID_HANDLE_VALUE)
            {
                return {};
            }

            LARGE_INTEGER size{};

            if (!::GetFileSizeEx(file, &size) || static_cast<uint64_t>(size.QuadPart) > maximumBytes ||
                !BCRYPT_SUCCESS(::BCryptCreateHash(BCRYPT_SHA256_ALG_HANDLE, &hash, nullptr, 0, nullptr, 0, 0)))
            {
                ::CloseHandle(file);
                return {};
            }

            std::vector<uint8_t> buffer(ReadChunkBytes);
            uint64_t total{ 0 };
            auto ok = true;

            for (;;)
            {
                DWORD read{ 0 };

                if (!::ReadFile(file, buffer.data(), ReadChunkBytes, &read, nullptr))
                {
                    ok = false;
                    break;
                }

                if (read == 0)
                {
                    break;
                }

                total += read;

                if (total > maximumBytes || !BCRYPT_SUCCESS(::BCryptHashData(hash, buffer.data(), read, 0)))
                {
                    ok = false;
                    break;
                }
            }

            uint8_t digest[32]{};

            if (ok && BCRYPT_SUCCESS(::BCryptFinishHash(hash, digest, sizeof(digest), 0)))
            {
                for (auto const value : digest)
                {
                    result += std::format(L"{:02x}", value);
                }
            }
        }
        catch (...)
        {
            result.clear();
        }

        if (hash != nullptr)
        {
            ::BCryptDestroyHash(hash);
        }

        if (file != INVALID_HANDLE_VALUE)
        {
            ::CloseHandle(file);
        }

        return result;
    }

    _Use_decl_annotations_
    FileHashIndex::FileHashIndex(std::wstring cachePath) noexcept :
        m_cachePath(std::move(cachePath))
    {
    }

    void FileHashIndex::LoadCache() noexcept
    {
        m_loaded = true;

        if (m_cachePath.empty())
        {
            return;
        }

        try
        {
            std::error_code ec{};
            auto const size = std::filesystem::file_size(m_cachePath, ec);

            if (ec || size == 0 || size > MaximumCacheBytes)
            {
                return;
            }

            std::ifstream file{ std::filesystem::path{ m_cachePath }, std::ios::binary };
            std::string bytes{ std::istreambuf_iterator<char>{ file }, std::istreambuf_iterator<char>{} };

            bool valid{ false };
            auto const text = midiapp::DeviceLinkFromUtf8(
                std::span<uint8_t const>{ reinterpret_cast<uint8_t const*>(bytes.data()), bytes.size() }, valid);

            mjson::JsonObject root{ nullptr };

            if (!valid || !mjson::JsonObject::TryParse(text, root) || root == nullptr ||
                !root.HasKey(KeyFiles) || root.GetNamedValue(KeyFiles).ValueType() != mjson::JsonValueType::Array)
            {
                return;
            }

            for (auto const& item : root.GetNamedArray(KeyFiles))
            {
                if (m_entries.size() >= MaximumIndexedFiles)
                {
                    break;
                }

                if (item.ValueType() != mjson::JsonValueType::Object)
                {
                    continue;
                }

                auto const value = item.GetObject();

                Entry entry{};
                entry.Path = StringOf(value, KeyPath);
                entry.Sha256 = StringOf(value, KeySha256);

                auto const fileSize = ParseDecimal(StringOf(value, KeySize));
                auto const written = ParseDecimal(StringOf(value, KeyWritten));

                if (entry.Path.empty() || !IsSha256Hex(entry.Sha256) || fileSize < 0 || written < 0)
                {
                    continue;
                }

                entry.Size = static_cast<uint64_t>(fileSize);
                entry.Written = written;

                m_entries[Lower(entry.Path)] = std::move(entry);
            }
        }
        catch (...)
        {
            m_entries.clear();
        }
    }

    _Use_decl_annotations_
    void FileHashIndex::Refresh(std::vector<std::wstring> const& folders, std::vector<uint64_t> const* onlySizes) noexcept
    {
        try
        {
            if (!m_loaded)
            {
                LoadCache();
            }

            std::map<std::wstring, Entry> current{};

            for (auto const& folder : folders)
            {
                std::error_code ec{};

                if (folder.empty() || !std::filesystem::is_directory(folder, ec))
                {
                    continue;
                }

                for (std::filesystem::directory_iterator it{ folder, ec }, end{}; !ec && it != end; it.increment(ec))
                {
                    if (current.size() >= MaximumIndexedFiles)
                    {
                        break;
                    }

                    auto const path = it->path().wstring();

                    uint64_t size{ 0 };
                    int64_t written{ 0 };

                    if (!ReadAttributes(path, size, written) || size > midiapp::MaximumContentPackBytes)
                    {
                        continue;
                    }

                    auto const key = Lower(path);

                    if (auto const known = m_entries.find(key);
                        known != m_entries.end() && known->second.Size == size && known->second.Written == written)
                    {
                        current[key] = known->second;
                        continue;
                    }

                    if (onlySizes != nullptr && std::find(onlySizes->begin(), onlySizes->end(), size) == onlySizes->end())
                    {
                        continue;
                    }

                    Entry entry{};
                    entry.Path = path;
                    entry.Size = size;
                    entry.Written = written;
                    entry.Sha256 = HashFileSha256(path, midiapp::MaximumContentPackBytes);

                    if (!entry.Sha256.empty())
                    {
                        current[key] = std::move(entry);
                    }
                }
            }

            m_entries = std::move(current);

            m_byHash.clear();

            for (auto const& [key, entry] : m_entries)
            {
                m_byHash.emplace(entry.Sha256, key);
            }
        }
        catch (...)
        {
            m_entries.clear();
            m_byHash.clear();
        }
    }

    _Use_decl_annotations_
    std::wstring FileHashIndex::Find(std::wstring const& sha256) const noexcept
    {
        try
        {
            auto const found = m_byHash.find(sha256);

            if (found == m_byHash.end())
            {
                return {};
            }

            auto const entry = m_entries.find(found->second);

            return entry == m_entries.end() ? std::wstring{} : entry->second.Path;
        }
        catch (...)
        {
            return {};
        }
    }

    bool FileHashIndex::Save() const noexcept
    {
        if (m_cachePath.empty())
        {
            return true;
        }

        try
        {
            mjson::JsonArray files{};

            for (auto const& [key, entry] : m_entries)
            {
                mjson::JsonObject value{};
                value.SetNamedValue(KeyPath, mjson::JsonValue::CreateStringValue(entry.Path));
                value.SetNamedValue(KeySize, mjson::JsonValue::CreateStringValue(std::to_wstring(entry.Size)));
                value.SetNamedValue(KeyWritten, mjson::JsonValue::CreateStringValue(std::to_wstring(entry.Written)));
                value.SetNamedValue(KeySha256, mjson::JsonValue::CreateStringValue(entry.Sha256));

                files.Append(value);
            }

            mjson::JsonObject root{};
            root.SetNamedValue(KeyFormatVersion, mjson::JsonValue::CreateNumberValue(1));
            root.SetNamedValue(KeyFiles, files);

            auto const utf8 = midiapp::DeviceLinkUtf8(root.Stringify());

            std::filesystem::path const path{ m_cachePath };

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

            if (!::MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING))
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
