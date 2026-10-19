// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "SynthStore.h"

namespace midisoundmodule
{
    namespace
    {
        // Under %LOCALAPPDATA%, beside the other MIDI tools' folders.
        constexpr wchar_t FolderName[] = L"Microsoft\\MIDI Sound Module";
        constexpr wchar_t FileName[] = L"synths.json";

        constexpr wchar_t FileComment[] = L"Synths for the Windows MIDI Sound Module app.";

        constexpr size_t MaximumFileBytes = 1024 * 1024;

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

        enum class ReadOutcome
        {
            Read,
            Missing,
            TooLarge,
            Failed,
        };

        ReadOutcome ReadAllText(_In_ std::wstring const& path, _Out_ std::string& contents, _Out_ uint32_t& error) noexcept
        {
            contents.clear();
            error = ERROR_SUCCESS;

            try
            {
                wil::unique_hfile file{};

                // Something else may hold the file for a moment, such as a virus scanner, and a few
                // tries ride that out rather than refusing to start.
                for (uint32_t attempt = 1; ; attempt++)
                {
                    file.reset(::CreateFileW(
                        path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));

                    if (file)
                    {
                        break;
                    }

                    error = ::GetLastError();

                    if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND)
                    {
                        return ReadOutcome::Missing;
                    }

                    if ((error != ERROR_SHARING_VIOLATION && error != ERROR_LOCK_VIOLATION) || attempt == 10)
                    {
                        return ReadOutcome::Failed;
                    }

                    ::Sleep(100);
                }

                if (::GetFileType(file.get()) != FILE_TYPE_DISK)
                {
                    error = ERROR_BAD_FILE_TYPE;
                    return ReadOutcome::Failed;
                }

                LARGE_INTEGER size{};

                if (!::GetFileSizeEx(file.get(), &size) || size.QuadPart < 0)
                {
                    error = ::GetLastError();
                    return ReadOutcome::Failed;
                }

                if (static_cast<uint64_t>(size.QuadPart) > MaximumFileBytes)
                {
                    return ReadOutcome::TooLarge;
                }

                contents.resize(static_cast<size_t>(size.QuadPart));

                DWORD read{ 0 };

                if (!contents.empty() &&
                    !::ReadFile(file.get(), contents.data(), static_cast<DWORD>(contents.size()), &read, nullptr))
                {
                    error = ::GetLastError();
                    contents.clear();
                    return ReadOutcome::Failed;
                }

                contents.resize(read);

                if (contents.size() >= 3 &&
                    static_cast<unsigned char>(contents[0]) == 0xEF &&
                    static_cast<unsigned char>(contents[1]) == 0xBB &&
                    static_cast<unsigned char>(contents[2]) == 0xBF)
                {
                    contents.erase(0, 3);
                }

                return ReadOutcome::Read;
            }
            catch (...)
            {
                contents.clear();
                error = ERROR_OUTOFMEMORY;
                return ReadOutcome::Failed;
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

    SynthList SynthStore::Load() noexcept
    {
        SynthList list{};

        try
        {
            auto const path = FilePath();

            // With nowhere to keep a list there is none, and nothing to lose by starting empty.
            if (path.empty())
            {
                return list;
            }

            std::string text;
            uint32_t error{ ERROR_SUCCESS };

            switch (ReadAllText(path, text, error))
            {
            case ReadOutcome::Missing:
                return list;

            case ReadOutcome::TooLarge:
                list.Problem = SynthListProblem::TooLarge;
                break;

            case ReadOutcome::Failed:
                list.Problem = SynthListProblem::CannotOpen;
                list.SystemError = error;
                break;

            case ReadOutcome::Read:
            default:
                // An empty file holds no synths.
                if (!text.empty())
                {
                    list = ParseSynthList(text);
                }
                break;
            }

            if (list.Problem != SynthListProblem::None)
            {
                MIDI_SF2SYNTH_LOG_WARNING(L"The synth list could not be read. It is left as it is.");
                return list;
            }

            for (auto& synth : list.Synths)
            {
                if (synth.ProductInstanceId.empty())
                {
                    synth.ProductInstanceId = NewProductInstanceId();
                }
            }
        }
        catch (...)
        {
            MIDI_SF2SYNTH_LOG_GENERAL_EXCEPTION(L"Unable to load the synth list.");

            list = SynthList{};
            list.Problem = SynthListProblem::CannotOpen;
            list.SystemError = ERROR_OUTOFMEMORY;
        }

        return list;
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
            root.SetNamedValue(SynthListKeys::Comment, json::JsonValue::CreateStringValue(FileComment));
            root.SetNamedValue(SynthListKeys::Version, json::JsonValue::CreateNumberValue(SynthListFileVersion));

            json::JsonArray list{};

            for (auto const& synth : synths)
            {
                json::JsonObject item{};

                item.SetNamedValue(SynthListKeys::Id, json::JsonValue::CreateStringValue(synth.Id));
                item.SetNamedValue(SynthListKeys::Name, json::JsonValue::CreateStringValue(synth.Name));
                item.SetNamedValue(SynthListKeys::SoundFont, json::JsonValue::CreateStringValue(synth.SoundFontPath));
                item.SetNamedValue(SynthListKeys::ProductInstanceId, json::JsonValue::CreateStringValue(winrt::to_hstring(synth.ProductInstanceId)));
                item.SetNamedValue(SynthListKeys::VolumeDb, json::JsonValue::CreateNumberValue(synth.VolumeDb));
                item.SetNamedValue(SynthListKeys::Enabled, json::JsonValue::CreateBooleanValue(synth.Enabled));

                list.Append(item);
            }

            root.SetNamedValue(SynthListKeys::Synths, list);

            return WriteAllText(folder + L"\\" + FileName, winrt::to_string(root.Stringify()));
        }
        MIDI_SF2SYNTH_CATCH_AND_LOG(L"Unable to save the synth list.")

        return false;
    }
}
