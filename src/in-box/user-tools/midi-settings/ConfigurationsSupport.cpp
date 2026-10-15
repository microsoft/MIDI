// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================
//
// UNSUPPORTED - DO NOT COPY THIS APPROACH. This code reads and writes the Windows MIDI Services
// configuration file directly. Microsoft does not support direct manipulation of that file by
// anything other than the in-box MIDI tools. Its format, its name, its location and the registry
// value that selects it are implementation details and can change in any release without notice,
// so this code can corrupt a configuration or stop working at any time. It exists for development
// and diagnostics inside the Windows MIDI Services repository only. Applications and third-party
// tools must use the Windows MIDI Services API (Windows.Devices.Midi2.ServiceConfig) instead.
// ============================================================================

#include "pch.h"
#include "ConfigurationsSupport.h"

#include "AppSettings.h"
#include "MidiConfig.h"
#include "StringResources.h"
#include "Feature_Servicing_MIDI2PortNamingRework.h"

namespace res = ::midisettings::resources;

namespace midisettings::configurations
{
    namespace
    {
        constexpr wchar_t MidiServiceName[] = L"MidiSrv";

        constexpr uint64_t FileTimeTicksPerDay{ 24ull * 60 * 60 * 10'000'000ull };

        // Enough for every configuration anyone keeps, and small enough for one registry value.
        constexpr size_t MaximumImportedRecords{ 50 };

        engine::PortNaming FromAppNaming(_In_ Midi1PortNaming const naming) noexcept
        {
            switch (naming)
            {
            case Midi1PortNaming::ClassicCompatible: return engine::PortNaming::ClassicCompatible;
            case Midi1PortNaming::NewStyle: return engine::PortNaming::NewStyle;
            case Midi1PortNaming::Automatic: return engine::PortNaming::Automatic;
            default: return engine::PortNaming::None;
            }
        }

        bool ToAppNaming(_In_ engine::PortNaming const naming, _Out_ Midi1PortNaming& value) noexcept
        {
            switch (naming)
            {
            case engine::PortNaming::ClassicCompatible: value = Midi1PortNaming::ClassicCompatible; return true;
            case engine::PortNaming::NewStyle: value = Midi1PortNaming::NewStyle; return true;
            case engine::PortNaming::Automatic: value = Midi1PortNaming::Automatic; return true;
            default: value = Midi1PortNaming::UseGlobalDefault; return false;
            }
        }

        std::wstring ThisComputerName()
        {
            wchar_t name[MAX_COMPUTERNAME_LENGTH + 1]{};
            DWORD size{ ARRAYSIZE(name) };

            if (!::GetComputerNameW(name, &size))
            {
                return {};
            }

            return std::wstring{ name, size };
        }

        std::wstring ThisAppVersion()
        {
            wchar_t path[MAX_PATH]{};

            if (::GetModuleFileNameW(nullptr, path, ARRAYSIZE(path)) == 0)
            {
                return {};
            }

            DWORD handle{ 0 };
            auto const size = ::GetFileVersionInfoSizeW(path, &handle);

            if (size == 0)
            {
                return {};
            }

            std::vector<uint8_t> buffer(size);

            VS_FIXEDFILEINFO* info{ nullptr };
            UINT infoSize{ 0 };

            if (!::GetFileVersionInfoW(path, 0, size, buffer.data()) ||
                !::VerQueryValueW(buffer.data(), L"\\", reinterpret_cast<void**>(&info), &infoSize) ||
                info == nullptr || infoSize < sizeof(VS_FIXEDFILEINFO))
            {
                return {};
            }

            return std::format(L"{}.{}.{}.{}",
                HIWORD(info->dwFileVersionMS), LOWORD(info->dwFileVersionMS),
                HIWORD(info->dwFileVersionLS), LOWORD(info->dwFileVersionLS));
        }

        std::wstring TemporaryFolder()
        {
            wchar_t folder[MAX_PATH + 1]{};

            auto const length = ::GetTempPathW(ARRAYSIZE(folder), folder);

            if (length == 0 || length > MAX_PATH)
            {
                return {};
            }

            std::wstring result{ folder, length };

            if (result.back() != L'\\')
            {
                result += L'\\';
            }

            return result;
        }

        std::wstring SystemErrorText(_In_ DWORD const error)
        {
            if (error == ERROR_SUCCESS)
            {
                return {};
            }

            wil::unique_hlocal_string buffer{};

            auto const characters = ::FormatMessageW(
                FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                nullptr,
                error,
                0,
                reinterpret_cast<LPWSTR>(buffer.put()),
                0,
                nullptr);

            if (characters == 0 || !buffer)
            {
                return std::format(L"0x{:08X}", error);
            }

            std::wstring message{ buffer.get() };

            while (!message.empty() && (message.back() == L'\r' || message.back() == L'\n' || message.back() == L' '))
            {
                message.pop_back();
            }

            return message;
        }

        bool ToLocalTime(_In_ FILETIME const& time, _Out_ SYSTEMTIME& local) noexcept
        {
            SYSTEMTIME utc{};

            local = {};

            return ::FileTimeToSystemTime(&time, &utc) && ::SystemTimeToTzSpecificLocalTime(nullptr, &utc, &local);
        }

        // The local date as a day number, for counting days between two times.
        bool LocalDayNumber(_In_ SYSTEMTIME const& local, _Out_ uint64_t& day) noexcept
        {
            SYSTEMTIME date{};
            date.wYear = local.wYear;
            date.wMonth = local.wMonth;
            date.wDay = local.wDay;

            FILETIME midnight{};

            day = 0;

            if (!::SystemTimeToFileTime(&date, &midnight))
            {
                return false;
            }

            day = engine::FileTimeValue(midnight) / FileTimeTicksPerDay;

            return true;
        }

        // Neither of these can hold the separators the record uses.
        std::wstring WithoutSeparators(_In_ std::wstring const& text)
        {
            std::wstring cleaned{};

            for (auto const ch : text)
            {
                if (ch != L'|' && ch != L';')
                {
                    cleaned += ch;
                }
            }

            return cleaned;
        }

        struct ImportedRecord
        {
            std::wstring FileName{};
            std::wstring ComputerName{};
            uint64_t Time{ 0 };
        };

        std::vector<ImportedRecord> ReadImportedRecords()
        {
            std::vector<ImportedRecord> records{};

            std::wstring const text{ AppSettings::Current().ImportedConfigurations() };

            size_t start{ 0 };

            while (start < text.size())
            {
                auto end = text.find(L';', start);

                if (end == std::wstring::npos)
                {
                    end = text.size();
                }

                auto const entry = text.substr(start, end - start);
                auto const first = entry.find(L'|');
                auto const second = first == std::wstring::npos ? std::wstring::npos : entry.find(L'|', first + 1);

                if (second != std::wstring::npos)
                {
                    ImportedRecord record{};
                    record.FileName = entry.substr(0, first);
                    record.ComputerName = entry.substr(first + 1, second - first - 1);

                    try
                    {
                        record.Time = std::stoull(entry.substr(second + 1));
                    }
                    catch (...)
                    {
                        record.Time = 0;
                    }

                    if (!record.FileName.empty())
                    {
                        records.push_back(std::move(record));
                    }
                }

                start = end + 1;
            }

            return records;
        }
    }

    engine::ConfigEnvironment MakeEnvironment() noexcept
    {
        engine::ConfigEnvironment environment{};

        try
        {
            auto const folder = config::FolderPath();

            environment.ConfigFolder = folder;
            environment.BackupFolder = folder.empty() ? std::wstring{} : folder + L"Backups\\";
            environment.TemporaryFolder = TemporaryFolder();
            environment.ComputerName = ThisComputerName();
            environment.AppVersion = ThisAppVersion();

            environment.ReadCurrentFileName = []() { return config::CurrentFileName(); };

            environment.WriteCurrentFileName = [](std::wstring const& fileName)
                {
                    std::wstring errorMessage{};
                    return config::SetCurrentFileName(fileName, errorMessage);
                };

            environment.ReadPortNaming = []() { return FromAppNaming(config::DefaultMidi1PortNaming()); };

            environment.WritePortNaming = [](engine::PortNaming const naming)
                {
                    Midi1PortNaming value{};

                    if (!ToAppNaming(naming, value))
                    {
                        return false;
                    }

                    std::wstring errorMessage{};
                    return config::SetDefaultMidi1PortNaming(value, errorMessage);
                };

            environment.AutomaticPortNamingAvailable = Feature_Servicing_MIDI2PortNamingRework::IsEnabled();

            environment.PicturePath = [](std::wstring const& fileName)
                {
                    return midiapp::EndpointImageAssets::Exists(fileName) ?
                        midiapp::EndpointImageAssets::FullPathForFileName(fileName) :
                        std::wstring{};
                };

            environment.StorePicture = [](std::wstring const& sourcePath)
                {
                    std::wstring errorMessage{};
                    return midiapp::EndpointImageAssets::CopyIntoFolder(sourcePath, errorMessage);
                };
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to describe this PC to the configuration engine.")

        return environment;
    }

    uint32_t ServiceProcessId() noexcept
    {
        try
        {
            wil::unique_schandle manager{ ::OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT) };

            if (!manager)
            {
                return 0;
            }

            wil::unique_schandle service{ ::OpenServiceW(manager.get(), MidiServiceName, SERVICE_QUERY_STATUS) };

            if (!service)
            {
                return 0;
            }

            SERVICE_STATUS_PROCESS status{};
            DWORD bytesNeeded{ 0 };

            if (!::QueryServiceStatusEx(
                service.get(),
                SC_STATUS_PROCESS_INFO,
                reinterpret_cast<LPBYTE>(&status),
                sizeof(status),
                &bytesNeeded))
            {
                return 0;
            }

            return status.dwCurrentState == SERVICE_STOPPED ? 0 : status.dwProcessId;
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to ask for the MIDI service's process.")

        return 0;
    }

    bool IsRtpMidiInstalled() noexcept
    {
        try
        {
            if (!midi2::MidiApi::EnsureServiceAvailable())
            {
                return true;
            }

            auto const installed = midi2rept::MidiReporting::GetInstalledTransportPlugins();

            if (installed == nullptr)
            {
                return true;
            }

            for (auto const& transport : installed)
            {
                if (engine::SameText(winrt::to_hstring(transport.TransportId()), engine::transportids::RtpMidi))
                {
                    return true;
                }
            }

            return false;
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to ask which transports are installed.")

        return true;
    }

    std::vector<LiveEndpoint> LiveEndpoints() noexcept
    {
        std::vector<LiveEndpoint> endpoints{};

        try
        {
            if (!midi2::MidiApi::EnsureServiceAvailable())
            {
                return endpoints;
            }

            for (auto const& endpoint : midi2enum::MidiEndpointDeviceInformation::FindAll())
            {
                LiveEndpoint live{};
                live.EndpointDeviceId = endpoint.EndpointDeviceId();
                live.DeviceInstanceId = endpoint.DeviceInstanceId();

                if (auto const info = endpoint.GetTransportSuppliedInfo())
                {
                    live.TransportSuppliedName = info.Name();
                }

                endpoints.push_back(std::move(live));
            }
        }
        catch (...)
        {
            MIDI_SETTINGS_LOG_GENERAL_EXCEPTION(L"Unable to list the endpoints that are here.");
            endpoints.clear();
        }

        return endpoints;
    }

    _Use_decl_annotations_
    PendingRestart ReadPendingRestart(uint32_t const serviceProcessId) noexcept
    {
        try
        {
            auto const& settings = AppSettings::Current();

            if (settings.PendingRestartKinds() == 0 || settings.PendingRestartServiceProcessId() != serviceProcessId)
            {
                return {};
            }

            PendingRestart pending{};
            pending.Kinds = settings.PendingRestartKinds();
            pending.RunningFileName = settings.PendingRestartRunningConfig();

            return pending;
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to read whether the MIDI service needs a restart.")

        return {};
    }

    _Use_decl_annotations_
    void AddPendingRestart(uint32_t const serviceProcessId, uint32_t const kinds, std::wstring const& runningFileName) noexcept
    {
        try
        {
            auto pending = ReadPendingRestart(serviceProcessId);

            if ((pending.Kinds & restartkinds::Switch) == 0 && (kinds & restartkinds::Switch) != 0)
            {
                pending.RunningFileName = runningFileName;
            }

            pending.Kinds |= kinds;

            WritePendingRestart(serviceProcessId, pending);
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to remember that the MIDI service needs a restart.")
    }

    _Use_decl_annotations_
    void WritePendingRestart(uint32_t const serviceProcessId, PendingRestart const& pending) noexcept
    {
        try
        {
            if (pending.Kinds == 0)
            {
                ClearPendingRestart();
                return;
            }

            AppSettings::Current().PendingRestart(serviceProcessId, pending.Kinds, pending.RunningFileName);
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to remember that the MIDI service needs a restart.")
    }

    void ClearPendingRestart() noexcept
    {
        try
        {
            AppSettings::Current().PendingRestart(0, 0, {});
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to forget a MIDI service restart.")
    }

    _Use_decl_annotations_
    void RememberImported(std::wstring const& fileName, std::wstring const& computerName) noexcept
    {
        try
        {
            auto records = ReadImportedRecords();

            std::erase_if(records, [&fileName](auto const& record) { return engine::SameText(record.FileName, fileName); });

            ImportedRecord record{};
            record.FileName = WithoutSeparators(fileName);
            record.ComputerName = WithoutSeparators(computerName);
            record.Time = engine::FileTimeValue(engine::CurrentFileTime());

            records.push_back(std::move(record));

            if (records.size() > MaximumImportedRecords)
            {
                records.erase(records.begin(), records.begin() + static_cast<std::ptrdiff_t>(records.size() - MaximumImportedRecords));
            }

            std::wstring text{};

            for (auto const& entry : records)
            {
                if (!text.empty())
                {
                    text += L';';
                }

                text += entry.FileName + L"|" + entry.ComputerName + L"|" + std::to_wstring(entry.Time);
            }

            AppSettings::Current().ImportedConfigurations(text);
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to remember where a configuration came from.")
    }

    _Use_decl_annotations_
    bool TryGetImported(std::wstring const& fileName, std::wstring& computerName, FILETIME& importedAt) noexcept
    {
        computerName.clear();
        importedAt = {};

        try
        {
            for (auto const& record : ReadImportedRecords())
            {
                if (engine::SameText(record.FileName, fileName) && record.Time != 0)
                {
                    computerName = record.ComputerName;
                    importedAt.dwLowDateTime = static_cast<DWORD>(record.Time & 0xFFFFFFFF);
                    importedAt.dwHighDateTime = static_cast<DWORD>(record.Time >> 32);

                    return true;
                }
            }
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to read where a configuration came from.")

        return false;
    }

    _Use_decl_annotations_
    winrt::hstring ErrorText(engine::ConfigResult const& result) noexcept
    {
        try
        {
            std::wstring_view key{ L"CfgErrorUnexpected" };

            switch (result.Error)
            {
            case engine::ConfigError::NoFolder:             key = L"CfgErrorNoFolder"; break;
            case engine::ConfigError::NameNotUsable:        key = L"CfgErrorNameNotUsable"; break;
            case engine::ConfigError::NameTaken:            key = L"CfgErrorNameTaken"; break;
            case engine::ConfigError::NotFound:             key = L"CfgErrorNotFound"; break;
            case engine::ConfigError::Empty:                key = L"CfgErrorEmpty"; break;
            case engine::ConfigError::TooLarge:             key = L"CfgErrorTooLarge"; break;
            case engine::ConfigError::NotAConfiguration:    key = L"CfgErrorNotAConfiguration"; break;
            case engine::ConfigError::NotAPackage:          key = L"CfgErrorNotAPackage"; break;
            case engine::ConfigError::NewerPackage:         key = L"CfgErrorNewerPackage"; break;
            case engine::ConfigError::Damaged:              key = L"CfgErrorDamaged"; break;
            case engine::ConfigError::InUse:                key = L"CfgErrorInUse"; break;
            case engine::ConfigError::ChangedSincePlanned:  key = L"CfgErrorChangedSincePlanned"; break;
            case engine::ConfigError::BackupFailed:         key = L"CfgErrorBackupFailed"; break;
            case engine::ConfigError::ReadFailed:           key = L"CfgErrorReadFailed"; break;
            case engine::ConfigError::WriteFailed:          key = L"CfgErrorWriteFailed"; break;
            case engine::ConfigError::LinkRefused:          key = L"CfgErrorLinkRefused"; break;
            case engine::ConfigError::RegistryFailed:       key = L"CfgErrorRegistryFailed"; break;
            case engine::ConfigError::DeleteFailed:         key = L"CfgErrorDeleteFailed"; break;
            default: break;
            }

            auto const text = res::GetString(key);
            auto const system = SystemErrorText(result.SystemError);

            return system.empty() ? text : res::FormatString(L"CfgErrorWithSystemFormat", text, system);
        }
        catch (...)
        {
            return res::GetString(L"CfgErrorUnexpected");
        }
    }

    _Use_decl_annotations_
    winrt::hstring CountText(std::wstring_view const key, size_t const count) noexcept
    {
        try
        {
            if (count == 1)
            {
                return res::GetString(std::wstring{ key } + L"One");
            }

            return res::FormatString(std::wstring{ key } + L"Format", count);
        }
        catch (...)
        {
            return winrt::to_hstring(count);
        }
    }

    _Use_decl_annotations_
    winrt::hstring ListText(std::vector<winrt::hstring> const& parts) noexcept
    {
        try
        {
            auto const separator = res::GetString(L"CfgListSeparator");

            std::wstring text{};

            for (auto const& part : parts)
            {
                if (part.empty())
                {
                    continue;
                }

                if (!text.empty())
                {
                    text += separator;
                }

                text += part;
            }

            return winrt::hstring{ text };
        }
        catch (...)
        {
            return {};
        }
    }

    _Use_decl_annotations_
    int64_t DaysAgo(FILETIME const& time) noexcept
    {
        SYSTEMTIME local{};
        SYSTEMTIME now{};

        uint64_t day{ 0 };
        uint64_t today{ 0 };

        ::GetLocalTime(&now);

        if (!ToLocalTime(time, local) || !LocalDayNumber(local, day) || !LocalDayNumber(now, today))
        {
            return 0;
        }

        return static_cast<int64_t>(today) - static_cast<int64_t>(day);
    }

    _Use_decl_annotations_
    winrt::hstring TimeText(FILETIME const& time) noexcept
    {
        try
        {
            SYSTEMTIME local{};

            if (!ToLocalTime(time, local))
            {
                return {};
            }

            wchar_t buffer[64]{};

            if (::GetTimeFormatEx(LOCALE_NAME_USER_DEFAULT, TIME_NOSECONDS, &local, nullptr, buffer, ARRAYSIZE(buffer)) == 0)
            {
                return {};
            }

            return winrt::hstring{ buffer };
        }
        catch (...)
        {
            return {};
        }
    }

    _Use_decl_annotations_
    winrt::hstring DateText(FILETIME const& time, std::wstring_view const pictureKey) noexcept
    {
        try
        {
            SYSTEMTIME local{};
            SYSTEMTIME now{};

            ::GetLocalTime(&now);

            if (!ToLocalTime(time, local))
            {
                return {};
            }

            auto const picture = local.wYear == now.wYear ?
                res::GetString(pictureKey) :
                res::GetString(std::wstring{ pictureKey } + L"WithYear");

            wchar_t buffer[128]{};

            if (::GetDateFormatEx(LOCALE_NAME_USER_DEFAULT, 0, &local, picture.c_str(), buffer, ARRAYSIZE(buffer), nullptr) == 0)
            {
                return {};
            }

            return winrt::hstring{ buffer };
        }
        catch (...)
        {
            return {};
        }
    }

    _Use_decl_annotations_
    winrt::hstring WhenText(FILETIME const& time, bool const capitalized) noexcept
    {
        try
        {
            auto const days = DaysAgo(time);
            auto const clock = TimeText(time);

            if (days == 0)
            {
                return res::FormatString(capitalized ? L"CfgWhenTodayCapitalFormat" : L"CfgWhenTodayFormat", clock);
            }

            if (days == 1)
            {
                return res::FormatString(capitalized ? L"CfgWhenYesterdayCapitalFormat" : L"CfgWhenYesterdayFormat", clock);
            }

            return res::FormatString(
                capitalized ? L"CfgWhenDateCapitalFormat" : L"CfgWhenDateFormat",
                DateText(time, L"CfgDatePictureShort"),
                clock);
        }
        catch (...)
        {
            return {};
        }
    }

    _Use_decl_annotations_
    winrt::hstring ShortContentsText(engine::ConfigInventory const& inventory) noexcept
    {
        try
        {
            if (inventory.IsEmpty())
            {
                return res::GetString(L"CfgNothingSetUp");
            }

            constexpr size_t MaximumParts{ 3 };

            std::vector<winrt::hstring> parts{};

            auto const add = [&parts](std::wstring_view const key, size_t const count)
                {
                    if (count > 0 && parts.size() < MaximumParts)
                    {
                        parts.push_back(CountText(key, count));
                    }
                };

            add(L"CfgCountDeviceNames", inventory.DeviceNames.size());
            add(L"CfgCountPictures", inventory.Pictures.size());
            add(L"CfgCountLoopbacks", inventory.Loopbacks.size());
            add(L"CfgCountNetworkHosts", inventory.Hosts.size());
            add(L"CfgCountBluetoothDevices", inventory.BluetoothDevices.size());

            if (parts.empty())
            {
                return res::GetString(inventory.HasSynthesizer ? L"CfgContentsSynthesizer" : L"CfgContentsOtherSettings");
            }

            return ListText(parts);
        }
        catch (...)
        {
            return {};
        }
    }

    _Use_decl_annotations_
    winrt::hstring SynthModeText(std::wstring const& synthMode) noexcept
    {
        if (synthMode.empty())
        {
            return res::GetString(L"CfgNoValue");
        }

        if (engine::SameText(synthMode, L"compatible"))
        {
            return res::GetString(L"CfgSynthModeCompatible");
        }

        if (engine::SameText(synthMode, L"modern"))
        {
            return res::GetString(L"CfgSynthModeModern");
        }

        return winrt::hstring{ synthMode };
    }

    _Use_decl_annotations_
    std::wstring ConfigDisplayName(engine::ConfigFileEntry const& entry)
    {
        if (!entry.ConfigName.empty())
        {
            return entry.ConfigName;
        }

        auto name = entry.FileName;

        if (engine::EndsWithText(name, engine::ConfigFileSuffix))
        {
            name.resize(name.size() - engine::ConfigFileSuffix.size());
        }

        return name;
    }

    _Use_decl_annotations_
    std::wstring SafeFileName(std::wstring const& name)
    {
        std::wstring cleaned{};

        for (auto const ch : name)
        {
            if (ch < 0x20 || ch == L'\\' || ch == L'/' || ch == L':' || ch == L'*' ||
                ch == L'?' || ch == L'"' || ch == L'<' || ch == L'>' || ch == L'|')
            {
                continue;
            }

            cleaned += ch;
        }

        while (!cleaned.empty() && (cleaned.back() == L' ' || cleaned.back() == L'.'))
        {
            cleaned.pop_back();
        }

        while (!cleaned.empty() && (cleaned.front() == L' ' || cleaned.front() == L'.'))
        {
            cleaned.erase(cleaned.begin());
        }

        return cleaned.empty() ? std::wstring{ res::GetString(L"CfgDefaultPackageName") } : cleaned;
    }

    _Use_decl_annotations_
    std::wstring ParentFolder(std::wstring const& path)
    {
        auto const separator = path.find_last_of(L"\\/");

        return separator == std::wstring::npos ? std::wstring{} : path.substr(0, separator);
    }

    _Use_decl_annotations_
    std::wstring ShowSavePackageDialog(
        HWND const owner,
        std::wstring const& suggestedFileName,
        std::wstring const& startFolder) noexcept
    {
        try
        {
            winrt::com_ptr<IFileSaveDialog> dialog{};

            if (FAILED(::CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(dialog.put()))))
            {
                return {};
            }

            std::wstring const packages{ res::GetString(L"CfgPackageFileTypeName") };
            std::wstring const allFiles{ res::GetString(L"CfgAllFilesFileTypeName") };

            COMDLG_FILTERSPEC const filters[]
            {
                { packages.c_str(), L"*.midiconfigpack" },
                { allFiles.c_str(), L"*.*" },
            };

            LOG_IF_FAILED(dialog->SetFileTypes(ARRAYSIZE(filters), filters));
            LOG_IF_FAILED(dialog->SetDefaultExtension(L"midiconfigpack"));
            LOG_IF_FAILED(dialog->SetOptions(FOS_OVERWRITEPROMPT | FOS_PATHMUSTEXIST | FOS_FORCEFILESYSTEM | FOS_STRICTFILETYPES));

            if (!suggestedFileName.empty())
            {
                LOG_IF_FAILED(dialog->SetFileName(suggestedFileName.c_str()));
            }

            if (!startFolder.empty())
            {
                winrt::com_ptr<IShellItem> folder{};

                if (SUCCEEDED(::SHCreateItemFromParsingName(startFolder.c_str(), nullptr, IID_PPV_ARGS(folder.put()))))
                {
                    LOG_IF_FAILED(dialog->SetFolder(folder.get()));
                }
            }

            // canceling comes back as a failure, so it isn't logged
            if (FAILED(dialog->Show(owner)))
            {
                return {};
            }

            winrt::com_ptr<IShellItem> item{};

            if (FAILED(dialog->GetResult(item.put())))
            {
                return {};
            }

            wil::unique_cotaskmem_string path{};

            if (FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, path.put())))
            {
                return {};
            }

            return std::wstring{ path.get() };
        }
        catch (...)
        {
            return {};
        }
    }

    _Use_decl_annotations_
    std::wstring ShowOpenPackageDialog(HWND const owner, std::wstring const& startFolder) noexcept
    {
        try
        {
            winrt::com_ptr<IFileOpenDialog> dialog{};

            if (FAILED(::CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(dialog.put()))))
            {
                return {};
            }

            std::wstring const packages{ res::GetString(L"CfgPackageOrConfigFileTypeName") };
            std::wstring const allFiles{ res::GetString(L"CfgAllFilesFileTypeName") };

            COMDLG_FILTERSPEC const filters[]
            {
                { packages.c_str(), L"*.midiconfigpack;*.midiconfig.json" },
                { allFiles.c_str(), L"*.*" },
            };

            LOG_IF_FAILED(dialog->SetFileTypes(ARRAYSIZE(filters), filters));
            LOG_IF_FAILED(dialog->SetOptions(FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST | FOS_FORCEFILESYSTEM));

            if (!startFolder.empty())
            {
                winrt::com_ptr<IShellItem> folder{};

                if (SUCCEEDED(::SHCreateItemFromParsingName(startFolder.c_str(), nullptr, IID_PPV_ARGS(folder.put()))))
                {
                    LOG_IF_FAILED(dialog->SetFolder(folder.get()));
                }
            }

            if (FAILED(dialog->Show(owner)))
            {
                return {};
            }

            winrt::com_ptr<IShellItem> item{};

            if (FAILED(dialog->GetResult(item.put())))
            {
                return {};
            }

            wil::unique_cotaskmem_string path{};

            if (FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, path.put())))
            {
                return {};
            }

            return std::wstring{ path.get() };
        }
        catch (...)
        {
            return {};
        }
    }

    _Use_decl_annotations_
    void ShowInFolder(std::wstring const& path) noexcept
    {
        try
        {
            if (path.empty())
            {
                return;
            }

            auto const item = ::ILCreateFromPathW(path.c_str());

            if (item == nullptr)
            {
                return;
            }

            LOG_IF_FAILED(::SHOpenFolderAndSelectItems(item, 0, nullptr, 0));

            ::ILFree(item);
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to show a package in its folder.")
    }
}
