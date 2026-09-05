// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiConfigFile.h"

#include "MidiReporting.h"

namespace winrt::Windows::Devices::Midi2::ServiceConfig::implementation
{
    namespace
    {
        // An entry in one of these arrays is identified by a single named value rather than by its
        // position, so a caller can supply just the entry it changed instead of the whole array.
        // Everything else, including the network allowed and denied client lists, is a whole-list
        // value and is replaced outright.
        struct KeyedArrayRule
        {
            std::wstring_view ArrayKey;
            std::wstring_view IdentityKey;
        };

        constexpr KeyedArrayRule KeyedArrayRules[]
        {
            { MIDI_CONFIG_JSON_ENDPOINT_COMMON_UPDATE_KEY, L"match" },
            { L"devices", L"deviceId" },
        };

        constexpr size_t IndentSpaces = 4;

        // a config file far larger than this is not something we wrote
        constexpr uint64_t MaximumConfigFileBytes = 0x400000;

        constexpr uint32_t SharingRetryCount = 20;
        constexpr uint32_t SharingRetryWaitMilliseconds = 100;

        std::wstring_view IdentityKeyForArray(_In_ std::wstring_view const arrayKey) noexcept
        {
            for (auto const& rule : KeyedArrayRules)
            {
                if (rule.ArrayKey == arrayKey)
                {
                    return rule.IdentityKey;
                }
            }

            return {};
        }

        void AppendIndent(_Inout_ std::wstring& text, _In_ int32_t const depth) noexcept
        {
            text.append(static_cast<size_t>(depth) * IndentSpaces, L' ');
        }

        // Windows.Data.Json only stringifies to a single line. The configuration file is meant to
        // be readable and hand-editable, so it is written back out indented.
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

                    auto const appendMember =
                        [&text, &index, &object, depth](
                            winrt::hstring const& key,
                            json::IJsonValue const& value)
                        {
                            AppendIndent(text, depth + 1);

                            text += json::JsonValue::CreateStringValue(key).Stringify();
                            text += L": ";

                            AppendPretty(text, value, depth + 1);

                            if (++index < object.Size())
                            {
                                text += L",";
                            }

                            text += L"\n";
                        };

                    // The comment says what the object is, so it leads regardless of where the
                    // map happened to put it. Json objects have no inherent order.
                    auto const commentKey = winrt::hstring{ MIDI_CONFIG_JSON_COMMON_COMMENT_KEY };
                    auto const hasComment = object.HasKey(commentKey);

                    if (hasComment)
                    {
                        appendMember(commentKey, object.GetNamedValue(commentKey));
                    }

                    for (auto const& pair : object)
                    {
                        if (hasComment && pair.Key() == commentKey)
                        {
                            continue;
                        }

                        appendMember(pair.Key(), pair.Value());
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

        void StripUtf8ByteOrderMark(_Inout_ std::string& contents) noexcept
        {
            if (contents.size() >= 3 &&
                static_cast<unsigned char>(contents[0]) == 0xEF &&
                static_cast<unsigned char>(contents[1]) == 0xBB &&
                static_cast<unsigned char>(contents[2]) == 0xBF)
            {
                contents.erase(0, 3);
            }
        }

        json::JsonObject EnsureObject(
            _In_ json::JsonObject const& parent,
            _In_ winrt::hstring const& key) noexcept
        {
            try
            {
                if (parent.HasKey(key))
                {
                    auto const existing = parent.GetNamedValue(key);

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

        // Deep merge, so changing one entry cannot discard another transport's settings or
        // anything the customer hand-edited into the file. Arrays are replaced unless the key
        // names one of the keyed arrays above.
        void MergeInto(
            _In_ json::JsonObject const& target,
            _In_ json::JsonObject const& source) noexcept;

        // Merges the incoming entry into the one with a matching identity, or appends it when
        // there is none. Merging rather than replacing means a caller can change one property of
        // an entry without having to resend the properties it did not touch.
        json::JsonArray UpsertKeyedArray(
            _In_ json::JsonArray const& existing,
            _In_ json::JsonArray const& incoming,
            _In_ std::wstring_view const identityKey) noexcept
        {
            try
            {
                auto const identity = [identityKey](json::IJsonValue const& value) -> std::wstring
                    {
                        if (value == nullptr || value.ValueType() != json::JsonValueType::Object)
                        {
                            return {};
                        }

                        auto const object = value.GetObject();
                        auto const key = winrt::hstring{ identityKey };

                        if (!object.HasKey(key))
                        {
                            return {};
                        }

                        return std::wstring{ object.GetNamedValue(key).Stringify() };
                    };

                json::JsonArray result{};

                std::vector<bool> consumed(incoming.Size(), false);

                // existing order is preserved, so a merge does not shuffle the file around
                for (uint32_t i = 0; i < existing.Size(); i++)
                {
                    auto const existingValue = existing.GetAt(i);
                    auto const existingIdentity = identity(existingValue);

                    bool merged{ false };

                    if (!existingIdentity.empty() && existingValue.ValueType() == json::JsonValueType::Object)
                    {
                        for (uint32_t j = 0; j < incoming.Size(); j++)
                        {
                            if (consumed[j] || identity(incoming.GetAt(j)) != existingIdentity)
                            {
                                continue;
                            }

                            auto const target = existingValue.GetObject();

                            MergeInto(target, incoming.GetAt(j).GetObject());

                            result.Append(target);

                            consumed[j] = true;
                            merged = true;
                            break;
                        }
                    }

                    if (!merged)
                    {
                        result.Append(existingValue);
                    }
                }

                for (uint32_t j = 0; j < incoming.Size(); j++)
                {
                    if (!consumed[j])
                    {
                        result.Append(incoming.GetAt(j));
                    }
                }

                return result;
            }
            catch (...)
            {
                return incoming;
            }
        }

        void MergeInto(
            _In_ json::JsonObject const& target,
            _In_ json::JsonObject const& source) noexcept
        {
            try
            {
                for (auto const& pair : source)
                {
                    auto const key = pair.Key();
                    auto const sourceValue = pair.Value();

                    if (sourceValue == nullptr)
                    {
                        continue;
                    }

                    if (sourceValue.ValueType() == json::JsonValueType::Object && target.HasKey(key))
                    {
                        auto const existing = target.GetNamedValue(key);

                        if (existing != nullptr && existing.ValueType() == json::JsonValueType::Object)
                        {
                            MergeInto(existing.GetObject(), sourceValue.GetObject());
                            continue;
                        }
                    }

                    if (sourceValue.ValueType() == json::JsonValueType::Array)
                    {
                        auto const identityKey = IdentityKeyForArray(std::wstring_view{ key });

                        if (!identityKey.empty() && target.HasKey(key))
                        {
                            auto const existing = target.GetNamedValue(key);

                            if (existing != nullptr && existing.ValueType() == json::JsonValueType::Array)
                            {
                                target.SetNamedValue(
                                    key,
                                    UpsertKeyedArray(existing.GetArray(), sourceValue.GetArray(), identityKey));

                                continue;
                            }
                        }
                    }

                    target.SetNamedValue(key, sourceValue);
                }
            }
            catch (...)
            {
            }
        }

        // A removal names what to delete rather than what to store. Loopback sends an array of
        // entry keys, network sends an object mirroring the shape of "create" whose leaves name
        // the entries. Both delete from the persisted "create", and neither is ever written to
        // the file, because a removal left in the file would run again on every service start.
        void ApplyRemoval(
            _In_ json::JsonObject const& createSection,
            _In_ json::IJsonValue const& removeValue) noexcept
        {
            try
            {
                if (createSection == nullptr || removeValue == nullptr)
                {
                    return;
                }

                if (removeValue.ValueType() == json::JsonValueType::Array)
                {
                    auto const entries = removeValue.GetArray();

                    for (uint32_t i = 0; i < entries.Size(); i++)
                    {
                        auto const entry = entries.GetAt(i);

                        if (entry == nullptr || entry.ValueType() != json::JsonValueType::String)
                        {
                            continue;
                        }

                        auto const wanted = internal::ToUpperTrimmedWStringCopy(std::wstring{ entry.GetString() });

                        // the key casing in the file is whatever wrote it, so match without case
                        for (auto const& pair : createSection)
                        {
                            if (internal::ToUpperTrimmedWStringCopy(std::wstring{ pair.Key() }) == wanted)
                            {
                                createSection.Remove(pair.Key());
                                break;
                            }
                        }
                    }

                    return;
                }

                if (removeValue.ValueType() == json::JsonValueType::Object)
                {
                    auto const entries = removeValue.GetObject();

                    for (auto const& pair : entries)
                    {
                        auto const key = pair.Key();
                        auto const value = pair.Value();

                        // an empty object is a leaf: it names an entry to delete
                        bool const isLeaf =
                            value == nullptr ||
                            value.ValueType() != json::JsonValueType::Object ||
                            value.GetObject().Size() == 0;

                        std::wstring const wanted{ internal::ToUpperTrimmedWStringCopy(std::wstring{ key }) };

                        winrt::hstring matchedKey{};

                        for (auto const& existing : createSection)
                        {
                            if (internal::ToUpperTrimmedWStringCopy(std::wstring{ existing.Key() }) == wanted)
                            {
                                matchedKey = existing.Key();
                                break;
                            }
                        }

                        if (matchedKey.empty())
                        {
                            continue;
                        }

                        if (isLeaf)
                        {
                            createSection.Remove(matchedKey);
                            continue;
                        }

                        auto const child = createSection.GetNamedValue(matchedKey);

                        if (child != nullptr && child.ValueType() == json::JsonValueType::Object)
                        {
                            ApplyRemoval(child.GetObject(), value);
                        }
                    }
                }
            }
            catch (...)
            {
            }
        }

        // Every property named in the wanted object must be present and equal in the candidate.
        // A subset rather than an exact comparison, so a caller can name just the one property
        // which identifies the entry instead of restating the whole match block.
        bool MatchObjectContains(
            _In_ json::JsonObject const& candidate,
            _In_ json::JsonObject const& wanted) noexcept
        {
            try
            {
                if (candidate == nullptr || wanted == nullptr)
                {
                    return false;
                }

                for (auto const& pair : wanted)
                {
                    if (!candidate.HasKey(pair.Key()))
                    {
                        return false;
                    }

                    auto const candidateValue = candidate.GetNamedValue(pair.Key());
                    auto const wantedValue = pair.Value();

                    if (candidateValue == nullptr || wantedValue == nullptr)
                    {
                        return false;
                    }

                    if (candidateValue.ValueType() == json::JsonValueType::String &&
                        wantedValue.ValueType() == json::JsonValueType::String)
                    {
                        if (internal::ToUpperTrimmedWStringCopy(std::wstring{ candidateValue.GetString() }) !=
                            internal::ToUpperTrimmedWStringCopy(std::wstring{ wantedValue.GetString() }))
                        {
                            return false;
                        }

                        continue;
                    }

                    if (candidateValue.Stringify() != wantedValue.Stringify())
                    {
                        return false;
                    }
                }

                return true;
            }
            catch (...)
            {
                return false;
            }
        }

        // Removal from an array whose entries are identified by a property rather than by
        // position. A listed string names the identity value; a listed object is matched against
        // the entry's identity object, which is how an endpoint customization is deleted.
        void ApplyKeyedArrayRemoval(
            _In_ json::JsonArray const& persistedArray,
            _In_ json::IJsonValue const& removeValue,
            _In_ std::wstring_view const identityKey) noexcept
        {
            try
            {
                if (persistedArray == nullptr || removeValue == nullptr || identityKey.empty())
                {
                    return;
                }

                if (removeValue.ValueType() != json::JsonValueType::Array)
                {
                    return;
                }

                auto const wantedEntries = removeValue.GetArray();
                winrt::hstring const identityKeyName{ identityKey };

                for (uint32_t wantedIndex = 0; wantedIndex < wantedEntries.Size(); wantedIndex++)
                {
                    auto const wanted = wantedEntries.GetAt(wantedIndex);

                    if (wanted == nullptr)
                    {
                        continue;
                    }

                    // walked backwards because entries are removed while iterating
                    for (int32_t index = static_cast<int32_t>(persistedArray.Size()) - 1; index >= 0; index--)
                    {
                        auto const existing = persistedArray.GetAt(static_cast<uint32_t>(index));

                        if (existing == nullptr || existing.ValueType() != json::JsonValueType::Object)
                        {
                            continue;
                        }

                        auto const existingObject = existing.GetObject();

                        if (!existingObject.HasKey(identityKeyName))
                        {
                            continue;
                        }

                        auto const existingIdentity = existingObject.GetNamedValue(identityKeyName);

                        if (existingIdentity == nullptr)
                        {
                            continue;
                        }

                        bool matched{ false };

                        if (wanted.ValueType() == json::JsonValueType::String &&
                            existingIdentity.ValueType() == json::JsonValueType::String)
                        {
                            matched =
                                internal::ToUpperTrimmedWStringCopy(std::wstring{ existingIdentity.GetString() }) ==
                                internal::ToUpperTrimmedWStringCopy(std::wstring{ wanted.GetString() });
                        }
                        else if (wanted.ValueType() == json::JsonValueType::Object &&
                                 existingIdentity.ValueType() == json::JsonValueType::Object)
                        {
                            auto wantedObject = wanted.GetObject();

                            // accepts either the identity object itself, or an entry which wraps it
                            if (wantedObject.HasKey(identityKeyName))
                            {
                                auto const nested = wantedObject.GetNamedValue(identityKeyName);

                                if (nested != nullptr && nested.ValueType() == json::JsonValueType::Object)
                                {
                                    wantedObject = nested.GetObject();
                                }
                            }

                            matched = MatchObjectContains(existingIdentity.GetObject(), wantedObject);
                        }

                        if (matched)
                        {
                            persistedArray.RemoveAt(static_cast<uint32_t>(index));
                        }
                    }
                }
            }
            catch (...)
            {
            }
        }

        void MergeTransportSection(
            _In_ json::JsonObject const& persistedSection,
            _In_ json::JsonObject const& incomingSection) noexcept
        {
            try
            {
                for (auto const& pair : incomingSection)
                {
                    auto const key = pair.Key();

                    if (key == MIDI_CONFIG_JSON_ENDPOINT_COMMON_REMOVE_KEY)
                    {
                        auto const removeValue = pair.Value();

                        // A keyed array lives directly in the transport section rather than under
                        // create, so those keys are handled before falling back to create.
                        bool handledAsKeyedArray{ false };

                        if (removeValue != nullptr && removeValue.ValueType() == json::JsonValueType::Object)
                        {
                            for (auto const& removePair : removeValue.GetObject())
                            {
                                auto const identityKey = IdentityKeyForArray(std::wstring_view{ removePair.Key() });

                                if (identityKey.empty() || !persistedSection.HasKey(removePair.Key()))
                                {
                                    continue;
                                }

                                auto const persistedValue = persistedSection.GetNamedValue(removePair.Key());

                                if (persistedValue != nullptr && persistedValue.ValueType() == json::JsonValueType::Array)
                                {
                                    ApplyKeyedArrayRemoval(persistedValue.GetArray(), removePair.Value(), identityKey);
                                    handledAsKeyedArray = true;
                                }
                            }
                        }

                        if (!handledAsKeyedArray && persistedSection.HasKey(MIDI_CONFIG_JSON_ENDPOINT_COMMON_CREATE_KEY))
                        {
                            auto const createValue = persistedSection.GetNamedValue(MIDI_CONFIG_JSON_ENDPOINT_COMMON_CREATE_KEY);

                            if (createValue != nullptr && createValue.ValueType() == json::JsonValueType::Object)
                            {
                                ApplyRemoval(createValue.GetObject(), removeValue);
                            }
                        }

                        continue;
                    }

                    json::JsonObject single{};
                    single.SetNamedValue(key, pair.Value());

                    MergeInto(persistedSection, single);
                }
            }
            catch (...)
            {
            }
        }

        // The transport reports its own name, which is the only source that cannot drift from what
        // the customer sees elsewhere. This is cosmetic, so not reaching the service simply means
        // no comment is written.
        std::wstring TransportDisplayName(_In_ winrt::guid const& transportId) noexcept
        {
            try
            {
                for (auto const& transport : rpt::MidiReporting::GetInstalledTransportPlugins())
                {
                    if (transport.TransportId() == transportId)
                    {
                        return std::wstring{ transport.Name() };
                    }
                }
            }
            catch (...)
            {
            }

            return {};
        }

        std::wstring TodayStamp() noexcept
        {
            SYSTEMTIME now{};
            ::GetLocalTime(&now);

            wchar_t buffer[16]{};
            swprintf_s(buffer, L"%04u-%02u-%02u", now.wYear, now.wMonth, now.wDay);

            return std::wstring{ buffer };
        }

        // One backup per day, taken from the contents as they were before this change, so a bad
        // editing session can be undone back to the state the file started the day in. Old
        // backups are never removed: the folder permissions do not necessarily allow deleting
        // files, and losing a backup is worse than keeping one.
        std::wstring TryWriteDailyBackup(
            _In_ std::wstring const& configPath,
            _In_ std::string const& originalBytes) noexcept
        {
            if (originalBytes.empty())
            {
                return {};
            }

            auto const backupPath = configPath + L"." + TodayStamp() + L".bak";

            // CREATE_NEW leaves today's existing backup alone, so the first save of the day wins
            wil::unique_hfile file{ ::CreateFileW(
                backupPath.c_str(),
                GENERIC_WRITE,
                FILE_SHARE_READ,
                nullptr,
                CREATE_NEW,
                FILE_ATTRIBUTE_NORMAL,
                nullptr) };

            if (!file)
            {
                return ::GetLastError() == ERROR_FILE_EXISTS ? backupPath : std::wstring{};
            }

            DWORD written{ 0 };

            if (!::WriteFile(file.get(), originalBytes.data(), static_cast<DWORD>(originalBytes.size()), &written, nullptr) ||
                written != originalBytes.size())
            {
                return {};
            }

            return backupPath;
        }

        bool ReadWholeFile(_In_ HANDLE file, _Out_ std::string& contents) noexcept
        {
            contents.clear();

            LARGE_INTEGER size{};

            if (!::GetFileSizeEx(file, &size))
            {
                return false;
            }

            if (size.QuadPart == 0)
            {
                return true;
            }

            if (size.QuadPart < 0 || static_cast<uint64_t>(size.QuadPart) > MaximumConfigFileBytes)
            {
                return false;
            }

            LARGE_INTEGER zero{};

            if (!::SetFilePointerEx(file, zero, nullptr, FILE_BEGIN))
            {
                return false;
            }

            contents.resize(static_cast<size_t>(size.QuadPart));

            DWORD bytesRead{ 0 };

            if (!::ReadFile(file, contents.data(), static_cast<DWORD>(contents.size()), &bytesRead, nullptr))
            {
                contents.clear();
                return false;
            }

            contents.resize(bytesRead);
            StripUtf8ByteOrderMark(contents);

            return true;
        }

        bool WriteWholeFile(_In_ HANDLE file, _In_ std::string const& contents) noexcept
        {
            LARGE_INTEGER zero{};

            if (!::SetFilePointerEx(file, zero, nullptr, FILE_BEGIN))
            {
                return false;
            }

            DWORD written{ 0 };

            if (!::WriteFile(file, contents.data(), static_cast<DWORD>(contents.size()), &written, nullptr) ||
                written != contents.size())
            {
                return false;
            }

            // the new contents can be shorter than the old, and the tail would otherwise remain
            return ::SetEndOfFile(file) != FALSE;
        }

        std::mutex g_pathOverrideLock{};
        std::wstring g_pathOverride{};
    }


#ifdef _DEBUG
    _Use_decl_annotations_
    void MidiConfigFile::SetPathOverride(std::wstring const& path) noexcept
    {
        auto lock = std::scoped_lock{ g_pathOverrideLock };

        g_pathOverride = path;
    }

    std::wstring MidiConfigFile::GetPathOverride() noexcept
    {
        auto lock = std::scoped_lock{ g_pathOverrideLock };

        return g_pathOverride;
    }
#endif


    std::wstring MidiConfigFile::ResolvePath() noexcept
    {
        try
        {
#ifdef _DEBUG
            if (auto const overridePath = GetPathOverride(); !overridePath.empty())
            {
                return overridePath;
            }
#endif

            // the service only ever opens a file inside this folder, so the registry holds a bare
            // file name. Anything with a path in it is a tampered value and is refused.
            std::wstring fileName{};

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
                    fileName = internal::TrimmedWStringCopy(std::wstring{ buffer });
                }
            }

            if (fileName.empty() ||
                fileName.find(L'\\') != std::wstring::npos ||
                fileName.find(L'/') != std::wstring::npos ||
                fileName.find(L':') != std::wstring::npos ||
                fileName.find(L'%') != std::wstring::npos)
            {
                return {};
            }

            wchar_t folder[MAX_PATH]{};

            auto const expanded = ::ExpandEnvironmentStringsW(MIDI_CONFIG_FILE_FOLDER, folder, ARRAYSIZE(folder));

            if (expanded == 0 || expanded > ARRAYSIZE(folder))
            {
                return {};
            }

            return std::wstring{ folder } + fileName;
        }
        catch (...)
        {
            return {};
        }
    }


    MidiConfigFileSaveOutcome MidiConfigFile::EnsureExists() noexcept
    {
        MidiConfigFileSaveOutcome outcome{};

        try
        {
            if (auto const existing = ResolvePath(); !existing.empty())
            {
                outcome.Result = svc::MidiServiceConfigSaveResult::Success;
                outcome.ConfigFilePath = winrt::hstring{ existing };

                return outcome;
            }

            // Writing here needs no elevation on a normally installed machine: the setup grants
            // Authenticated Users SetValue on this key. Creating the key itself does need it, so
            // a machine which has never had any Windows MIDI Services component installed is
            // reported as a failure rather than silently doing nothing.
            wil::unique_hkey key{};

            auto const openResult = ::RegCreateKeyExW(
                HKEY_LOCAL_MACHINE,
                MIDI_ROOT_REG_KEY,
                0,
                nullptr,
                0,
                KEY_SET_VALUE,
                nullptr,
                key.put(),
                nullptr);

            if (openResult != ERROR_SUCCESS)
            {
                outcome.Result = openResult == ERROR_ACCESS_DENIED ?
                    svc::MidiServiceConfigSaveResult::ErrorAccessDenied :
                    svc::MidiServiceConfigSaveResult::ErrorNoConfigFileRegistered;

                return outcome;
            }

            std::wstring const fileName{ MIDI_CONFIG_FILE_DEFAULT_NAME };

            auto const setResult = ::RegSetValueExW(
                key.get(),
                MIDI_CONFIG_FILE_REG_VALUE,
                0,
                REG_SZ,
                reinterpret_cast<BYTE const*>(fileName.c_str()),
                static_cast<DWORD>((fileName.size() + 1) * sizeof(wchar_t)));

            if (setResult != ERROR_SUCCESS)
            {
                outcome.Result = setResult == ERROR_ACCESS_DENIED ?
                    svc::MidiServiceConfigSaveResult::ErrorAccessDenied :
                    svc::MidiServiceConfigSaveResult::ErrorNoConfigFileRegistered;

                return outcome;
            }

            auto const path = ResolvePath();

            if (path.empty())
            {
                outcome.Result = svc::MidiServiceConfigSaveResult::ErrorNoConfigFileRegistered;
                return outcome;
            }

            outcome.ConfigFilePath = winrt::hstring{ path };

            // An empty file parses as nothing, so the skeleton is written to give the service and
            // anyone opening it in an editor something valid to read.
            wil::unique_hfile file{ ::CreateFileW(
                path.c_str(),
                GENERIC_READ | GENERIC_WRITE,
                FILE_SHARE_READ,
                nullptr,
                OPEN_ALWAYS,
                FILE_ATTRIBUTE_NORMAL,
                nullptr) };

            if (!file)
            {
                outcome.Result = ::GetLastError() == ERROR_ACCESS_DENIED ?
                    svc::MidiServiceConfigSaveResult::ErrorAccessDenied :
                    svc::MidiServiceConfigSaveResult::ErrorWritingConfigFile;

                return outcome;
            }

            std::string existingBytes{};

            if (ReadWholeFile(file.get(), existingBytes) && !existingBytes.empty())
            {
                // something is already there, so it is left exactly as it is
                outcome.Result = svc::MidiServiceConfigSaveResult::Success;
                return outcome;
            }

            json::JsonObject skeleton{};

            skeleton.SetNamedValue(
                MIDI_CONFIG_JSON_COMMON_COMMENT_KEY,
                json::JsonValue::CreateStringValue(L"Windows MIDI Services configuration"));

            skeleton.SetNamedValue(MIDI_CONFIG_JSON_TRANSPORT_PLUGIN_SETTINGS_OBJECT, json::JsonObject{});

            std::wstring text{};
            AppendPretty(text, skeleton, 0);
            text += L"\n";

            outcome.Result = WriteWholeFile(file.get(), ToUtf8(text)) ?
                svc::MidiServiceConfigSaveResult::Success :
                svc::MidiServiceConfigSaveResult::ErrorWritingConfigFile;

            return outcome;
        }
        catch (...)
        {
            outcome.Result = svc::MidiServiceConfigSaveResult::ErrorUnexpected;
            return outcome;
        }
    }


    _Use_decl_annotations_
    MidiConfigFileSaveOutcome MidiConfigFile::SaveTransportSection(
        winrt::guid const& transportId,
        json::JsonObject const& transportSection) noexcept
    {
        MidiConfigFileSaveOutcome outcome{};

        try
        {
            if (transportSection == nullptr)
            {
                outcome.Result = svc::MidiServiceConfigSaveResult::ErrorConfigJsonNullOrEmpty;
                return outcome;
            }

            auto const path = ResolvePath();

            if (path.empty())
            {
                outcome.Result = svc::MidiServiceConfigSaveResult::ErrorNoConfigFileRegistered;
                return outcome;
            }

            outcome.ConfigFilePath = winrt::hstring{ path };

            // Resolved before the file is opened. This asks the service for the name, and the
            // service reads this same file, so the two must not be made to wait on each other.
            auto const transportName = TransportDisplayName(transportId);

            // This handle is the write lock. Sharing read means the service is never blocked from
            // reading, and because the handle is owned by the process, a crash releases it rather
            // than leaving a stale lock behind.
            wil::unique_hfile file{};

            for (uint32_t attempt = 0; attempt < SharingRetryCount; attempt++)
            {
                file.reset(::CreateFileW(
                    path.c_str(),
                    GENERIC_READ | GENERIC_WRITE,
                    FILE_SHARE_READ,
                    nullptr,
                    OPEN_ALWAYS,
                    FILE_ATTRIBUTE_NORMAL,
                    nullptr));

                if (file)
                {
                    break;
                }

                auto const error = ::GetLastError();

                if (error == ERROR_ACCESS_DENIED)
                {
                    outcome.Result = svc::MidiServiceConfigSaveResult::ErrorAccessDenied;
                    return outcome;
                }

                if (error != ERROR_SHARING_VIOLATION && error != ERROR_LOCK_VIOLATION)
                {
                    outcome.Result = svc::MidiServiceConfigSaveResult::ErrorWritingConfigFile;
                    return outcome;
                }

                ::Sleep(SharingRetryWaitMilliseconds);
            }

            if (!file)
            {
                outcome.Result = svc::MidiServiceConfigSaveResult::ErrorConfigFileBusy;
                return outcome;
            }

            std::string originalBytes{};

            if (!ReadWholeFile(file.get(), originalBytes))
            {
                outcome.Result = svc::MidiServiceConfigSaveResult::ErrorConfigFileNotValidJson;
                return outcome;
            }

            json::JsonObject config{ nullptr };

            if (originalBytes.empty())
            {
                config = json::JsonObject{};
            }
            else if (!json::JsonObject::TryParse(winrt::hstring{ FromUtf8(originalBytes) }, config) || config == nullptr)
            {
                // Overwriting would discard whatever the customer has in there, and they may be
                // able to repair it, so the file is left exactly as it is.
                outcome.Result = svc::MidiServiceConfigSaveResult::ErrorConfigFileNotValidJson;
                return outcome;
            }

            auto const pluginSettings = EnsureObject(config, MIDI_CONFIG_JSON_TRANSPORT_PLUGIN_SETTINGS_OBJECT);

            if (pluginSettings == nullptr)
            {
                outcome.Result = svc::MidiServiceConfigSaveResult::ErrorProcessingConfigJson;
                return outcome;
            }

            auto const transportKey = winrt::hstring{ internal::GuidToString(transportId) };

            // the file may already hold this transport's section under different key casing
            winrt::hstring existingKey{};

            for (auto const& pair : pluginSettings)
            {
                if (internal::ToUpperTrimmedWStringCopy(std::wstring{ pair.Key() }) ==
                    internal::ToUpperTrimmedWStringCopy(std::wstring{ transportKey }))
                {
                    existingKey = pair.Key();
                    break;
                }
            }

            auto const persistedSection = EnsureObject(
                pluginSettings,
                existingKey.empty() ? transportKey : existingKey);

            if (persistedSection == nullptr)
            {
                outcome.Result = svc::MidiServiceConfigSaveResult::ErrorProcessingConfigJson;
                return outcome;
            }

            // Json has no comments, so this names the transport for anyone reading the file by
            // hand. It is refreshed on every save and is never read back by the service.
            if (!transportName.empty())
            {
                persistedSection.SetNamedValue(
                    MIDI_CONFIG_JSON_COMMON_COMMENT_KEY,
                    json::JsonValue::CreateStringValue(winrt::hstring{ transportName }));
            }

            MergeTransportSection(persistedSection, transportSection);

            std::wstring text{};
            AppendPretty(text, config, 0);
            text += L"\n";

            auto const newBytes = ToUtf8(text);

            if (newBytes.empty())
            {
                outcome.Result = svc::MidiServiceConfigSaveResult::ErrorProcessingConfigJson;
                return outcome;
            }

            outcome.BackupFilePath = winrt::hstring{ TryWriteDailyBackup(path, originalBytes) };

            if (!WriteWholeFile(file.get(), newBytes))
            {
                outcome.Result = ::GetLastError() == ERROR_ACCESS_DENIED ?
                    svc::MidiServiceConfigSaveResult::ErrorAccessDenied :
                    svc::MidiServiceConfigSaveResult::ErrorWritingConfigFile;

                return outcome;
            }

            // Replacing the file in one atomic step is not possible here, because the folder
            // permissions do not necessarily allow deleting the temporary file a rename would
            // need. Reading back what was written is what catches a partial write instead.
            std::string verifyBytes{};
            json::JsonObject verified{ nullptr };

            if (!ReadWholeFile(file.get(), verifyBytes) ||
                !json::JsonObject::TryParse(winrt::hstring{ FromUtf8(verifyBytes) }, verified) ||
                verified == nullptr)
            {
                if (!originalBytes.empty())
                {
                    LOG_HR_IF(E_FAIL, !WriteWholeFile(file.get(), originalBytes));
                }

                outcome.Result = svc::MidiServiceConfigSaveResult::ErrorVerificationFailed;
                return outcome;
            }

            outcome.Result = svc::MidiServiceConfigSaveResult::Success;

            return outcome;
        }
        catch (...)
        {
            outcome.Result = svc::MidiServiceConfigSaveResult::ErrorUnexpected;
            return outcome;
        }
    }
}
