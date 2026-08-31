// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiConfig.h"

#include "MidiDefs.h"

namespace midisettings::config
{
    namespace
    {
        constexpr wchar_t ConfigFileSuffix[] = L".midiconfig.json";
        constexpr wchar_t DefaultMidi1PortNamingValueName[] = L"DefaultMidi1PortNaming";
        constexpr wchar_t MidiServiceName[] = L"MidiSrv";

        std::wstring FormatSystemError(DWORD const error) noexcept
        {
            try
            {
                wil::unique_hlocal_string buffer;

                auto const characters = ::FormatMessageW(
                    FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                    nullptr,
                    error,
                    0,
                    reinterpret_cast<LPWSTR>(buffer.put()),
                    0,
                    nullptr);

                if (characters > 0 && buffer)
                {
                    std::wstring message{ buffer.get() };

                    while (!message.empty() && (message.back() == L'\r' || message.back() == L'\n'))
                    {
                        message.pop_back();
                    }

                    return message;
                }
            }
            catch (...)
            {
            }

            return std::format(L"Error 0x{:08X}", error);
        }

        bool WaitForServiceState(SC_HANDLE const service, DWORD const desiredState, std::chrono::seconds const timeout) noexcept
        {
            auto const deadline = std::chrono::steady_clock::now() + timeout;

            while (std::chrono::steady_clock::now() < deadline)
            {
                SERVICE_STATUS_PROCESS status{};
                DWORD bytesNeeded{ 0 };

                if (!::QueryServiceStatusEx(
                    service,
                    SC_STATUS_PROCESS_INFO,
                    reinterpret_cast<LPBYTE>(&status),
                    sizeof(status),
                    &bytesNeeded))
                {
                    return false;
                }

                if (status.dwCurrentState == desiredState)
                {
                    return true;
                }

                ::Sleep(250);
            }

            return false;
        }
    }

    std::wstring FolderPath() noexcept
    {
        try
        {
            wchar_t folder[MAX_PATH]{};

            auto const expanded = ::ExpandEnvironmentStringsW(MIDI_CONFIG_FILE_FOLDER, folder, ARRAYSIZE(folder));

            if (expanded == 0 || expanded > ARRAYSIZE(folder))
            {
                return {};
            }

            return std::wstring{ folder };
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to resolve the configuration folder.")

        return {};
    }

    std::wstring CurrentFileName() noexcept
    {
        try
        {
            auto const value = wil::reg::try_get_value_string(
                HKEY_LOCAL_MACHINE, MIDI_ROOT_REG_KEY, MIDI_CONFIG_FILE_REG_VALUE);

            if (value.has_value())
            {
                return value.value();
            }
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to read the current configuration file name.")

        return {};
    }

    std::wstring CurrentFullPath() noexcept
    {
        try
        {
            auto const fileName = CurrentFileName();

            if (fileName.empty())
            {
                return {};
            }

            // Only ever a bare name inside the one folder the service will open.
            if (fileName.find_first_of(LR"(\/:*?"<>|)") != std::wstring::npos)
            {
                return {};
            }

            auto const fullPath = FolderPath() + fileName;

            if (!::PathFileExistsW(fullPath.c_str()))
            {
                return {};
            }

            return fullPath;
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to resolve the current configuration file.")

        return {};
    }

    _Use_decl_annotations_
    std::wstring ReadConfigName(std::wstring const& fullPath) noexcept
    {
        try
        {
            std::ifstream stream{ fullPath, std::ios::binary };

            if (!stream.is_open())
            {
                return {};
            }

            std::string bytes{ std::istreambuf_iterator<char>{ stream }, std::istreambuf_iterator<char>{} };

            if (bytes.size() >= 3 && static_cast<unsigned char>(bytes[0]) == 0xEF)
            {
                bytes.erase(0, 3);
            }

            auto const wide = winrt::to_hstring(bytes);

            json::JsonObject document{ nullptr };

            if (!json::JsonObject::TryParse(wide, document) || document == nullptr)
            {
                return {};
            }

            auto const header = document.TryLookup(L"header");

            if (header == nullptr || header.ValueType() != json::JsonValueType::Object)
            {
                return {};
            }

            auto const name = header.GetObject().TryLookup(L"configName");

            if (name == nullptr || name.ValueType() != json::JsonValueType::String)
            {
                return {};
            }

            return std::wstring{ name.GetString() };
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to read the configuration file name.")

        return {};
    }

    std::vector<ConfigFileInfo> EnumerateFiles() noexcept
    {
        std::vector<ConfigFileInfo> results{};

        try
        {
            auto const folder = FolderPath();

            if (folder.empty())
            {
                return results;
            }

            auto const currentFileName = CurrentFileName();

            WIN32_FIND_DATAW findData{};

            wil::unique_hfind find{ ::FindFirstFileW((folder + L"*" + ConfigFileSuffix).c_str(), &findData) };

            if (!find)
            {
                return results;
            }

            do
            {
                if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
                {
                    continue;
                }

                ConfigFileInfo info{};

                info.FileName = findData.cFileName;
                info.FullPath = folder + info.FileName;
                info.ConfigName = ReadConfigName(info.FullPath);
                info.IsCurrent = ::_wcsicmp(info.FileName.c_str(), currentFileName.c_str()) == 0;

                if (info.ConfigName.empty())
                {
                    info.ConfigName = info.FileName;
                }

                results.push_back(std::move(info));
            } while (::FindNextFileW(find.get(), &findData));

            std::sort(results.begin(), results.end(), [](auto const& left, auto const& right)
                {
                    return ::_wcsicmp(left.ConfigName.c_str(), right.ConfigName.c_str()) < 0;
                });
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to list the configuration files.")

        return results;
    }

    _Use_decl_annotations_
    std::wstring FileNameFromConfigName(std::wstring const& configName) noexcept
    {
        try
        {
            std::wstring cleaned{};

            // Same rule as the old settings app: keep it to characters that are safe in a file
            // name and free of the json and shell punctuation that has caused trouble before.
            for (auto const ch : configName)
            {
                if (::iswalnum(ch) || ch == L' ' || ch == L'-' || ch == L'_' || ch == L'(' || ch == L')')
                {
                    cleaned += ch;
                }
            }

            while (!cleaned.empty() && cleaned.front() == L' ')
            {
                cleaned.erase(cleaned.begin());
            }

            while (!cleaned.empty() && cleaned.back() == L' ')
            {
                cleaned.pop_back();
            }

            if (cleaned.empty())
            {
                return {};
            }

            return cleaned + ConfigFileSuffix;
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to build a configuration file name.")

        return {};
    }

    _Use_decl_annotations_
    bool SetCurrentFileName(std::wstring const& fileName, std::wstring& errorMessage) noexcept
    {
        errorMessage.clear();

        try
        {
            if (fileName.empty() || fileName.find_first_of(LR"(\/:*?"<>|)") != std::wstring::npos)
            {
                errorMessage = L"The configuration file name is not valid.";
                return false;
            }

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
                errorMessage = FormatSystemError(openResult);
                return false;
            }

            auto const setResult = ::RegSetValueExW(
                key.get(),
                MIDI_CONFIG_FILE_REG_VALUE,
                0,
                REG_SZ,
                reinterpret_cast<BYTE const*>(fileName.c_str()),
                static_cast<DWORD>((fileName.size() + 1) * sizeof(wchar_t)));

            if (setResult != ERROR_SUCCESS)
            {
                errorMessage = FormatSystemError(setResult);
                return false;
            }

            return true;
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to set the current configuration file.")

        errorMessage = L"An unexpected error occurred.";

        return false;
    }

    _Use_decl_annotations_
    bool CreateFile(
        std::wstring const& configName,
        std::wstring const& fileName,
        std::wstring& errorMessage) noexcept
    {
        errorMessage.clear();

        try
        {
            if (fileName.empty() || fileName.find_first_of(LR"(\/:*?"<>|)") != std::wstring::npos)
            {
                errorMessage = L"The configuration file name is not valid.";
                return false;
            }

            auto const folder = FolderPath();

            if (folder.empty())
            {
                errorMessage = L"The configuration folder could not be found.";
                return false;
            }

            auto const fullPath = folder + fileName;

            if (::PathFileExistsW(fullPath.c_str()))
            {
                errorMessage = L"A configuration file with that name already exists.";
                return false;
            }

            json::JsonObject document{};
            json::JsonObject header{};

            header.SetNamedValue(L"_comment",
                json::JsonValue::CreateStringValue(
                    L"NOTE: All json keys are case-sensitive, including GUIDs."));
            header.SetNamedValue(L"configName", json::JsonValue::CreateStringValue(winrt::hstring{ configName }));
            header.SetNamedValue(L"product", json::JsonValue::CreateStringValue(L"Windows MIDI Services"));
            header.SetNamedValue(L"fileVersion", json::JsonValue::CreateNumberValue(1.0));

            document.SetNamedValue(L"header", header);
            document.SetNamedValue(L"endpointTransportPluginSettings", json::JsonObject{});

            auto const text = winrt::to_string(document.Stringify());

            // In place write. The folder grants write but not delete to standard users, so a
            // temp file and rename is not available here.
            std::ofstream stream{ fullPath, std::ios::binary | std::ios::trunc };

            if (!stream.is_open())
            {
                errorMessage = L"The configuration file could not be created.";
                return false;
            }

            stream.write(text.data(), static_cast<std::streamsize>(text.size()));
            stream.close();

            return stream.good();
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to create the configuration file.")

        errorMessage = L"An unexpected error occurred.";

        return false;
    }

    _Use_decl_annotations_
    bool CopyCurrentFileTo(std::wstring const& destinationPath, std::wstring& errorMessage) noexcept
    {
        errorMessage.clear();

        try
        {
            auto const source = CurrentFullPath();

            if (source.empty())
            {
                errorMessage = L"There is no active configuration file to copy.";
                return false;
            }

            if (destinationPath.empty())
            {
                errorMessage = L"No destination was chosen.";
                return false;
            }

            if (!::CopyFileW(source.c_str(), destinationPath.c_str(), FALSE))
            {
                errorMessage = FormatSystemError(::GetLastError());
                return false;
            }

            return true;
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to copy the configuration file.")

        errorMessage = L"An unexpected error occurred.";

        return false;
    }

    Midi1PortNaming DefaultMidi1PortNaming() noexcept
    {
        try
        {
            auto const value = wil::reg::try_get_value_dword(
                HKEY_LOCAL_MACHINE, MIDI_ROOT_REG_KEY, DefaultMidi1PortNamingValueName);

            if (value.has_value())
            {
                auto const stored = static_cast<Midi1PortNaming>(value.value());

                if (stored == Midi1PortNaming::ClassicCompatible || stored == Midi1PortNaming::NewStyle)
                {
                    return stored;
                }
            }
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to read the default MIDI 1.0 port naming.")

        // matches MIDI_MIDI1_PORT_NAMING_DEFAULT_VALUE in the naming library
        return Midi1PortNaming::ClassicCompatible;
    }

    _Use_decl_annotations_
    bool SetDefaultMidi1PortNaming(Midi1PortNaming const value, std::wstring& errorMessage) noexcept
    {
        errorMessage.clear();

        try
        {
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
                errorMessage = FormatSystemError(openResult);
                return false;
            }

            DWORD const stored = static_cast<DWORD>(value);

            auto const setResult = ::RegSetValueExW(
                key.get(),
                DefaultMidi1PortNamingValueName,
                0,
                REG_DWORD,
                reinterpret_cast<BYTE const*>(&stored),
                sizeof(stored));

            if (setResult != ERROR_SUCCESS)
            {
                errorMessage = FormatSystemError(setResult);
                return false;
            }

            return true;
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to set the default MIDI 1.0 port naming.")

        errorMessage = L"An unexpected error occurred.";

        return false;
    }

    _Use_decl_annotations_
    bool RestartService(std::wstring& errorMessage) noexcept
    {
        errorMessage.clear();

        try
        {
            wil::unique_schandle manager{ ::OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT) };

            if (!manager)
            {
                errorMessage = FormatSystemError(::GetLastError());
                return false;
            }

            wil::unique_schandle service{
                ::OpenServiceW(manager.get(), MidiServiceName, SERVICE_QUERY_STATUS | SERVICE_START | SERVICE_STOP) };

            if (!service)
            {
                errorMessage = FormatSystemError(::GetLastError());
                return false;
            }

            SERVICE_STATUS_PROCESS status{};
            DWORD bytesNeeded{ 0 };

            if (::QueryServiceStatusEx(
                service.get(),
                SC_STATUS_PROCESS_INFO,
                reinterpret_cast<LPBYTE>(&status),
                sizeof(status),
                &bytesNeeded) &&
                status.dwCurrentState != SERVICE_STOPPED)
            {
                SERVICE_STATUS stopStatus{};

                if (!::ControlService(service.get(), SERVICE_CONTROL_STOP, &stopStatus))
                {
                    auto const error = ::GetLastError();

                    if (error != ERROR_SERVICE_NOT_ACTIVE)
                    {
                        errorMessage = FormatSystemError(error);
                        return false;
                    }
                }

                if (!WaitForServiceState(service.get(), SERVICE_STOPPED, std::chrono::seconds{ 30 }))
                {
                    errorMessage = L"The MIDI service did not stop in time.";
                    return false;
                }
            }

            if (!::StartServiceW(service.get(), 0, nullptr))
            {
                auto const error = ::GetLastError();

                if (error != ERROR_SERVICE_ALREADY_RUNNING)
                {
                    errorMessage = FormatSystemError(error);
                    return false;
                }
            }

            if (!WaitForServiceState(service.get(), SERVICE_RUNNING, std::chrono::seconds{ 30 }))
            {
                errorMessage = L"The MIDI service did not start in time.";
                return false;
            }

            return true;
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to restart the MIDI service.")

        errorMessage = L"An unexpected error occurred.";

        return false;
    }
}
