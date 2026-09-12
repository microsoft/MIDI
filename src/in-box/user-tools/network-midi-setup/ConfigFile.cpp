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

#include "..\..\Transport\UdpNetworkMidi2Transport\network_json_defs.h"

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

        // The service keys a remote client on the name and product instance id pair, compared
        // without case, so the file has to agree with it.
        bool IsSameIdentity(
            _In_ KnownClientEntry const& entry,
            _In_ winrt::hstring const& umpEndpointName,
            _In_ winrt::hstring const& productInstanceId) noexcept
        {
            return
                LoweredTrimmed(entry.UmpEndpointName) == LoweredTrimmed(umpEndpointName) &&
                LoweredTrimmed(entry.ProductInstanceId) == LoweredTrimmed(productInstanceId);
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
        return ApplySaveResponse(
            midi2svc::MidiServiceTransportPluginConfigManager::SaveUpdate(
                midi2net::MidiNetworkTransportManager::TransportId(),
                transportSection));
    }

    _Use_decl_annotations_
    bool NetworkConfigFile::SaveConfig(midi2svc::IMidiServiceTransportPluginConfig const& config) noexcept
    {
        if (config == nullptr)
        {
            return false;
        }

        return ApplySaveResponse(
            midi2svc::MidiServiceTransportPluginConfigManager::SaveUpdate(config));
    }

    _Use_decl_annotations_
    bool NetworkConfigFile::ApplySaveResponse(midi2svc::MidiServiceConfigSaveResponse const& response) noexcept
    {
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
    bool NetworkConfigFile::RemoveHost(winrt::guid const& hostId) noexcept
    {
        // The config object knows the shape of a host removal, so it is not spelled out again
        // here. The SDK's merge matches the entry without case, and dropping a host which is
        // already gone is not an error, so there is no need to read the file first.
        midi2net::MidiNetworkHostRemovalConfig config{};
        config.HostId(hostId);

        return SaveConfig(config);
    }

    _Use_decl_annotations_
    bool NetworkConfigFile::RemoveClient(winrt::hstring const& clientIdKey) noexcept
    {
        // Deliberately not a config object. MidiNetworkClientDisconnectConfig is a command which
        // tears down a live session and is refused by SaveUpdate, and there is no client equivalent
        // of MidiNetworkHostRemovalConfig, so the removal is spelled out here.
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
    winrt::hstring NetworkConfigFile::GetClientMatchProductInstanceId(winrt::hstring const& clientIdKey) noexcept
    {
        json::JsonObject config{ nullptr };

        if (!LoadCached(config))
        {
            return {};
        }

        auto clients = GetEntriesObject(config, MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENTS_KEY, false);

        auto client = FindObject(clients, ResolveKey(clients, clientIdKey));

        if (client == nullptr)
        {
            return {};
        }

        try
        {
            auto const match = client.GetNamedObject(
                MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_MATCH_OBJECT_KEY, nullptr);

            if (match == nullptr)
            {
                return {};
            }

            return match.GetNamedString(
                MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_MATCH_UMP_ENDPOINT_PID_KEY, L"");
        }
        catch (...)
        {
            return {};
        }
    }

    _Use_decl_annotations_
    winrt::hstring NetworkConfigFile::GetClientCustomEndpointName(winrt::hstring const& clientIdKey) noexcept
    {
        json::JsonObject config{ nullptr };

        if (!LoadCached(config))
        {
            return {};
        }

        auto clients = GetEntriesObject(config, MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENTS_KEY, false);

        auto client = FindObject(clients, ResolveKey(clients, clientIdKey));

        if (client == nullptr)
        {
            return {};
        }

        try
        {
            return client.GetNamedString(
                MIDI_CONFIG_JSON_NETWORK_MIDI_CUSTOM_ENDPOINT_NAME_KEY, L"");
        }
        catch (...)
        {
            return {};
        }
    }

    _Use_decl_annotations_
    uint8_t NetworkConfigFile::GetClientFallbackMidi1PortCount(winrt::hstring const& clientIdKey) noexcept
    {
        json::JsonObject config{ nullptr };

        if (!LoadCached(config))
        {
            return MIDI_NETWORK_MIDI_FALLBACK_MIDI1_PORT_COUNT_DEFAULT;
        }

        auto clients = GetEntriesObject(config, MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENTS_KEY, false);

        auto client = FindObject(clients, ResolveKey(clients, clientIdKey));

        if (client == nullptr)
        {
            return MIDI_NETWORK_MIDI_FALLBACK_MIDI1_PORT_COUNT_DEFAULT;
        }

        try
        {
            auto const value = client.GetNamedNumber(
                MIDI_CONFIG_JSON_NETWORK_MIDI_FALLBACK_MIDI1_PORT_COUNT_KEY,
                MIDI_NETWORK_MIDI_FALLBACK_MIDI1_PORT_COUNT_DEFAULT);

            if (value < MIDI_NETWORK_MIDI_FALLBACK_MIDI1_PORT_COUNT_MINIMUM ||
                value > MIDI_NETWORK_MIDI_FALLBACK_MIDI1_PORT_COUNT_MAXIMUM)
            {
                return MIDI_NETWORK_MIDI_FALLBACK_MIDI1_PORT_COUNT_DEFAULT;
            }

            return static_cast<uint8_t>(value);
        }
        catch (...)
        {
            return MIDI_NETWORK_MIDI_FALLBACK_MIDI1_PORT_COUNT_DEFAULT;
        }
    }

    _Use_decl_annotations_
    bool NetworkConfigFile::GetClientCreateMidi1Ports(winrt::hstring const& clientIdKey) noexcept
    {
        json::JsonObject config{ nullptr };

        if (!LoadCached(config))
        {
            return true;
        }

        auto clients = GetEntriesObject(config, MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENTS_KEY, false);

        auto client = FindObject(clients, ResolveKey(clients, clientIdKey));

        if (client == nullptr)
        {
            return true;
        }

        try
        {
            return client.GetNamedBoolean(MIDI_CONFIG_JSON_NETWORK_MIDI_CREATE_MIDI1_PORTS_KEY, true);
        }
        catch (...)
        {
            return true;
        }
    }

    _Use_decl_annotations_
    bool NetworkConfigFile::SetRemoteClientDecision(
        winrt::guid const& hostId,
        winrt::hstring const& umpEndpointName,
        winrt::hstring const& productInstanceId,
        bool const allowed) noexcept
    {
        auto const hostIdKey = winrt::to_hstring(hostId);

        if (!HasHostEntry(hostIdKey))
        {
            // the decision was still applied live; there is simply no entry to persist it in
            m_lastError = resources::GetString(L"ConfigFileHostEntryMissingError");
            return false;
        }

        try
        {
            // The saved lists are replaced whole rather than added to, so the config carries every
            // client the host knows about and not only the one being decided.
            midi2net::MidiNetworkHostKnownClientsConfig config{ hostId };

            // a client belongs to exactly one of the two lists, so the existing entry for it is
            // dropped and replaced by one carrying the decision just made
            for (auto const& known : GetKnownClients(hostIdKey))
            {
                if (IsSameIdentity(known, umpEndpointName, productInstanceId))
                {
                    continue;
                }

                config.KnownClients().Append(
                    midi2net::MidiNetworkKnownRemoteClient{ known.UmpEndpointName, known.ProductInstanceId, known.Allowed });
            }

            config.KnownClients().Append(
                midi2net::MidiNetworkKnownRemoteClient{ umpEndpointName, productInstanceId, allowed });

            return SaveConfig(config);
        }
        catch (...)
        {
            m_lastError = resources::FormatString(L"ConfigFileWriteError", m_path);
            return false;
        }
    }

    _Use_decl_annotations_
    bool NetworkConfigFile::ForgetRemoteClient(
        winrt::guid const& hostId,
        winrt::hstring const& umpEndpointName,
        winrt::hstring const& productInstanceId) noexcept
    {
        auto const hostIdKey = winrt::to_hstring(hostId);

        if (!HasHostEntry(hostIdKey))
        {
            return true;
        }

        try
        {
            midi2net::MidiNetworkHostKnownClientsConfig config{ hostId };

            bool removed{ false };

            // forgetting is leaving the client out of the saved set, which puts it back to being
            // one the host has never been told about
            for (auto const& known : GetKnownClients(hostIdKey))
            {
                if (IsSameIdentity(known, umpEndpointName, productInstanceId))
                {
                    removed = true;
                    continue;
                }

                config.KnownClients().Append(
                    midi2net::MidiNetworkKnownRemoteClient{ known.UmpEndpointName, known.ProductInstanceId, known.Allowed });
            }

            if (!removed)
            {
                return true;
            }

            return SaveConfig(config);
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
