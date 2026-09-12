// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "ClockStore.h"
#include "StringResources.h"

#include "MidiDefs.h"

namespace res = ::midiclock::resources;

namespace midiclock
{
    namespace
    {
        // Its own subfolder of the configuration folder, so the clocks sit alongside the
        // service configuration without being mistaken for part of it.
        constexpr wchar_t ClockFolderName[] = L"MIDI Clock";
        constexpr wchar_t ClockFileName[] = L"clocks.json";

        constexpr wchar_t KeyComment[] = L"_comment";
        constexpr wchar_t KeyVersion[] = L"version";
        constexpr wchar_t KeyClocks[] = L"clocks";
        constexpr wchar_t KeyId[] = L"id";
        constexpr wchar_t KeyName[] = L"name";
        constexpr wchar_t KeyBeatsPerMinute[] = L"bpm";
        constexpr wchar_t KeyEndpointDeviceId[] = L"endpointDeviceId";
        constexpr wchar_t KeyEndpointName[] = L"endpointName";
        constexpr wchar_t KeyGroup[] = L"group";
        constexpr wchar_t KeyPulsesPerQuarterNote[] = L"ppqn";
        constexpr wchar_t KeySendStartStop[] = L"sendStartStop";
        constexpr wchar_t KeyDisplayOrder[] = L"displayOrder";

        constexpr wchar_t FileComment[] =
            L"Saved clocks for the Windows MIDI Clock app. The Windows MIDI Services service "
            L"does not read this file.";

        constexpr int32_t FileVersion = 1;

        // Untrusted input: this file lives in a folder any authenticated user can write to.
        constexpr size_t MaximumFileBytes = 1024 * 1024;
        constexpr size_t MaximumStringLength = 1024;

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
                static_cast<uint64_t>(size.QuadPart) > MaximumFileBytes)
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

        // The configuration folder grants write but not delete, so the file is rewritten in
        // place rather than replaced through a temporary copy.
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

        std::wstring TrimmedToLength(_In_ winrt::hstring const& value) noexcept
        {
            std::wstring result{ value };

            if (result.size() > MaximumStringLength)
            {
                result.resize(MaximumStringLength);
            }

            // a control character in a name would corrupt the display rather than say anything
            std::erase_if(result, [](wchar_t ch) { return ch < L' '; });

            return result;
        }

        winrt::hstring GetNamedStringOrEmpty(
            _In_ json::JsonObject const& parent,
            _In_ std::wstring_view const key) noexcept
        {
            try
            {
                if (!parent.HasKey(key))
                {
                    return {};
                }

                auto const value = parent.GetNamedValue(key);

                if (value == nullptr || value.ValueType() != json::JsonValueType::String)
                {
                    return {};
                }

                return value.GetString();
            }
            catch (...)
            {
                return {};
            }
        }

        double GetNamedNumberOrDefault(
            _In_ json::JsonObject const& parent,
            _In_ std::wstring_view const key,
            _In_ double const defaultValue) noexcept
        {
            try
            {
                if (!parent.HasKey(key))
                {
                    return defaultValue;
                }

                auto const value = parent.GetNamedValue(key);

                if (value == nullptr || value.ValueType() != json::JsonValueType::Number)
                {
                    return defaultValue;
                }

                auto const number = value.GetNumber();

                return std::isfinite(number) ? number : defaultValue;
            }
            catch (...)
            {
                return defaultValue;
            }
        }

        bool GetNamedBooleanOrDefault(
            _In_ json::JsonObject const& parent,
            _In_ std::wstring_view const key,
            _In_ bool const defaultValue) noexcept
        {
            try
            {
                if (!parent.HasKey(key))
                {
                    return defaultValue;
                }

                auto const value = parent.GetNamedValue(key);

                if (value == nullptr || value.ValueType() != json::JsonValueType::Boolean)
                {
                    return defaultValue;
                }

                return value.GetBoolean();
            }
            catch (...)
            {
                return defaultValue;
            }
        }

        // Windows.Data.Json stringifies onto one line. The file sits next to the service
        // configuration, which people do edit by hand, so it is laid out the same way.
        std::wstring Prettify(_In_ std::wstring const& compact) noexcept
        {
            try
            {
                std::wstring result{};
                result.reserve(compact.size() * 2);

                int depth{ 0 };
                bool inString{ false };
                bool escaped{ false };

                auto newLine = [&result](int indent)
                    {
                        result.append(L"\r\n");
                        result.append(static_cast<size_t>(indent) * 2, L' ');
                    };

                for (auto const ch : compact)
                {
                    if (inString)
                    {
                        result.push_back(ch);

                        if (escaped)
                        {
                            escaped = false;
                        }
                        else if (ch == L'\\')
                        {
                            escaped = true;
                        }
                        else if (ch == L'"')
                        {
                            inString = false;
                        }

                        continue;
                    }

                    switch (ch)
                    {
                    case L'"':
                        inString = true;
                        result.push_back(ch);
                        break;

                    case L'{':
                    case L'[':
                        result.push_back(ch);
                        depth++;
                        newLine(depth);
                        break;

                    case L'}':
                    case L']':
                        depth--;
                        newLine(depth);
                        result.push_back(ch);
                        break;

                    case L',':
                        result.push_back(ch);
                        newLine(depth);
                        break;

                    case L':':
                        result.push_back(ch);
                        result.push_back(L' ');
                        break;

                    default:
                        result.push_back(ch);
                        break;
                    }
                }

                result.append(L"\r\n");

                return result;
            }
            catch (...)
            {
                return compact;
            }
        }
    }

    ClockStore& ClockStore::Current() noexcept
    {
        static ClockStore instance{};

        return instance;
    }

    ClockStore::ClockStore() noexcept
    {
        ResolveDefaultPath();
    }

    void ClockStore::ResolveDefaultPath() noexcept
    {
        try
        {
            wchar_t folder[MAX_PATH]{};

            auto const expanded = ::ExpandEnvironmentStringsW(
                MIDI_CONFIG_FILE_FOLDER, folder, ARRAYSIZE(folder));

            if (expanded == 0 || expanded > ARRAYSIZE(folder))
            {
                return;
            }

            std::filesystem::path root{ folder };

            m_folder = (root / ClockFolderName).wstring();
            m_path = (root / ClockFolderName / ClockFileName).wstring();
        }
        catch (...)
        {
            m_folder.clear();
            m_path.clear();
        }
    }

    std::wstring ClockStore::NewId() noexcept
    {
        try
        {
            GUID value{};

            if (FAILED(::CoCreateGuid(&value)))
            {
                return {};
            }

            wchar_t buffer[64]{};

            if (::StringFromGUID2(value, buffer, ARRAYSIZE(buffer)) == 0)
            {
                return {};
            }

            std::wstring result{ buffer };

            if (result.size() >= 2)
            {
                result = result.substr(1, result.size() - 2);
            }

            std::transform(result.begin(), result.end(), result.begin(),
                [](wchar_t ch) { return static_cast<wchar_t>(::towlower(ch)); });

            return result;
        }
        catch (...)
        {
            return {};
        }
    }

    _Use_decl_annotations_
    ClockDefinition const* ClockStore::Find(std::wstring const& id) const noexcept
    {
        if (id.empty())
        {
            return nullptr;
        }

        auto const match = std::find_if(m_clocks.begin(), m_clocks.end(),
            [&id](ClockDefinition const& entry) { return entry.Id == id; });

        return match == m_clocks.end() ? nullptr : &(*match);
    }

    _Use_decl_annotations_
    ClockDefinition const* ClockStore::FindByEndpoint(std::wstring const& endpointDeviceId) const noexcept
    {
        if (endpointDeviceId.empty())
        {
            return nullptr;
        }

        auto const match = std::find_if(m_clocks.begin(), m_clocks.end(),
            [&endpointDeviceId](ClockDefinition const& entry)
            {
                return midiapp::EndpointIdsMatch(
                    winrt::hstring{ entry.EndpointDeviceId }, winrt::hstring{ endpointDeviceId });
            });

        return match == m_clocks.end() ? nullptr : &(*match);
    }

    _Use_decl_annotations_
    std::wstring ClockStore::Upsert(ClockDefinition definition) noexcept
    {
        try
        {
            if (definition.Id.empty())
            {
                definition.Id = NewId();
            }

            definition.BeatsPerMinute = std::clamp(
                definition.BeatsPerMinute, MinimumBeatsPerMinute, MaximumBeatsPerMinute);

            definition.PulsesPerQuarterNote = std::clamp(
                definition.PulsesPerQuarterNote, MinimumPulsesPerQuarterNote, MaximumPulsesPerQuarterNote);

            if (definition.GroupIndex != AllDeclaredGroups)
            {
                definition.GroupIndex = std::clamp(definition.GroupIndex, 0, 15);
            }

            auto const match = std::find_if(m_clocks.begin(), m_clocks.end(),
                [&definition](ClockDefinition const& entry) { return entry.Id == definition.Id; });

            if (match != m_clocks.end())
            {
                *match = definition;
            }
            else if (!IsFull())
            {
                m_clocks.push_back(definition);
            }
            else
            {
                m_lastError = res::GetString(L"ClockStoreFullError");
                return {};
            }

            return definition.Id;
        }
        catch (...)
        {
            return {};
        }
    }

    _Use_decl_annotations_
    void ClockStore::Remove(std::wstring const& id) noexcept
    {
        try
        {
            std::erase_if(m_clocks, [&id](ClockDefinition const& entry) { return entry.Id == id; });
        }
        catch (...)
        {
        }
    }

    bool ClockStore::Load() noexcept
    {
        m_clocks.clear();

        try
        {
            if (m_path.empty())
            {
                m_lastError = res::GetString(L"ClockStoreNoPathError");
                return false;
            }

            std::string bytes{};

            if (!ReadAllBytes(m_path, bytes))
            {
                auto const attributes = ::GetFileAttributesW(m_path.c_str());

                if (attributes == INVALID_FILE_ATTRIBUTES)
                {
                    // nothing saved yet
                    return true;
                }

                m_lastError = res::GetString(L"ClockStoreReadError");
                return false;
            }

            if (bytes.empty())
            {
                return true;
            }

            json::JsonObject root{ nullptr };

            if (!json::JsonObject::TryParse(winrt::hstring{ Utf8ToWide(bytes) }, root) || root == nullptr)
            {
                m_lastError = res::GetString(L"ClockStoreParseError");
                return false;
            }

            if (!root.HasKey(KeyClocks))
            {
                return true;
            }

            auto const clocksValue = root.GetNamedValue(KeyClocks);

            if (clocksValue == nullptr || clocksValue.ValueType() != json::JsonValueType::Array)
            {
                m_lastError = res::GetString(L"ClockStoreParseError");
                return false;
            }

            auto const clocks = clocksValue.GetArray();

            for (uint32_t index = 0; index < clocks.Size(); index++)
            {
                if (m_clocks.size() >= MaximumClockCount)
                {
                    break;
                }

                auto const entryValue = clocks.GetAt(index);

                // A JsonArray element is an IJsonValue; try_as<JsonObject> on it is always null.
                if (entryValue == nullptr || entryValue.ValueType() != json::JsonValueType::Object)
                {
                    continue;
                }

                auto const entry = entryValue.GetObject();

                ClockDefinition definition{};

                definition.Id = TrimmedToLength(GetNamedStringOrEmpty(entry, KeyId));

                if (definition.Id.empty())
                {
                    definition.Id = NewId();
                }

                definition.Name = TrimmedToLength(GetNamedStringOrEmpty(entry, KeyName));
                definition.EndpointDeviceId = TrimmedToLength(GetNamedStringOrEmpty(entry, KeyEndpointDeviceId));
                definition.EndpointName = TrimmedToLength(GetNamedStringOrEmpty(entry, KeyEndpointName));

                definition.BeatsPerMinute = std::clamp(
                    GetNamedNumberOrDefault(entry, KeyBeatsPerMinute, DefaultBeatsPerMinute),
                    MinimumBeatsPerMinute, MaximumBeatsPerMinute);

                auto const group = static_cast<int32_t>(
                    GetNamedNumberOrDefault(entry, KeyGroup, 0.0));

                definition.GroupIndex = group == AllDeclaredGroups ? AllDeclaredGroups : std::clamp(group, 0, 15);

                definition.PulsesPerQuarterNote = std::clamp(
                    static_cast<int32_t>(GetNamedNumberOrDefault(
                        entry, KeyPulsesPerQuarterNote, DefaultPulsesPerQuarterNote)),
                    MinimumPulsesPerQuarterNote, MaximumPulsesPerQuarterNote);

                definition.SendStartStop = GetNamedBooleanOrDefault(entry, KeySendStartStop, true);

                definition.DisplayOrder = static_cast<int32_t>(
                    std::clamp(GetNamedNumberOrDefault(entry, KeyDisplayOrder, static_cast<double>(index)),
                        0.0, static_cast<double>(MaximumClockCount)));

                // two entries claiming the same id would make every lookup ambiguous
                if (Find(definition.Id) != nullptr)
                {
                    definition.Id = NewId();
                }

                m_clocks.push_back(std::move(definition));
            }

            std::stable_sort(m_clocks.begin(), m_clocks.end(),
                [](ClockDefinition const& left, ClockDefinition const& right)
                {
                    return left.DisplayOrder < right.DisplayOrder;
                });

            m_lastError = {};

            return true;
        }
        MIDI_CLOCK_CATCH_AND_LOG(L"Unable to read the saved clocks.")

        m_lastError = res::GetString(L"ClockStoreReadError");

        return false;
    }

    bool ClockStore::Save() noexcept
    {
        try
        {
            if (m_path.empty() || m_folder.empty())
            {
                m_lastError = res::GetString(L"ClockStoreNoPathError");
                return false;
            }

            if (!::CreateDirectoryW(m_folder.c_str(), nullptr) &&
                ::GetLastError() != ERROR_ALREADY_EXISTS)
            {
                m_lastError = res::GetString(L"ClockStoreFolderError");
                return false;
            }

            json::JsonObject root{};

            root.SetNamedValue(KeyComment, json::JsonValue::CreateStringValue(FileComment));
            root.SetNamedValue(KeyVersion, json::JsonValue::CreateNumberValue(FileVersion));

            json::JsonArray clocks{};

            int32_t order{ 0 };

            for (auto const& definition : m_clocks)
            {
                json::JsonObject entry{};

                entry.SetNamedValue(KeyId, json::JsonValue::CreateStringValue(definition.Id));
                entry.SetNamedValue(KeyName, json::JsonValue::CreateStringValue(definition.Name));
                entry.SetNamedValue(KeyBeatsPerMinute, json::JsonValue::CreateNumberValue(definition.BeatsPerMinute));
                entry.SetNamedValue(KeyEndpointDeviceId, json::JsonValue::CreateStringValue(definition.EndpointDeviceId));
                entry.SetNamedValue(KeyEndpointName, json::JsonValue::CreateStringValue(definition.EndpointName));
                entry.SetNamedValue(KeyGroup, json::JsonValue::CreateNumberValue(definition.GroupIndex));
                entry.SetNamedValue(KeyPulsesPerQuarterNote, json::JsonValue::CreateNumberValue(definition.PulsesPerQuarterNote));
                entry.SetNamedValue(KeySendStartStop, json::JsonValue::CreateBooleanValue(definition.SendStartStop));
                entry.SetNamedValue(KeyDisplayOrder, json::JsonValue::CreateNumberValue(order++));

                clocks.Append(entry);
            }

            root.SetNamedValue(KeyClocks, clocks);

            auto const text = Prettify(std::wstring{ root.Stringify() });

            if (!WriteAllBytes(m_path, WideToUtf8(text)))
            {
                m_lastError = res::GetString(L"ClockStoreWriteError");
                return false;
            }

            m_lastError = {};

            return true;
        }
        MIDI_CLOCK_CATCH_AND_LOG(L"Unable to save the clocks.")

        m_lastError = res::GetString(L"ClockStoreWriteError");

        return false;
    }
}
