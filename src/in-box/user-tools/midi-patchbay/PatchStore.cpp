// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "PatchStore.h"
#include "StringResources.h"

namespace midipatchbay
{
    namespace
    {
        constexpr wchar_t FolderName[] = L"MIDI Patchbay";

        constexpr wchar_t KeyFileVersion[] = L"fileVersion";
        constexpr wchar_t KeyName[] = L"name";
        constexpr wchar_t KeyDescription[] = L"description";
        constexpr wchar_t KeyCreated[] = L"created";
        constexpr wchar_t KeyModified[] = L"modified";
        constexpr wchar_t KeyActivateAtStartup[] = L"activateAtStartup";
        constexpr wchar_t KeyEndpoints[] = L"endpoints";
        constexpr wchar_t KeyConnections[] = L"connections";
        constexpr wchar_t KeyId[] = L"id";
        constexpr wchar_t KeyDisplayName[] = L"displayName";
        constexpr wchar_t KeyTransportCode[] = L"transportCode";
        constexpr wchar_t KeyMatch[] = L"match";
        constexpr wchar_t KeyMatchMode[] = L"matchMode";
        constexpr wchar_t KeyCanvasX[] = L"x";
        constexpr wchar_t KeyCanvasY[] = L"y";
        constexpr wchar_t KeyShowAllGroups[] = L"showAllGroups";
        constexpr wchar_t KeySourceEndpoint[] = L"sourceEndpointId";
        constexpr wchar_t KeySourceGroup[] = L"sourceGroup";
        constexpr wchar_t KeyDestinationEndpoint[] = L"destinationEndpointId";
        constexpr wchar_t KeyDestinationGroup[] = L"destinationGroup";
        constexpr wchar_t KeyMuted[] = L"muted";
        constexpr wchar_t KeyFilter[] = L"filter";
        constexpr wchar_t KeyTransform[] = L"transform";
        constexpr wchar_t KeyComment[] = L"_comment";

        constexpr wchar_t CommentText[] =
            L"Windows MIDI Patchbay. Written by the MIDI Patchbay app. The MIDI service does not "
            L"read this file.";

        constexpr int32_t FileVersion = 1;

        constexpr wchar_t MatchModeDeviceId[] = L"endpointDeviceId";
        constexpr wchar_t MatchModeUsb[] = L"usbVendorAndProduct";
        constexpr wchar_t MatchModeName[] = L"endpointName";

        // Canvas coordinates are clamped rather than rejected: a nonsense value should move a
        // node back into view, not throw the whole patch away.
        constexpr double MaximumCanvasCoordinate = 100000.0;

        std::wstring Utf8ToWide(_In_ std::string const& text) noexcept
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

        std::string WideToUtf8(_In_ std::wstring const& text) noexcept
        {
            if (text.empty())
            {
                return {};
            }

            auto const required = ::WideCharToMultiByte(
                CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);

            if (required <= 0)
            {
                return {};
            }

            std::string result(static_cast<size_t>(required), '\0');

            ::WideCharToMultiByte(
                CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                result.data(), required, nullptr, nullptr);

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

            if (!::GetFileSizeEx(file.get(), &size) || size.QuadPart < 0 ||
                static_cast<uint64_t>(size.QuadPart) > MaximumPatchFileBytes)
            {
                return false;
            }

            if (size.QuadPart == 0)
            {
                return true;
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
                FILE_SHARE_READ,
                nullptr,
                CREATE_ALWAYS,
                FILE_ATTRIBUTE_NORMAL,
                nullptr) };

            if (!file)
            {
                return false;
            }

            if (contents.empty())
            {
                return true;
            }

            DWORD written{ 0 };

            if (!::WriteFile(file.get(), contents.data(), static_cast<DWORD>(contents.size()), &written, nullptr))
            {
                return false;
            }

            return written == contents.size();
        }

        std::wstring GetNamedString(_In_ json::JsonObject const& parent, _In_ std::wstring_view key) noexcept
        {
            try
            {
                if (parent != nullptr && parent.HasKey(key))
                {
                    auto const value = parent.GetNamedValue(key);

                    if (value != nullptr && value.ValueType() == json::JsonValueType::String)
                    {
                        return SanitizeStoredString(std::wstring{ value.GetString() });
                    }
                }
            }
            catch (...)
            {
            }

            return {};
        }

        double GetNamedDouble(
            _In_ json::JsonObject const& parent,
            _In_ std::wstring_view key,
            _In_ double defaultValue) noexcept
        {
            try
            {
                if (parent != nullptr && parent.HasKey(key))
                {
                    auto const value = parent.GetNamedValue(key);

                    if (value != nullptr && value.ValueType() == json::JsonValueType::Number)
                    {
                        auto const number = value.GetNumber();

                        if (std::isfinite(number))
                        {
                            return number;
                        }
                    }
                }
            }
            catch (...)
            {
            }

            return defaultValue;
        }

        bool GetNamedBool(
            _In_ json::JsonObject const& parent,
            _In_ std::wstring_view key,
            _In_ bool defaultValue) noexcept
        {
            try
            {
                if (parent != nullptr && parent.HasKey(key))
                {
                    auto const value = parent.GetNamedValue(key);

                    if (value != nullptr && value.ValueType() == json::JsonValueType::Boolean)
                    {
                        return value.GetBoolean();
                    }
                }
            }
            catch (...)
            {
            }

            return defaultValue;
        }

        json::JsonObject GetNamedObject(_In_ json::JsonObject const& parent, _In_ std::wstring_view key) noexcept
        {
            try
            {
                if (parent != nullptr && parent.HasKey(key))
                {
                    auto const value = parent.GetNamedValue(key);

                    if (value != nullptr && value.ValueType() == json::JsonValueType::Object)
                    {
                        return value.GetObject();
                    }
                }
            }
            catch (...)
            {
            }

            return nullptr;
        }

        int32_t ReadGroupIndex(_In_ json::JsonObject const& parent, _In_ std::wstring_view key) noexcept
        {
            auto const raw = static_cast<int32_t>(GetNamedDouble(parent, key, AllGroups));

            if (raw < 0 || raw >= MaximumGroupCount)
            {
                return AllGroups;
            }

            return raw;
        }

        double ClampCoordinate(_In_ double value) noexcept
        {
            if (!std::isfinite(value))
            {
                return 0.0;
            }

            return std::clamp(value, -MaximumCanvasCoordinate, MaximumCanvasCoordinate);
        }

        EndpointMatchMode MatchModeFromString(_In_ std::wstring const& value) noexcept
        {
            if (value == MatchModeUsb)
            {
                return EndpointMatchMode::UsbVendorAndProduct;
            }

            if (value == MatchModeName)
            {
                return EndpointMatchMode::EndpointName;
            }

            return EndpointMatchMode::EndpointDeviceId;
        }

        std::wstring MatchModeToString(_In_ EndpointMatchMode mode) noexcept
        {
            switch (mode)
            {
            case EndpointMatchMode::UsbVendorAndProduct:    return MatchModeUsb;
            case EndpointMatchMode::EndpointName:           return MatchModeName;
            default:                                        return MatchModeDeviceId;
            }
        }

        // Seconds since 1970, not a FILETIME: a FILETIME needs more bits than a JSON number can
        // hold exactly, so it comes back rounded to the nearest few seconds.
        int64_t CurrentTimestamp() noexcept
        {
            FILETIME now{};
            ::GetSystemTimeAsFileTime(&now);

            ULARGE_INTEGER value{};
            value.LowPart = now.dwLowDateTime;
            value.HighPart = now.dwHighDateTime;

            constexpr uint64_t HundredNanosecondsPerSecond = 10000000ull;
            constexpr uint64_t SecondsFrom1601To1970 = 11644473600ull;

            return static_cast<int64_t>(value.QuadPart / HundredNanosecondsPerSecond) -
                static_cast<int64_t>(SecondsFrom1601To1970);
        }

        // A patch name is customer text and goes straight into a file name, so everything the
        // file system reserves is replaced rather than escaped.
        std::wstring MakeSafeFileStem(_In_ std::wstring const& name) noexcept
        {
            std::wstring result{};
            result.reserve(name.size());

            for (auto const ch : name)
            {
                if (ch < L' ' || ch == L'<' || ch == L'>' || ch == L':' || ch == L'"' ||
                    ch == L'/' || ch == L'\\' || ch == L'|' || ch == L'?' || ch == L'*')
                {
                    result.push_back(L'_');
                }
                else
                {
                    result.push_back(ch);
                }
            }

            // trailing dots and spaces are legal to type but cannot be created on disk
            while (!result.empty() && (result.back() == L'.' || result.back() == L' '))
            {
                result.pop_back();
            }

            if (result.size() > 96)
            {
                result.resize(96);
            }

            if (result.empty())
            {
                result = L"Patch";
            }

            // the reserved DOS device names are still reserved with an extension attached
            static constexpr std::wstring_view reserved[] = {
                L"CON", L"PRN", L"AUX", L"NUL",
                L"COM1", L"COM2", L"COM3", L"COM4", L"COM5", L"COM6", L"COM7", L"COM8", L"COM9",
                L"LPT1", L"LPT2", L"LPT3", L"LPT4", L"LPT5", L"LPT6", L"LPT7", L"LPT8", L"LPT9" };

            std::wstring upper{ result };
            std::transform(upper.begin(), upper.end(), upper.begin(),
                [](wchar_t c) { return static_cast<wchar_t>(::towupper(c)); });

            for (auto const& name2 : reserved)
            {
                if (upper == name2)
                {
                    result.insert(result.begin(), L'_');
                    break;
                }
            }

            return result;
        }
    }

    PatchStore::PatchStore() noexcept
    {
        try
        {
            wil::unique_cotaskmem_string documents;

            if (SUCCEEDED(::SHGetKnownFolderPath(FOLDERID_Documents, KF_FLAG_DEFAULT, nullptr, &documents)) &&
                documents)
            {
                std::filesystem::path root{ documents.get() };
                root /= FolderName;

                m_folder = root.wstring();
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to resolve the patch folder.")
    }

    PatchStore& PatchStore::Current() noexcept
    {
        static PatchStore instance{};
        return instance;
    }

    bool PatchStore::EnsureFolder() noexcept
    {
        try
        {
            if (m_folder.empty())
            {
                m_lastError = resources::GetString(L"ErrorNoPatchFolder");
                return false;
            }

            std::error_code ec{};

            if (std::filesystem::exists(m_folder, ec))
            {
                return true;
            }

            std::filesystem::create_directories(m_folder, ec);

            if (ec)
            {
                m_lastError = resources::FormatString(L"ErrorCreateFolderFormat", m_folder);
                return false;
            }

            return true;
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to create the patch folder.")

        m_lastError = resources::GetString(L"ErrorNoPatchFolder");
        return false;
    }

    void PatchStore::ShowFolder() noexcept
    {
        try
        {
            if (!EnsureFolder())
            {
                return;
            }

            SHELLEXECUTEINFOW info{};
            info.cbSize = sizeof(info);
            info.fMask = SEE_MASK_NOASYNC;
            info.lpVerb = L"open";
            info.lpFile = m_folder.c_str();
            info.nShow = SW_SHOWNORMAL;

            ::ShellExecuteExW(&info);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to open the patch folder.")
    }

    _Use_decl_annotations_
    std::wstring PatchStore::BuildUniqueFilePath(
        std::wstring const& patchName,
        std::wstring const& currentPath) const noexcept
    {
        try
        {
            if (m_folder.empty())
            {
                return {};
            }

            auto const stem = MakeSafeFileStem(patchName);

            for (int suffix = 0; suffix < 1000; suffix++)
            {
                std::filesystem::path candidate{ m_folder };

                candidate /= suffix == 0
                    ? stem + FileExtension
                    : stem + L" (" + std::to_wstring(suffix) + L")" + FileExtension;

                auto const text = candidate.wstring();

                if (!currentPath.empty() && ::CompareStringOrdinal(
                    text.c_str(), -1, currentPath.c_str(), -1, TRUE) == CSTR_EQUAL)
                {
                    return text;
                }

                std::error_code ec{};

                if (!std::filesystem::exists(candidate, ec))
                {
                    return text;
                }
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to build a patch file name.")

        return {};
    }

    _Use_decl_annotations_
    std::optional<PatchDocument> PatchStore::LoadFile(std::wstring const& path) noexcept
    {
        try
        {
            std::string bytes{};

            if (!ReadAllBytes(path, bytes) || bytes.empty())
            {
                return std::nullopt;
            }

            json::JsonObject root{ nullptr };

            if (!json::JsonObject::TryParse(winrt::hstring{ Utf8ToWide(bytes) }, root) || root == nullptr)
            {
                return std::nullopt;
            }

            PatchDocument patch{};

            patch.FilePath = path;
            patch.Name = GetNamedString(root, KeyName);
            patch.Description = GetNamedString(root, KeyDescription);
            patch.ActivateAtStartup = GetNamedBool(root, KeyActivateAtStartup, true);
            patch.CreatedTimestamp = static_cast<int64_t>(GetNamedDouble(root, KeyCreated, 0.0));
            patch.ModifiedTimestamp = static_cast<int64_t>(GetNamedDouble(root, KeyModified, 0.0));

            if (patch.Name.empty())
            {
                patch.Name = std::filesystem::path{ path }.stem().wstring();
            }

            if (root.HasKey(KeyEndpoints))
            {
                auto const value = root.GetNamedValue(KeyEndpoints);

                if (value != nullptr && value.ValueType() == json::JsonValueType::Array)
                {
                    for (auto const& entry : value.GetArray())
                    {
                        if (patch.Endpoints.size() >= MaximumEndpointsPerPatch)
                        {
                            break;
                        }

                        if (entry == nullptr || entry.ValueType() != json::JsonValueType::Object)
                        {
                            continue;
                        }

                        auto const item = entry.GetObject();

                        PatchEndpoint endpoint{};

                        endpoint.Id = GetNamedString(item, KeyId);
                        endpoint.DisplayName = GetNamedString(item, KeyDisplayName);
                        endpoint.TransportCode = GetNamedString(item, KeyTransportCode);
                        endpoint.Match = MatchFromJson(GetNamedObject(item, KeyMatch));
                        endpoint.MatchMode = MatchModeFromString(GetNamedString(item, KeyMatchMode));
                        endpoint.CanvasX = ClampCoordinate(GetNamedDouble(item, KeyCanvasX, 0.0));
                        endpoint.CanvasY = ClampCoordinate(GetNamedDouble(item, KeyCanvasY, 0.0));
                        endpoint.ShowAllGroups = GetNamedBool(item, KeyShowAllGroups, false);

                        if (endpoint.Id.empty())
                        {
                            continue;
                        }

                        if (patch.FindEndpoint(endpoint.Id) != nullptr)
                        {
                            continue;
                        }

                        patch.Endpoints.push_back(std::move(endpoint));
                    }
                }
            }

            if (root.HasKey(KeyConnections))
            {
                auto const value = root.GetNamedValue(KeyConnections);

                if (value != nullptr && value.ValueType() == json::JsonValueType::Array)
                {
                    for (auto const& entry : value.GetArray())
                    {
                        if (patch.Connections.size() >= MaximumConnectionsPerPatch)
                        {
                            break;
                        }

                        if (entry == nullptr || entry.ValueType() != json::JsonValueType::Object)
                        {
                            continue;
                        }

                        auto const item = entry.GetObject();

                        PatchConnection connection{};

                        connection.Id = GetNamedString(item, KeyId);
                        connection.SourceEndpointId = GetNamedString(item, KeySourceEndpoint);
                        connection.SourceGroupIndex = ReadGroupIndex(item, KeySourceGroup);
                        connection.DestinationEndpointId = GetNamedString(item, KeyDestinationEndpoint);
                        connection.DestinationGroupIndex = ReadGroupIndex(item, KeyDestinationGroup);
                        connection.Muted = GetNamedBool(item, KeyMuted, false);
                        connection.Filter = FilterFromJson(GetNamedObject(item, KeyFilter));
                        connection.Transform = TransformFromJson(GetNamedObject(item, KeyTransform));

                        if (connection.Id.empty())
                        {
                            connection.Id = PatchDocument::NewId();
                        }

                        // a connection that names an endpoint the file does not contain would
                        // draw from nowhere, so it is dropped rather than half rendered
                        if (patch.FindEndpoint(connection.SourceEndpointId) == nullptr ||
                            patch.FindEndpoint(connection.DestinationEndpointId) == nullptr)
                        {
                            continue;
                        }

                        if (patch.HasConnection(
                            connection.SourceEndpointId, connection.SourceGroupIndex,
                            connection.DestinationEndpointId, connection.DestinationGroupIndex))
                        {
                            continue;
                        }

                        patch.Connections.push_back(std::move(connection));
                    }
                }
            }

            return patch;
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to read a patch file.")

        return std::nullopt;
    }

    _Use_decl_annotations_
    bool PatchStore::LoadAll(std::vector<PatchDocument>& patches) noexcept
    {
        patches.clear();

        try
        {
            if (m_folder.empty())
            {
                m_lastError = resources::GetString(L"ErrorNoPatchFolder");
                return false;
            }

            std::error_code ec{};

            if (!std::filesystem::exists(m_folder, ec))
            {
                return true;
            }

            for (auto const& entry : std::filesystem::directory_iterator{ m_folder, ec })
            {
                if (ec)
                {
                    break;
                }

                if (patches.size() >= MaximumPatchCount)
                {
                    break;
                }

                if (!entry.is_regular_file(ec))
                {
                    continue;
                }

                auto const name = entry.path().filename().wstring();

                if (name.size() <= ARRAYSIZE(FileExtension) - 1)
                {
                    continue;
                }

                auto const tail = name.substr(name.size() - (ARRAYSIZE(FileExtension) - 1));

                if (::CompareStringOrdinal(tail.c_str(), -1, FileExtension, -1, TRUE) != CSTR_EQUAL)
                {
                    continue;
                }

                auto loaded = LoadFile(entry.path().wstring());

                if (loaded.has_value())
                {
                    patches.push_back(std::move(loaded.value()));
                }
            }

            return true;
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to enumerate the patch folder.")

        m_lastError = resources::GetString(L"ErrorReadPatches");
        return false;
    }

    _Use_decl_annotations_
    bool PatchStore::Save(PatchDocument& patch) noexcept
    {
        try
        {
            if (!EnsureFolder())
            {
                return false;
            }

            auto const targetPath = BuildUniqueFilePath(patch.Name, patch.FilePath);

            if (targetPath.empty())
            {
                m_lastError = resources::GetString(L"ErrorNoPatchFolder");
                return false;
            }

            if (patch.CreatedTimestamp == 0)
            {
                patch.CreatedTimestamp = CurrentTimestamp();
            }

            patch.ModifiedTimestamp = CurrentTimestamp();

            json::JsonObject root{};

            root.SetNamedValue(KeyComment, json::JsonValue::CreateStringValue(CommentText));
            root.SetNamedValue(KeyFileVersion, json::JsonValue::CreateNumberValue(FileVersion));
            root.SetNamedValue(KeyName, json::JsonValue::CreateStringValue(patch.Name));
            root.SetNamedValue(KeyDescription, json::JsonValue::CreateStringValue(patch.Description));
            root.SetNamedValue(KeyCreated, json::JsonValue::CreateNumberValue(static_cast<double>(patch.CreatedTimestamp)));
            root.SetNamedValue(KeyModified, json::JsonValue::CreateNumberValue(static_cast<double>(patch.ModifiedTimestamp)));
            root.SetNamedValue(KeyActivateAtStartup, json::JsonValue::CreateBooleanValue(patch.ActivateAtStartup));

            json::JsonArray endpoints{};

            for (auto const& endpoint : patch.Endpoints)
            {
                json::JsonObject item{};

                item.SetNamedValue(KeyId, json::JsonValue::CreateStringValue(endpoint.Id));
                item.SetNamedValue(KeyDisplayName, json::JsonValue::CreateStringValue(endpoint.DisplayName));
                item.SetNamedValue(KeyTransportCode, json::JsonValue::CreateStringValue(endpoint.TransportCode));
                item.SetNamedValue(KeyMatch, MatchToJson(endpoint.Match));
                item.SetNamedValue(KeyMatchMode, json::JsonValue::CreateStringValue(MatchModeToString(endpoint.MatchMode)));
                item.SetNamedValue(KeyCanvasX, json::JsonValue::CreateNumberValue(endpoint.CanvasX));
                item.SetNamedValue(KeyCanvasY, json::JsonValue::CreateNumberValue(endpoint.CanvasY));
                item.SetNamedValue(KeyShowAllGroups, json::JsonValue::CreateBooleanValue(endpoint.ShowAllGroups));

                endpoints.Append(item);
            }

            root.SetNamedValue(KeyEndpoints, endpoints);

            json::JsonArray connections{};

            for (auto const& connection : patch.Connections)
            {
                json::JsonObject item{};

                item.SetNamedValue(KeyId, json::JsonValue::CreateStringValue(connection.Id));
                item.SetNamedValue(KeySourceEndpoint, json::JsonValue::CreateStringValue(connection.SourceEndpointId));
                item.SetNamedValue(KeySourceGroup, json::JsonValue::CreateNumberValue(connection.SourceGroupIndex));
                item.SetNamedValue(KeyDestinationEndpoint, json::JsonValue::CreateStringValue(connection.DestinationEndpointId));
                item.SetNamedValue(KeyDestinationGroup, json::JsonValue::CreateNumberValue(connection.DestinationGroupIndex));
                item.SetNamedValue(KeyMuted, json::JsonValue::CreateBooleanValue(connection.Muted));

                // Only when it does something, so an untouched patch file stays readable.
                if (!connection.Filter.PassesEverything())
                {
                    item.SetNamedValue(KeyFilter, FilterToJson(connection.Filter));
                }

                if (!connection.Transform.ChangesNothing())
                {
                    item.SetNamedValue(KeyTransform, TransformToJson(connection.Transform));
                }

                connections.Append(item);
            }

            root.SetNamedValue(KeyConnections, connections);

            if (!WriteAllBytes(targetPath, WideToUtf8(std::wstring{ root.Stringify() })))
            {
                m_lastError = resources::FormatString(L"ErrorSavePatchFormat", targetPath);
                return false;
            }

            // a rename leaves the previous file behind otherwise
            if (!patch.FilePath.empty() && ::CompareStringOrdinal(
                patch.FilePath.c_str(), -1, targetPath.c_str(), -1, TRUE) != CSTR_EQUAL)
            {
                std::error_code ec{};
                std::filesystem::remove(patch.FilePath, ec);
            }

            patch.FilePath = targetPath;
            patch.IsTemporary = false;

            return true;
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to save the patch.")

        m_lastError = resources::GetString(L"ErrorSavePatch");
        return false;
    }

    _Use_decl_annotations_
    bool PatchStore::Delete(PatchDocument const& patch) noexcept
    {
        try
        {
            if (patch.FilePath.empty())
            {
                return true;
            }

            std::error_code ec{};
            std::filesystem::remove(patch.FilePath, ec);

            if (ec)
            {
                m_lastError = resources::FormatString(L"ErrorDeletePatchFormat", patch.FilePath);
                return false;
            }

            return true;
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to delete the patch.")

        m_lastError = resources::GetString(L"ErrorDeletePatch");
        return false;
    }
}
