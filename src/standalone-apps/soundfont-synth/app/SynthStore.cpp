// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "SynthStore.h"

#include "SynthCore.h"

namespace midisoundfontsynth
{
    namespace
    {
        constexpr wchar_t FolderName[] = L"MIDI 2.0 SoundFont Synth";
        constexpr wchar_t FileName[] = L"synths.json";

        constexpr wchar_t KeyComment[] = L"_comment";
        constexpr wchar_t KeyVersion[] = L"version";
        constexpr wchar_t KeySynths[] = L"synths";
        constexpr wchar_t KeyId[] = L"id";
        constexpr wchar_t KeyName[] = L"name";
        constexpr wchar_t KeySoundFont[] = L"soundFont";
        constexpr wchar_t KeyProductInstanceId[] = L"productInstanceId";
        constexpr wchar_t KeyVolumeDb[] = L"volumeDb";
        constexpr wchar_t KeyEnabled[] = L"enabled";

        constexpr wchar_t FileComment[] = L"Synths for the MIDI 2.0 SoundFont Synth app.";

        constexpr int32_t FileVersion = 1;

        constexpr size_t MaximumFileBytes = 1024 * 1024;
        constexpr size_t MaximumPathLength = 32767;
        constexpr size_t MaximumIdLength = 64;

        constexpr char ProductInstanceIdPrefix[] = "SF2SYNTH-";

        std::wstring FolderPath()
        {
            wil::unique_cotaskmem_string localAppData{};

            if (FAILED(::SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DEFAULT, nullptr, localAppData.put())) || !localAppData)
            {
                return {};
            }

            return std::wstring{ localAppData.get() } + L"\\" + FolderName;
        }

        bool ReadAllText(_In_ std::wstring const& path, _Out_ std::string& contents) noexcept
        {
            contents.clear();

            try
            {
                wil::unique_hfile file{ ::CreateFileW(
                    path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr) };

                if (!file || ::GetFileType(file.get()) != FILE_TYPE_DISK)
                {
                    return false;
                }

                LARGE_INTEGER size{};

                if (!::GetFileSizeEx(file.get(), &size) || size.QuadPart < 0 ||
                    static_cast<uint64_t>(size.QuadPart) > MaximumFileBytes)
                {
                    return false;
                }

                contents.resize(static_cast<size_t>(size.QuadPart));

                DWORD read{ 0 };

                if (!contents.empty() &&
                    !::ReadFile(file.get(), contents.data(), static_cast<DWORD>(contents.size()), &read, nullptr))
                {
                    contents.clear();
                    return false;
                }

                contents.resize(read);

                if (contents.size() >= 3 &&
                    static_cast<unsigned char>(contents[0]) == 0xEF &&
                    static_cast<unsigned char>(contents[1]) == 0xBB &&
                    static_cast<unsigned char>(contents[2]) == 0xBF)
                {
                    contents.erase(0, 3);
                }

                return true;
            }
            catch (...)
            {
                contents.clear();
                return false;
            }
        }

        // Written beside the real file and then swapped in, so a crash part way through leaves
        // the old list rather than half of a new one.
        bool WriteAllText(_In_ std::wstring const& path, _In_ std::string const& contents) noexcept
        {
            try
            {
                auto const temporary = path + L".new";

                {
                    wil::unique_hfile file{ ::CreateFileW(
                        temporary.c_str(), GENERIC_WRITE, 0, nullptr,
                        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr) };

                    if (!file)
                    {
                        return false;
                    }

                    DWORD written{ 0 };

                    if (!contents.empty() &&
                        (!::WriteFile(file.get(), contents.data(), static_cast<DWORD>(contents.size()), &written, nullptr) ||
                        written != contents.size()))
                    {
                        return false;
                    }

                    (void)::FlushFileBuffers(file.get());
                }

                return ::MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
            }
            catch (...)
            {
                return false;
            }
        }

        std::wstring Utf8ToWide(_In_ std::string const& text)
        {
            if (text.empty())
            {
                return {};
            }

            auto const required = ::MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);

            if (required <= 0)
            {
                return {};
            }

            std::wstring result(static_cast<size_t>(required), L'\0');

            ::MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), required);

            return result;
        }

        std::wstring ReadString(_In_ json::JsonObject const& parent, _In_ std::wstring_view key, _In_ size_t maximumLength)
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

            std::wstring text{ value.GetString() };

            if (text.size() > maximumLength)
            {
                return {};
            }

            return text;
        }

        double ReadNumber(_In_ json::JsonObject const& parent, _In_ std::wstring_view key, _In_ double fallback)
        {
            if (!parent.HasKey(key))
            {
                return fallback;
            }

            auto const value = parent.GetNamedValue(key);

            if (value == nullptr || value.ValueType() != json::JsonValueType::Number)
            {
                return fallback;
            }

            auto const number = value.GetNumber();

            return std::isfinite(number) ? number : fallback;
        }

        bool ReadBoolean(_In_ json::JsonObject const& parent, _In_ std::wstring_view key, _In_ bool fallback)
        {
            if (!parent.HasKey(key))
            {
                return fallback;
            }

            auto const value = parent.GetNamedValue(key);

            if (value == nullptr || value.ValueType() != json::JsonValueType::Boolean)
            {
                return fallback;
            }

            return value.GetBoolean();
        }

        bool IsValidId(_In_ std::wstring const& id) noexcept
        {
            if (id.empty() || id.size() > MaximumIdLength)
            {
                return false;
            }

            return std::all_of(id.begin(), id.end(), [](wchar_t ch)
                {
                    return (ch >= L'0' && ch <= L'9') || (ch >= L'a' && ch <= L'f') || (ch >= L'A' && ch <= L'F') ||
                        ch == L'-' || ch == L'{' || ch == L'}';
                });
        }

        // M2-104-UM: printable ASCII, and no longer than the stream notification can carry.
        bool IsValidProductInstanceId(_In_ std::wstring const& id) noexcept
        {
            if (id.empty() || id.size() > SoundFontSynth::SynthEndpointShape::MaximumProductInstanceIdBytes)
            {
                return false;
            }

            return std::all_of(id.begin(), id.end(), [](wchar_t ch) { return ch >= 0x20 && ch <= 0x7E; });
        }

        std::wstring CleanName(_In_ std::wstring name)
        {
            std::erase_if(name, [](wchar_t ch) { return ch < L' ' || ch == 0x7F; });

            auto const first = name.find_first_not_of(L' ');

            if (first == std::wstring::npos)
            {
                return {};
            }

            name.erase(0, first);
            name.erase(name.find_last_not_of(L' ') + 1);

            if (name.size() > SynthStore::MaximumNameLength)
            {
                name.resize(SynthStore::MaximumNameLength);
            }

            return name;
        }
    }

    std::wstring SynthStore::FilePath() noexcept
    {
        try
        {
            auto const folder = FolderPath();

            return folder.empty() ? std::wstring{} : folder + L"\\" + FileName;
        }
        catch (...)
        {
            return {};
        }
    }

    std::wstring SynthStore::NewId()
    {
        GUID guid{};
        THROW_IF_FAILED(::CoCreateGuid(&guid));

        return std::wstring{ winrt::to_hstring(guid) };
    }

    std::string SynthStore::NewProductInstanceId()
    {
        GUID guid{};
        THROW_IF_FAILED(::CoCreateGuid(&guid));

        char suffix[16]{};
        (void)snprintf(suffix, sizeof(suffix), "%08lX", static_cast<unsigned long>(guid.Data1));

        return std::string{ ProductInstanceIdPrefix } + suffix;
    }

    std::vector<SynthDefinition> SynthStore::Load() noexcept
    {
        std::vector<SynthDefinition> synths;

        try
        {
            auto const path = FilePath();

            std::string text;

            if (path.empty() || !ReadAllText(path, text) || text.empty())
            {
                return synths;
            }

            json::JsonObject root{ nullptr };

            if (!json::JsonObject::TryParse(Utf8ToWide(text), root) || root == nullptr || !root.HasKey(KeySynths))
            {
                MIDI_SF2SYNTH_LOG_WARNING(L"The synth list could not be read. Starting with none.");
                return synths;
            }

            auto const list = root.GetNamedValue(KeySynths);

            if (list == nullptr || list.ValueType() != json::JsonValueType::Array)
            {
                return synths;
            }

            for (auto const& entry : list.GetArray())
            {
                if (synths.size() >= MaximumSynths)
                {
                    break;
                }

                if (entry == nullptr || entry.ValueType() != json::JsonValueType::Object)
                {
                    continue;
                }

                auto const item = entry.GetObject();

                SynthDefinition definition{};

                definition.Id = ReadString(item, KeyId, MaximumIdLength);
                definition.SoundFontPath = ReadString(item, KeySoundFont, MaximumPathLength);

                if (!IsValidId(definition.Id) || definition.SoundFontPath.empty() || ::PathIsRelativeW(definition.SoundFontPath.c_str()))
                {
                    continue;
                }

                auto const duplicateId = std::any_of(synths.begin(), synths.end(),
                    [&definition](SynthDefinition const& other) { return other.Id == definition.Id; });

                if (duplicateId)
                {
                    continue;
                }

                definition.Name = CleanName(ReadString(item, KeyName, 1024));

                if (definition.Name.empty())
                {
                    definition.Name = CleanName(std::wstring{ ::PathFindFileNameW(definition.SoundFontPath.c_str()) });
                }

                auto const productInstanceId = ReadString(item, KeyProductInstanceId, 64);

                if (IsValidProductInstanceId(productInstanceId))
                {
                    // Checked above to be printable ASCII, so each character fits a byte.
                    for (auto const character : productInstanceId)
                    {
                        definition.ProductInstanceId.push_back(static_cast<char>(character));
                    }
                }

                // Two synths with one identity would be one endpoint, so a copied entry gets its own.
                auto const duplicateIdentity = definition.ProductInstanceId.empty() ||
                    std::any_of(synths.begin(), synths.end(),
                        [&definition](SynthDefinition const& other) { return other.ProductInstanceId == definition.ProductInstanceId; });

                if (duplicateIdentity)
                {
                    definition.ProductInstanceId = NewProductInstanceId();
                }

                definition.VolumeDb = (std::clamp)(
                    ReadNumber(item, KeyVolumeDb, 0.0),
                    SoundFontSynth::Synthesizer::MinimumUserVolumeDb,
                    SoundFontSynth::Synthesizer::MaximumUserVolumeDb);

                definition.Enabled = ReadBoolean(item, KeyEnabled, true);

                synths.push_back(std::move(definition));
            }
        }
        MIDI_SF2SYNTH_CATCH_AND_LOG(L"Unable to load the synth list.")

        return synths;
    }

    _Use_decl_annotations_
    bool SynthStore::Save(std::vector<SynthDefinition> const& synths) noexcept
    {
        try
        {
            auto const folder = FolderPath();

            if (folder.empty())
            {
                return false;
            }

            auto const created = ::SHCreateDirectoryExW(nullptr, folder.c_str(), nullptr);

            if (created != ERROR_SUCCESS && created != ERROR_ALREADY_EXISTS && created != ERROR_FILE_EXISTS)
            {
                return false;
            }

            json::JsonObject root{};
            root.SetNamedValue(KeyComment, json::JsonValue::CreateStringValue(FileComment));
            root.SetNamedValue(KeyVersion, json::JsonValue::CreateNumberValue(FileVersion));

            json::JsonArray list{};

            for (auto const& synth : synths)
            {
                json::JsonObject item{};

                item.SetNamedValue(KeyId, json::JsonValue::CreateStringValue(synth.Id));
                item.SetNamedValue(KeyName, json::JsonValue::CreateStringValue(synth.Name));
                item.SetNamedValue(KeySoundFont, json::JsonValue::CreateStringValue(synth.SoundFontPath));
                item.SetNamedValue(KeyProductInstanceId, json::JsonValue::CreateStringValue(winrt::to_hstring(synth.ProductInstanceId)));
                item.SetNamedValue(KeyVolumeDb, json::JsonValue::CreateNumberValue(synth.VolumeDb));
                item.SetNamedValue(KeyEnabled, json::JsonValue::CreateBooleanValue(synth.Enabled));

                list.Append(item);
            }

            root.SetNamedValue(KeySynths, list);

            return WriteAllText(folder + L"\\" + FileName, winrt::to_string(root.Stringify()));
        }
        MIDI_SF2SYNTH_CATCH_AND_LOG(L"Unable to save the synth list.")

        return false;
    }
}
