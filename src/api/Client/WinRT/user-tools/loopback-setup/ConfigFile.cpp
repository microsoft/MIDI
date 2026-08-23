// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "ConfigFile.h"
#include "StringResources.h"

#include "MidiDefs.h"
#include "json_defs.h"

namespace midiloopbacksetup
{
    namespace
    {
        // Neither the service nor the SDK reads this key. It is the tool's own record of the
        // order the customer arranged the list into.
        constexpr wchar_t DisplayOrderKey[] = L"displayOrder";

        // Braced uppercase, which is the form the configuration file keys both transport
        // sections and loopback entries by. StringFromGUID2 already produces exactly that.
        winrt::hstring BracedUppercaseGuid(_In_ winrt::guid const& value) noexcept
        {
            wchar_t buffer[64]{};

            if (::StringFromGUID2(reinterpret_cast<GUID const&>(value), buffer, ARRAYSIZE(buffer)) == 0)
            {
                return {};
            }

            return winrt::hstring{ buffer };
        }

        std::wstring LoweredTrimmed(_In_ winrt::hstring const& value) noexcept
        {
            try
            {
                std::wstring copy{ value };

                auto const first = copy.find_first_not_of(L" \t\r\n");

                if (first == std::wstring::npos)
                {
                    return {};
                }

                auto const last = copy.find_last_not_of(L" \t\r\n");

                copy = copy.substr(first, last - first + 1);

                std::transform(copy.begin(), copy.end(), copy.begin(),
                    [](wchar_t c) { return static_cast<wchar_t>(::towlower(c)); });

                return copy;
            }
            catch (...)
            {
                return {};
            }
        }

        // An entry key may be written braced or unbraced depending on which tool wrote it, so
        // comparisons are done on the bare guid text.
        std::wstring NormalizedEntryId(_In_ winrt::hstring const& value) noexcept
        {
            auto result = LoweredTrimmed(value);

            if (result.size() >= 2 && result.front() == L'{' && result.back() == L'}')
            {
                result = result.substr(1, result.size() - 2);
            }

            return result;
        }

        winrt::hstring TransportSectionKey(_In_ LoopbackKind const kind) noexcept
        {
            try
            {
                return BracedUppercaseGuid(kind == LoopbackKind::BasicLoopback ?
                    midi2bloop::MidiBasicLoopbackManager::TransportId() :
                    midi2loop::MidiLoopbackManager::TransportId());
            }
            catch (...)
            {
                return {};
            }
        }

        json::JsonObject EnsureObject(
            _In_ json::JsonObject const& parent,
            _In_ std::wstring_view const key) noexcept
        {
            try
            {
                if (parent.HasKey(key))
                {
                    auto existing = parent.GetNamedValue(key);

                    if (existing != nullptr && existing.ValueType() == json::JsonValueType::Object)
                    {
                        return existing.GetObject();
                    }
                }

                json::JsonObject created{};
                parent.SetNamedValue(key, created);

                return created;
            }
            catch (...)
            {
                return nullptr;
            }
        }

        json::JsonObject FindObject(
            _In_ json::JsonObject const& parent,
            _In_ std::wstring_view const key) noexcept
        {
            try
            {
                if (parent == nullptr || !parent.HasKey(key))
                {
                    return nullptr;
                }

                auto existing = parent.GetNamedValue(key);

                if (existing == nullptr || existing.ValueType() != json::JsonValueType::Object)
                {
                    return nullptr;
                }

                return existing.GetObject();
            }
            catch (...)
            {
            }

            return nullptr;
        }

        // JSON keys are case sensitive and a guid can be written several ways. A file written
        // by this tool always uses one form, but a hand-edited one may not, so the key is
        // matched the way a person would expect and the real key is handed back.
        winrt::hstring ResolveKey(
            _In_ json::JsonObject const& parent,
            _In_ winrt::hstring const& key) noexcept
        {
            try
            {
                if (parent == nullptr || key.empty())
                {
                    return {};
                }

                if (parent.HasKey(key))
                {
                    return key;
                }

                auto const wanted = NormalizedEntryId(key);

                for (auto const& pair : parent)
                {
                    if (NormalizedEntryId(pair.Key()) == wanted)
                    {
                        return pair.Key();
                    }
                }
            }
            catch (...)
            {
            }

            return {};
        }

        // Deep merge, so adding one loopback cannot discard another transport's settings, the
        // other loopbacks, or anything the customer hand-edited into the file.
        void MergeInto(_In_ json::JsonObject const& target, _In_ json::JsonObject const& source) noexcept
        {
            try
            {
                for (auto const& pair : source)
                {
                    auto const key = pair.Key();
                    auto const sourceValue = pair.Value();

                    if (sourceValue != nullptr &&
                        sourceValue.ValueType() == json::JsonValueType::Object &&
                        target.HasKey(key))
                    {
                        auto existing = target.GetNamedValue(key);

                        if (existing != nullptr && existing.ValueType() == json::JsonValueType::Object)
                        {
                            MergeInto(existing.GetObject(), sourceValue.GetObject());
                            continue;
                        }
                    }

                    target.SetNamedValue(key, sourceValue);
                }
            }
            catch (...)
            {
            }
        }

        // matches the indent the shipped configuration files already use
        constexpr size_t IndentSpaces = 4;

        void AppendIndent(_Inout_ std::wstring& text, _In_ int32_t const depth) noexcept
        {
            text.append(static_cast<size_t>(depth) * IndentSpaces, L' ');
        }

        // Windows.Data.Json only stringifies to a single line. The configuration file is meant
        // to be readable and hand-editable, so it is written back out indented.
        void AppendPretty(
            _Inout_ std::wstring& text,
            _In_ json::IJsonValue const& value,
            _In_ int32_t const depth) noexcept
        {
            try
            {
                if (value == nullptr)
                {
                    text += L"null";
                    return;
                }

                switch (value.ValueType())
                {
                case json::JsonValueType::Object:
                {
                    auto const object = value.GetObject();

                    if (object.Size() == 0)
                    {
                        text += L"{}";
                        return;
                    }

                    text += L"{\n";

                    uint32_t index{ 0 };

                    for (auto const& pair : object)
                    {
                        AppendIndent(text, depth + 1);

                        text += json::JsonValue::CreateStringValue(pair.Key()).Stringify();
                        text += L": ";

                        AppendPretty(text, pair.Value(), depth + 1);

                        if (++index < object.Size())
                        {
                            text += L",";
                        }

                        text += L"\n";
                    }

                    AppendIndent(text, depth);
                    text += L"}";
                    return;
                }

                case json::JsonValueType::Array:
                {
                    auto const array = value.GetArray();

                    if (array.Size() == 0)
                    {
                        text += L"[]";
                        return;
                    }

                    text += L"[\n";

                    for (uint32_t i = 0; i < array.Size(); i++)
                    {
                        AppendIndent(text, depth + 1);

                        AppendPretty(text, array.GetAt(i), depth + 1);

                        if (i + 1 < array.Size())
                        {
                            text += L",";
                        }

                        text += L"\n";
                    }

                    AppendIndent(text, depth);
                    text += L"]";
                    return;
                }

                default:
                    text += value.Stringify();
                    return;
                }
            }
            catch (...)
            {
            }
        }

        std::string ToUtf8(_In_ std::wstring const& text) noexcept
        {
            if (text.empty())
            {
                return {};
            }

            auto const required = ::WideCharToMultiByte(
                CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);

            if (required <= 0)
            {
                return {};
            }

            std::string result(static_cast<size_t>(required), '\0');

            ::WideCharToMultiByte(
                CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), result.data(), required, nullptr, nullptr);

            return result;
        }

        std::wstring FromUtf8(_In_ std::string const& text) noexcept
        {
            if (text.empty())
            {
                return {};
            }

            auto const required = ::MultiByteToWideChar(
                CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);

            if (required <= 0)
            {
                return {};
            }

            std::wstring result(static_cast<size_t>(required), L'\0');

            ::MultiByteToWideChar(
                CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), required);

            return result;
        }

        bool ReadAllBytes(_In_ std::wstring const& path, _Out_ std::string& contents) noexcept
        {
            contents.clear();

            wil::unique_hfile file{ ::CreateFileW(
                path.c_str(),
                GENERIC_READ,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                nullptr,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,
                nullptr) };

            if (!file)
            {
                return false;
            }

            LARGE_INTEGER size{};

            if (!::GetFileSizeEx(file.get(), &size) || size.QuadPart <= 0 || size.QuadPart > 0x400000)
            {
                // an empty or implausibly large file is treated as unreadable rather than parsed
                return size.QuadPart == 0;
            }

            contents.resize(static_cast<size_t>(size.QuadPart));

            DWORD bytesRead{ 0 };

            if (!::ReadFile(file.get(), contents.data(), static_cast<DWORD>(contents.size()), &bytesRead, nullptr))
            {
                contents.clear();
                return false;
            }

            contents.resize(bytesRead);

            // a UTF-8 byte order mark is legal in the file but not in the JSON text
            if (contents.size() >= 3 &&
                static_cast<unsigned char>(contents[0]) == 0xEF &&
                static_cast<unsigned char>(contents[1]) == 0xBB &&
                static_cast<unsigned char>(contents[2]) == 0xBF)
            {
                contents.erase(0, 3);
            }

            return true;
        }

        bool WriteAllBytes(_In_ std::wstring const& path, _In_ std::string const& contents) noexcept
        {
            wil::unique_hfile file{ ::CreateFileW(
                path.c_str(),
                GENERIC_WRITE,
                0,
                nullptr,
                CREATE_ALWAYS,
                FILE_ATTRIBUTE_NORMAL,
                nullptr) };

            if (!file)
            {
                return false;
            }

            DWORD bytesWritten{ 0 };

            if (!::WriteFile(file.get(), contents.data(), static_cast<DWORD>(contents.size()), &bytesWritten, nullptr))
            {
                return false;
            }

            return bytesWritten == contents.size();
        }
    }


    LoopbackConfigFile& LoopbackConfigFile::Current() noexcept
    {
        static LoopbackConfigFile instance{};

        return instance;
    }

    LoopbackConfigFile::LoopbackConfigFile() noexcept
    {
        ResolveDefaultPath();
    }

    void LoopbackConfigFile::ResolveDefaultPath() noexcept
    {
        try
        {
            // the service only ever opens a file inside this folder, so the name is all that
            // is stored in the registry
            std::wstring fileName{ L"WindowsMidiServices.midiconfig.json" };

            wil::unique_hkey key{};

            if (SUCCEEDED(HRESULT_FROM_WIN32(::RegOpenKeyExW(
                HKEY_LOCAL_MACHINE, MIDI_ROOT_REG_KEY, 0, KEY_READ, key.put()))))
            {
                wchar_t buffer[MAX_PATH]{};
                DWORD bufferBytes{ sizeof(buffer) };
                DWORD valueType{ 0 };

                if (::RegQueryValueExW(
                    key.get(),
                    MIDI_CONFIG_FILE_REG_VALUE,
                    nullptr,
                    &valueType,
                    reinterpret_cast<LPBYTE>(buffer),
                    &bufferBytes) == ERROR_SUCCESS && valueType == REG_SZ)
                {
                    std::wstring const value{ buffer };

                    if (!value.empty())
                    {
                        fileName = value;
                    }
                }
            }

            wchar_t folder[MAX_PATH]{};

            auto const expanded = ::ExpandEnvironmentStringsW(MIDI_CONFIG_FILE_FOLDER, folder, ARRAYSIZE(folder));

            if (expanded == 0 || expanded > ARRAYSIZE(folder))
            {
                m_path = fileName;
                return;
            }

            m_path = std::wstring{ folder } + fileName;
        }
        catch (...)
        {
            m_path.clear();
        }
    }

    _Use_decl_annotations_
    void LoopbackConfigFile::OverridePath(std::wstring const& path) noexcept
    {
        if (path.empty())
        {
            return;
        }

        m_path = path;
        m_isOverridden = true;
        m_cachedConfig = nullptr;
    }

    bool LoopbackConfigFile::Exists() const noexcept
    {
        if (m_path.empty())
        {
            return false;
        }

        auto const attributes = ::GetFileAttributesW(m_path.c_str());

        return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
    }

    _Use_decl_annotations_
    bool LoopbackConfigFile::Load(json::JsonObject& config) noexcept
    {
        config = nullptr;

        try
        {
            if (m_path.empty())
            {
                m_lastError = resources::GetString(L"ConfigFileNoPathError");
                return false;
            }

            std::string bytes{};

            if (!ReadAllBytes(m_path, bytes))
            {
                // a missing file is not an error: this tool may be the first thing to write one
                if (!Exists())
                {
                    config = json::JsonObject{};
                    return true;
                }

                m_lastError = resources::FormatString(L"ConfigFileReadError", m_path);
                return false;
            }

            auto const text = FromUtf8(bytes);

            if (text.find_first_not_of(L" \t\r\n") == std::wstring::npos)
            {
                config = json::JsonObject{};
                return true;
            }

            json::JsonObject parsed{ nullptr };

            if (!json::JsonObject::TryParse(winrt::hstring{ text }, parsed) || parsed == nullptr)
            {
                m_lastError = resources::FormatString(L"ConfigFileParseError", m_path);
                return false;
            }

            config = parsed;

            return true;
        }
        catch (...)
        {
            m_lastError = resources::FormatString(L"ConfigFileReadError", m_path);
            return false;
        }
    }

    _Use_decl_annotations_
    bool LoopbackConfigFile::LoadCached(json::JsonObject& config) noexcept
    {
        try
        {
            WIN32_FILE_ATTRIBUTE_DATA attributes{};

            auto const stamped = !m_path.empty() &&
                ::GetFileAttributesExW(m_path.c_str(), GetFileExInfoStandard, &attributes) != FALSE;

            uint64_t const size = stamped ?
                ((static_cast<uint64_t>(attributes.nFileSizeHigh) << 32) | attributes.nFileSizeLow) : 0;

            if (stamped &&
                m_cachedConfig != nullptr &&
                m_cachedSize == size &&
                ::CompareFileTime(&attributes.ftLastWriteTime, &m_cachedWriteTime) == 0)
            {
                config = m_cachedConfig;

                return true;
            }

            if (!Load(config))
            {
                m_cachedConfig = nullptr;

                return false;
            }

            // Without a usable stamp there is nothing to invalidate against, so the parse is not
            // cached rather than being cached and never refreshed.
            if (stamped)
            {
                m_cachedConfig = config;
                m_cachedWriteTime = attributes.ftLastWriteTime;
                m_cachedSize = size;
            }
            else
            {
                m_cachedConfig = nullptr;
            }

            return true;
        }
        catch (...)
        {
            m_cachedConfig = nullptr;

            return false;
        }
    }

    _Use_decl_annotations_
    bool LoopbackConfigFile::Save(json::JsonObject const& config) noexcept
    {
        try
        {
            if (config == nullptr || m_path.empty())
            {
                m_lastError = resources::GetString(L"ConfigFileNoPathError");
                return false;
            }

            std::wstring text{};
            AppendPretty(text, config, 0);
            text += L"\n";

            auto const bytes = ToUtf8(text);

            if (bytes.empty())
            {
                m_lastError = resources::FormatString(L"ConfigFileWriteError", m_path, L"0");
                return false;
            }

            // Written in place rather than swapped in from a temporary file. ReplaceFileW needs
            // delete rights on the replacement, and the ACL on %ProgramData%\Microsoft\MIDI
            // grants Authenticated Users write but not delete, so the temporary file could
            // neither be swapped in nor cleaned up afterwards and simply accumulated beside the
            // real file. The MIDI Settings app writes in place for the same reason.
            if (WriteAllBytes(m_path, bytes))
            {
                m_lastError = winrt::hstring{};

                // the file just changed under the cache
                m_cachedConfig = nullptr;

                return true;
            }

            m_lastError = resources::FormatString(
                L"ConfigFileWriteError",
                m_path,
                winrt::hstring{ std::to_wstring(::GetLastError()) });

            return false;
        }
        catch (...)
        {
            m_lastError = resources::FormatString(L"ConfigFileWriteError", m_path, L"0");
            return false;
        }
    }

    _Use_decl_annotations_
    json::JsonObject LoopbackConfigFile::GetCreateObject(
        json::JsonObject const& config,
        LoopbackKind const kind,
        bool const create) noexcept
    {
        auto const transportKey = TransportSectionKey(kind);

        if (transportKey.empty())
        {
            return nullptr;
        }

        if (create)
        {
            auto settings = EnsureObject(config, MIDI_CONFIG_JSON_TRANSPORT_PLUGIN_SETTINGS_OBJECT);
            if (settings == nullptr) return nullptr;

            auto transport = EnsureObject(settings, transportKey);
            if (transport == nullptr) return nullptr;

            return EnsureObject(transport, MIDI_CONFIG_JSON_ENDPOINT_COMMON_CREATE_KEY);
        }

        auto settings = FindObject(config, MIDI_CONFIG_JSON_TRANSPORT_PLUGIN_SETTINGS_OBJECT);

        // the transport key was written braced uppercase, but a hand edited file may differ
        auto transport = FindObject(settings, ResolveKey(settings, transportKey));

        return FindObject(transport, MIDI_CONFIG_JSON_ENDPOINT_COMMON_CREATE_KEY);
    }

    _Use_decl_annotations_
    bool LoopbackConfigFile::MergeSection(json::JsonObject const& wrappedSection) noexcept
    {
        if (wrappedSection == nullptr)
        {
            return false;
        }

        json::JsonObject config{ nullptr };

        if (!Load(config))
        {
            return false;
        }

        MergeInto(config, wrappedSection);

        return Save(config);
    }

    _Use_decl_annotations_
    bool LoopbackConfigFile::RemoveEntry(LoopbackKind const kind, winrt::hstring const& associationKey) noexcept
    {
        json::JsonObject config{ nullptr };

        if (!Load(config))
        {
            return false;
        }

        auto entries = GetCreateObject(config, kind, false);

        auto const actualKey = ResolveKey(entries, associationKey);

        if (entries == nullptr || actualKey.empty())
        {
            // nothing to remove is a success: the loopback is already gone from the file
            return true;
        }

        try
        {
            entries.Remove(actualKey);
        }
        catch (...)
        {
            return false;
        }

        return Save(config);
    }

    _Use_decl_annotations_
    bool LoopbackConfigFile::SetMuted(
        LoopbackKind const kind,
        winrt::hstring const& associationKey,
        bool const isMuted) noexcept
    {
        json::JsonObject config{ nullptr };

        if (!Load(config))
        {
            return false;
        }

        auto entries = GetCreateObject(config, kind, false);

        auto entry = FindObject(entries, ResolveKey(entries, associationKey));

        if (entry == nullptr)
        {
            // The mute was still applied live. There is simply no entry to persist it in,
            // which is the normal case for a loopback that was created without being saved.
            m_lastError = resources::GetString(L"ConfigFileEntryMissingError");
            return false;
        }

        try
        {
            // A pair keeps the flag on the association, because it mutes both directions at
            // once. A basic loopback keeps it on its single endpoint. This is where the SDK
            // creation config puts them, so the service reads them back from the same place.
            if (kind == LoopbackKind::BasicLoopback)
            {
                auto endpoint = FindObject(entry, MIDI_CONFIG_JSON_ENDPOINT_BASIC_LOOPBACK_DEVICE_ENDPOINT_KEY);

                if (endpoint == nullptr)
                {
                    m_lastError = resources::GetString(L"ConfigFileEntryMissingError");
                    return false;
                }

                endpoint.SetNamedValue(
                    MIDI_CONFIG_JSON_ENDPOINT_COMMON_MUTED_PROPERTY,
                    json::JsonValue::CreateBooleanValue(isMuted));
            }
            else
            {
                entry.SetNamedValue(
                    MIDI_CONFIG_JSON_ENDPOINT_COMMON_MUTED_PROPERTY,
                    json::JsonValue::CreateBooleanValue(isMuted));
            }
        }
        catch (...)
        {
            m_lastError = resources::FormatString(L"ConfigFileWriteError", m_path, L"0");
            return false;
        }

        return Save(config);
    }

    _Use_decl_annotations_
    bool LoopbackConfigFile::SetDisplayOrder(
        LoopbackKind const kind,
        std::vector<winrt::hstring> const& orderedAssociationKeys) noexcept
    {
        json::JsonObject config{ nullptr };

        if (!Load(config))
        {
            return false;
        }

        auto entries = GetCreateObject(config, kind, false);

        if (entries == nullptr)
        {
            return true;
        }

        bool changedAnything{ false };

        try
        {
            int32_t position{ 0 };

            for (auto const& key : orderedAssociationKeys)
            {
                auto entry = FindObject(entries, ResolveKey(entries, key));

                // a loopback which was never saved has no entry to record a position in, and
                // still takes up a position so the saved ones keep their relative order
                if (entry != nullptr)
                {
                    entry.SetNamedValue(
                        DisplayOrderKey,
                        json::JsonValue::CreateNumberValue(static_cast<double>(position)));

                    changedAnything = true;
                }

                position++;
            }
        }
        catch (...)
        {
            return false;
        }

        if (!changedAnything)
        {
            return true;
        }

        return Save(config);
    }

    _Use_decl_annotations_
    std::vector<std::wstring> LoopbackConfigFile::GetEntryIds(LoopbackKind const kind) noexcept
    {
        std::vector<std::wstring> results{};

        json::JsonObject config{ nullptr };

        if (!LoadCached(config))
        {
            return results;
        }

        try
        {
            auto entries = GetCreateObject(config, kind, false);

            if (entries != nullptr)
            {
                for (auto const& pair : entries)
                {
                    auto const id = NormalizedEntryId(pair.Key());

                    if (!id.empty())
                    {
                        results.push_back(id);
                    }
                }
            }
        }
        catch (...)
        {
        }

        return results;
    }

    _Use_decl_annotations_
    std::unordered_map<std::wstring, int32_t> LoopbackConfigFile::GetDisplayOrders(LoopbackKind const kind) noexcept
    {
        std::unordered_map<std::wstring, int32_t> results{};

        json::JsonObject config{ nullptr };

        if (!LoadCached(config))
        {
            return results;
        }

        try
        {
            auto entries = GetCreateObject(config, kind, false);

            if (entries != nullptr)
            {
                for (auto const& pair : entries)
                {
                    auto const id = NormalizedEntryId(pair.Key());

                    if (id.empty())
                    {
                        continue;
                    }

                    auto const value = pair.Value();

                    if (value == nullptr || value.ValueType() != json::JsonValueType::Object)
                    {
                        continue;
                    }

                    auto const entry = value.GetObject();

                    if (!entry.HasKey(DisplayOrderKey))
                    {
                        continue;
                    }

                    auto const stored = entry.GetNamedValue(DisplayOrderKey);

                    if (stored == nullptr || stored.ValueType() != json::JsonValueType::Number)
                    {
                        continue;
                    }

                    results.insert_or_assign(id, static_cast<int32_t>(stored.GetNumber()));
                }
            }
        }
        catch (...)
        {
        }

        return results;
    }
}
