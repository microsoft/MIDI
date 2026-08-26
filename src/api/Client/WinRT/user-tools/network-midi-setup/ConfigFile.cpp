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

#include "..\..\..\..\Transport\UdpNetworkMidi2Transport\network_json_defs.h"

namespace midinetworksetup
{
    namespace
    {
        // Braced uppercase, which is the form the configuration file keys transport sections by.
        // StringFromGUID2 already produces exactly that.
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

        // The transport section is keyed by the braced uppercase form of the transport id, and
        // the entries inside it by the unbraced lowercase form of their own identifier. The two
        // forms are not interchangeable, so both are produced from the same guid deliberately.
        winrt::hstring TransportSectionKey() noexcept
        {
            try
            {
                return BracedUppercaseGuid(midi2net::MidiNetworkTransportManager::TransportId());
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
                return nullptr;
            }
        }

        // Entry identifiers are guids, and JSON keys are case sensitive. A file written by this
        // tool always uses one form, but a hand-edited one may not, so the key is matched the
        // way a person would expect and the real key is handed back.
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

                auto const wanted = LoweredTrimmed(key);

                for (auto const& pair : parent)
                {
                    if (LoweredTrimmed(pair.Key()) == wanted)
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

        bool IdentityMatches(
            _In_ json::JsonObject const& entry,
            _In_ winrt::hstring const& umpEndpointName,
            _In_ winrt::hstring const& productInstanceId) noexcept
        {
            try
            {
                auto const entryName = entry.GetNamedString(MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_IDENTITY_NAME_KEY, L"");
                auto const entryProductInstanceId = entry.GetNamedString(MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_IDENTITY_PRODUCT_INSTANCE_ID_KEY, L"");

                // the service keys a remote client on the name and product instance id pair,
                // compared without case, so the file has to agree with it
                return
                    LoweredTrimmed(entryName) == LoweredTrimmed(umpEndpointName) &&
                    LoweredTrimmed(entryProductInstanceId) == LoweredTrimmed(productInstanceId);
            }
            catch (...)
            {
                return false;
            }
        }

        // Returns a new array with any entry matching the identity left out.
        json::JsonArray WithoutIdentity(
            _In_ json::JsonArray const& source,
            _In_ winrt::hstring const& umpEndpointName,
            _In_ winrt::hstring const& productInstanceId,
            _Out_ bool& removedAny) noexcept
        {
            removedAny = false;

            json::JsonArray result{};

            try
            {
                if (source == nullptr)
                {
                    return result;
                }

                for (auto const& value : source)
                {
                    if (value != nullptr && value.ValueType() == json::JsonValueType::Object &&
                        IdentityMatches(value.GetObject(), umpEndpointName, productInstanceId))
                    {
                        removedAny = true;
                        continue;
                    }

                    result.Append(value);
                }
            }
            catch (...)
            {
            }

            return result;
        }

        // A change to one host is expressed as the smallest section which identifies it, so the
        // SDK merges it into whatever the file currently holds rather than replacing the lot.
        json::JsonObject BuildHostSection(
            _In_ winrt::hstring const& hostKey,
            _In_ json::JsonObject const& hostChange) noexcept
        {
            try
            {
                json::JsonObject hosts{};
                hosts.SetNamedValue(hostKey, hostChange);

                json::JsonObject createObject{};
                createObject.SetNamedValue(MIDI_CONFIG_JSON_NETWORK_MIDI_HOSTS_KEY, hosts);

                json::JsonObject section{};
                section.SetNamedValue(MIDI_CONFIG_JSON_ENDPOINT_COMMON_CREATE_KEY, createObject);

                return section;
            }
            catch (...)
            {
                return nullptr;
            }
        }
    }


    NetworkConfigFile& NetworkConfigFile::Current() noexcept
    {
        static NetworkConfigFile instance{};

        return instance;
    }

    NetworkConfigFile::NetworkConfigFile() noexcept
    {
        ResolveDefaultPath();
    }

    void NetworkConfigFile::ResolveDefaultPath() noexcept
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
    void NetworkConfigFile::OverridePath(std::wstring const& path) noexcept
    {
#ifdef _DEBUG
        if (path.empty())
        {
            return;
        }

        m_path = path;
        m_isOverridden = true;
        m_cachedConfig = nullptr;

        try
        {
            midi2svc::MidiServiceTransportPluginConfigManager::ConfigFilePathOverride(winrt::hstring{ path });
        }
        catch (...)
        {
        }
#else
        // Saving goes through the SDK, which only ever writes the file this PC is configured to
        // use. Honoring the override here would read one file and write another.
        UNREFERENCED_PARAMETER(path);
#endif
    }

    bool NetworkConfigFile::Exists() const noexcept
    {
        if (m_path.empty())
        {
            return false;
        }

        auto const attributes = ::GetFileAttributesW(m_path.c_str());

        return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
    }

    _Use_decl_annotations_
    bool NetworkConfigFile::Load(json::JsonObject& config) noexcept
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
    bool NetworkConfigFile::LoadCached(json::JsonObject& config) noexcept
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
    json::JsonObject NetworkConfigFile::GetEntriesObject(
        json::JsonObject const& config,
        std::wstring_view const entriesKey,
        bool const create) noexcept
    {
        auto const transportKey = TransportSectionKey();

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

            auto createSection = EnsureObject(transport, MIDI_CONFIG_JSON_ENDPOINT_COMMON_CREATE_KEY);
            if (createSection == nullptr) return nullptr;

            return EnsureObject(createSection, entriesKey);
        }

        auto settings = FindObject(config, MIDI_CONFIG_JSON_TRANSPORT_PLUGIN_SETTINGS_OBJECT);
        auto transport = FindObject(settings, transportKey);
        auto createSection = FindObject(transport, MIDI_CONFIG_JSON_ENDPOINT_COMMON_CREATE_KEY);

        return FindObject(createSection, entriesKey);
    }

    _Use_decl_annotations_
    bool NetworkConfigFile::SaveSection(json::JsonObject const& transportSection) noexcept
    {
        if (transportSection == nullptr)
        {
            return false;
        }

        // The SDK re-reads and merges under its own write lock, so nothing this tool read
        // earlier can be written back over a change another program made in the meantime.
        auto const response = midi2svc::MidiServiceTransportPluginConfigManager::SaveUpdate(
            midi2net::MidiNetworkTransportManager::TransportId(),
            transportSection);

        if (response == nullptr || !response.Success())
        {
            m_lastError = response == nullptr ? resources::GetString(L"ConfigFileNoPathError") : response.ErrorMessage();

            return false;
        }

        m_lastError = winrt::hstring{};
        m_cachedConfig = nullptr;

        return true;
    }

    _Use_decl_annotations_
    bool NetworkConfigFile::MergeSection(json::JsonObject const& wrappedSection) noexcept
    {
        // wrappedSection comes from the SDK creation config already wrapped from the root, and
        // SaveUpdate takes it in that form or as a bare section
        return SaveSection(wrappedSection);
    }

    _Use_decl_annotations_
    bool NetworkConfigFile::RemoveHost(winrt::hstring const& hostIdKey) noexcept
    {
        return RemoveEntry(MIDI_CONFIG_JSON_NETWORK_MIDI_HOSTS_KEY, hostIdKey);
    }

    _Use_decl_annotations_
    bool NetworkConfigFile::RemoveClient(winrt::hstring const& clientIdKey) noexcept
    {
        return RemoveEntry(MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENTS_KEY, clientIdKey);
    }

    _Use_decl_annotations_
    bool NetworkConfigFile::RemoveEntry(
        std::wstring_view const entriesKey,
        winrt::hstring const& entryIdKey) noexcept
    {
        json::JsonObject config{ nullptr };

        if (!Load(config))
        {
            return false;
        }

        auto entries = GetEntriesObject(config, entriesKey, false);

        auto const actualKey = ResolveKey(entries, entryIdKey);

        if (entries == nullptr || actualKey.empty())
        {
            // nothing to remove is a success: the live entry is already gone
            return true;
        }

        try
        {
            // A removal names what to delete and mirrors the shape of "create". The leaf is an
            // empty object, which is what marks the entry itself as the thing being removed.
            json::JsonObject entryLeaf{};
            entryLeaf.SetNamedValue(actualKey, json::JsonObject{});

            json::JsonObject removeObject{};
            removeObject.SetNamedValue(winrt::hstring{ entriesKey }, entryLeaf);

            json::JsonObject section{};
            section.SetNamedValue(MIDI_CONFIG_JSON_ENDPOINT_COMMON_REMOVE_KEY, removeObject);

            return SaveSection(section);
        }
        catch (...)
        {
            return false;
        }
    }

    _Use_decl_annotations_
    bool NetworkConfigFile::HasHostEntry(winrt::hstring const& hostIdKey) noexcept
    {
        json::JsonObject config{ nullptr };

        if (!LoadCached(config))
        {
            return false;
        }

        auto hosts = GetEntriesObject(config, MIDI_CONFIG_JSON_NETWORK_MIDI_HOSTS_KEY, false);

        return hosts != nullptr && !ResolveKey(hosts, hostIdKey).empty();
    }

    _Use_decl_annotations_
    bool NetworkConfigFile::SetRemoteClientDecision(
        winrt::hstring const& hostIdKey,
        winrt::hstring const& umpEndpointName,
        winrt::hstring const& productInstanceId,
        bool const allowed) noexcept
    {
        json::JsonObject config{ nullptr };

        if (!Load(config))
        {
            return false;
        }

        auto hosts = GetEntriesObject(config, MIDI_CONFIG_JSON_NETWORK_MIDI_HOSTS_KEY, false);

        auto host = FindObject(hosts, ResolveKey(hosts, hostIdKey));

        if (host == nullptr)
        {
            // the decision was still applied live; there is simply no entry to persist it in
            m_lastError = resources::GetString(L"ConfigFileHostEntryMissingError");
            return false;
        }

        try
        {
            bool ignored{ false };

            auto const allowedList = host.HasKey(MIDI_CONFIG_JSON_NETWORK_MIDI_ALLOWED_CLIENTS_KEY) ?
                host.GetNamedArray(MIDI_CONFIG_JSON_NETWORK_MIDI_ALLOWED_CLIENTS_KEY, json::JsonArray{}) : json::JsonArray{};

            auto const deniedList = host.HasKey(MIDI_CONFIG_JSON_NETWORK_MIDI_DENIED_CLIENTS_KEY) ?
                host.GetNamedArray(MIDI_CONFIG_JSON_NETWORK_MIDI_DENIED_CLIENTS_KEY, json::JsonArray{}) : json::JsonArray{};

            // a client belongs to exactly one of the two lists, so it is removed from both and
            // then added back to the one which now applies
            auto newAllowed = WithoutIdentity(allowedList, umpEndpointName, productInstanceId, ignored);
            auto newDenied = WithoutIdentity(deniedList, umpEndpointName, productInstanceId, ignored);

            json::JsonObject identity{};
            identity.SetNamedValue(
                MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_IDENTITY_NAME_KEY,
                json::JsonValue::CreateStringValue(umpEndpointName));
            identity.SetNamedValue(
                MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_IDENTITY_PRODUCT_INSTANCE_ID_KEY,
                json::JsonValue::CreateStringValue(productInstanceId));

            if (allowed)
            {
                newAllowed.Append(identity);
            }
            else
            {
                newDenied.Append(identity);
            }

            // Both lists are written whole. An empty one is written as an empty array rather
            // than removed, because a merge cannot delete a key, and the service reads the two
            // the same way either way.
            json::JsonObject hostChange{};
            hostChange.SetNamedValue(MIDI_CONFIG_JSON_NETWORK_MIDI_ALLOWED_CLIENTS_KEY, newAllowed);
            hostChange.SetNamedValue(MIDI_CONFIG_JSON_NETWORK_MIDI_DENIED_CLIENTS_KEY, newDenied);

            return SaveSection(BuildHostSection(ResolveKey(hosts, hostIdKey), hostChange));
        }
        catch (...)
        {
            m_lastError = resources::FormatString(L"ConfigFileWriteError", m_path);
            return false;
        }
    }

    _Use_decl_annotations_
    bool NetworkConfigFile::ForgetRemoteClient(
        winrt::hstring const& hostIdKey,
        winrt::hstring const& umpEndpointName,
        winrt::hstring const& productInstanceId) noexcept
    {
        json::JsonObject config{ nullptr };

        if (!Load(config))
        {
            return false;
        }

        auto hosts = GetEntriesObject(config, MIDI_CONFIG_JSON_NETWORK_MIDI_HOSTS_KEY, false);

        auto host = FindObject(hosts, ResolveKey(hosts, hostIdKey));

        if (host == nullptr)
        {
            return true;
        }

        try
        {
            bool removedFromAllowed{ false };
            bool removedFromDenied{ false };

            json::JsonObject hostChange{};

            if (host.HasKey(MIDI_CONFIG_JSON_NETWORK_MIDI_ALLOWED_CLIENTS_KEY))
            {
                auto updated = WithoutIdentity(
                    host.GetNamedArray(MIDI_CONFIG_JSON_NETWORK_MIDI_ALLOWED_CLIENTS_KEY, json::JsonArray{}),
                    umpEndpointName,
                    productInstanceId,
                    removedFromAllowed);

                if (removedFromAllowed)
                {
                    hostChange.SetNamedValue(MIDI_CONFIG_JSON_NETWORK_MIDI_ALLOWED_CLIENTS_KEY, updated);
                }
            }

            if (host.HasKey(MIDI_CONFIG_JSON_NETWORK_MIDI_DENIED_CLIENTS_KEY))
            {
                auto updated = WithoutIdentity(
                    host.GetNamedArray(MIDI_CONFIG_JSON_NETWORK_MIDI_DENIED_CLIENTS_KEY, json::JsonArray{}),
                    umpEndpointName,
                    productInstanceId,
                    removedFromDenied);

                if (removedFromDenied)
                {
                    hostChange.SetNamedValue(MIDI_CONFIG_JSON_NETWORK_MIDI_DENIED_CLIENTS_KEY, updated);
                }
            }

            if (!removedFromAllowed && !removedFromDenied)
            {
                return true;
            }

            return SaveSection(BuildHostSection(ResolveKey(hosts, hostIdKey), hostChange));
        }
        catch (...)
        {
            m_lastError = resources::FormatString(L"ConfigFileWriteError", m_path);
            return false;
        }
    }

    _Use_decl_annotations_
    std::vector<KnownClientEntry> NetworkConfigFile::GetKnownClients(winrt::hstring const& hostIdKey) noexcept
    {
        std::vector<KnownClientEntry> results{};

        json::JsonObject config{ nullptr };

        if (!LoadCached(config))
        {
            return results;
        }

        auto hosts = GetEntriesObject(config, MIDI_CONFIG_JSON_NETWORK_MIDI_HOSTS_KEY, false);

        auto host = FindObject(hosts, ResolveKey(hosts, hostIdKey));

        if (host == nullptr)
        {
            return results;
        }

        try
        {
            auto const readList = [&](std::wstring_view const key, bool const allowed)
                {
                    if (!host.HasKey(key))
                    {
                        return;
                    }

                    for (auto const& value : host.GetNamedArray(key, json::JsonArray{}))
                    {
                        if (value == nullptr || value.ValueType() != json::JsonValueType::Object)
                        {
                            continue;
                        }

                        auto const entry = value.GetObject();

                        KnownClientEntry known{};
                        known.UmpEndpointName = entry.GetNamedString(MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_IDENTITY_NAME_KEY, L"");
                        known.ProductInstanceId = entry.GetNamedString(MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_IDENTITY_PRODUCT_INSTANCE_ID_KEY, L"");
                        known.Allowed = allowed;

                        if (known.UmpEndpointName.empty() && known.ProductInstanceId.empty())
                        {
                            continue;
                        }

                        results.push_back(known);
                    }
                };

            readList(MIDI_CONFIG_JSON_NETWORK_MIDI_ALLOWED_CLIENTS_KEY, true);
            readList(MIDI_CONFIG_JSON_NETWORK_MIDI_DENIED_CLIENTS_KEY, false);
        }
        catch (...)
        {
        }

        return results;
    }

    std::unordered_map<std::wstring, winrt::hstring> NetworkConfigFile::GetClientDisplayNames() noexcept
    {
        std::unordered_map<std::wstring, winrt::hstring> results{};

        json::JsonObject config{ nullptr };

        if (!LoadCached(config))
        {
            return results;
        }

        auto clients = GetEntriesObject(config, MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENTS_KEY, false);

        if (clients == nullptr)
        {
            return results;
        }

        try
        {
            for (auto const& pair : clients)
            {
                auto const value = pair.Value();

                if (value == nullptr || value.ValueType() != json::JsonValueType::Object)
                {
                    continue;
                }

                auto const entry = value.GetObject();

                auto name = entry.GetNamedString(MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_MATCH_UMP_ENDPOINT_NAME_KEY, L"");

                if (name.empty())
                {
                    auto const match = FindObject(entry, MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_MATCH_OBJECT_KEY);

                    if (match != nullptr)
                    {
                        name = match.GetNamedString(MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_MATCH_UMP_ENDPOINT_NAME_KEY, L"");
                    }
                }

                if (!name.empty())
                {
                    results.emplace(std::wstring{ pair.Key() }, name);
                }
            }
        }
        catch (...)
        {
        }

        return results;
    }
    std::vector<std::wstring> NetworkConfigFile::GetClientEntryIds() noexcept
    {
        std::vector<std::wstring> results{};

        json::JsonObject config{ nullptr };

        if (!LoadCached(config))
        {
            return results;
        }

        auto clients = GetEntriesObject(config, MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENTS_KEY, false);

        if (clients == nullptr)
        {
            return results;
        }

        try
        {
            for (auto const& pair : clients)
            {
                results.push_back(LoweredTrimmed(pair.Key()));
            }
        }
        catch (...)
        {
        }

        return results;
    }
}
