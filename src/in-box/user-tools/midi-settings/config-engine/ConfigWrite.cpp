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

// Pure: no precompiled header, so the unit tests compile this file exactly as it ships.

#include "ConfigWrite.h"
#include "ConfigJson.h"

namespace midisettings::configengine
{
    namespace
    {
        // The same patience the API's writer has with a file another program has open.
        constexpr uint32_t SharingRetryCount{ 20 };
        constexpr DWORD SharingRetryWaitMilliseconds{ 100 };

        class FileHandle
        {
        public:
            explicit FileHandle(_In_ HANDLE const handle) noexcept : m_handle(handle) {}
            ~FileHandle() { if (IsValid()) { ::CloseHandle(m_handle); } }

            FileHandle(FileHandle const&) = delete;
            FileHandle& operator=(FileHandle const&) = delete;

            bool IsValid() const noexcept { return m_handle != nullptr && m_handle != INVALID_HANDLE_VALUE; }
            HANDLE Get() const noexcept { return m_handle; }

        private:
            HANDLE m_handle{ INVALID_HANDLE_VALUE };
        };

        HANDLE OpenWithRetry(
            _In_ std::wstring const& path,
            _In_ DWORD const access,
            _In_ DWORD const disposition,
            _In_ DWORD const flags,
            _Out_ DWORD& systemError) noexcept
        {
            systemError = ERROR_SUCCESS;

            for (uint32_t attempt = 0; attempt < SharingRetryCount; attempt++)
            {
                auto const file = ::CreateFileW(
                    path.c_str(),
                    access,
                    FILE_SHARE_READ,
                    nullptr,
                    disposition,
                    FILE_ATTRIBUTE_NORMAL | flags,
                    nullptr);

                if (file != INVALID_HANDLE_VALUE)
                {
                    systemError = ERROR_SUCCESS;
                    return file;
                }

                systemError = ::GetLastError();

                if (systemError != ERROR_SHARING_VIOLATION && systemError != ERROR_LOCK_VIOLATION)
                {
                    break;
                }

                ::Sleep(SharingRetryWaitMilliseconds);
            }

            return INVALID_HANDLE_VALUE;
        }

        ConfigResult ReadFromHandle(
            _In_ HANDLE const file,
            _In_ uint64_t const maximumBytes,
            _Out_ std::string& bytes)
        {
            bytes.clear();

            ConfigResult result{};
            LARGE_INTEGER size{};

            if (!::GetFileSizeEx(file, &size))
            {
                result.Error = ConfigError::ReadFailed;
                result.SystemError = ::GetLastError();
                return result;
            }

            if (size.QuadPart < 0 || static_cast<uint64_t>(size.QuadPart) > maximumBytes)
            {
                result.Error = ConfigError::TooLarge;
                return result;
            }

            bytes.resize(static_cast<size_t>(size.QuadPart));

            size_t total{ 0 };

            while (total < bytes.size())
            {
                DWORD read{ 0 };

                if (!::ReadFile(file, bytes.data() + total, static_cast<DWORD>(bytes.size() - total), &read, nullptr))
                {
                    result.Error = ConfigError::ReadFailed;
                    result.SystemError = ::GetLastError();
                    bytes.clear();
                    return result;
                }

                if (read == 0)
                {
                    break;
                }

                total += read;
            }

            bytes.resize(total);

            return result;
        }

        // In place, never through a temporary file and a rename: the folder lets people write
        // files there but not always delete them. That's also how the API saves.
        bool WriteToHandle(
            _In_ HANDLE const file,
            _In_ std::string const& bytes,
            _Out_ DWORD& systemError) noexcept
        {
            systemError = ERROR_SUCCESS;

            LARGE_INTEGER const start{};
            DWORD written{ 0 };

            if (!::SetFilePointerEx(file, start, nullptr, FILE_BEGIN) ||
                !::WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) ||
                written != bytes.size() ||
                !::SetEndOfFile(file))
            {
                systemError = ::GetLastError();
                return false;
            }

            return true;
        }

        // Anyone can add files to these folders, and this runs as an administrator, so a link
        // planted under a file's name must not send the write somewhere else.
        bool IsPlainFile(_In_ HANDLE const file) noexcept
        {
            BY_HANDLE_FILE_INFORMATION information{};

            return ::GetFileInformationByHandle(file, &information) &&
                (information.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0 &&
                information.nNumberOfLinks == 1;
        }
    }

    _Use_decl_annotations_
    ConfigResult ReadWholeFile(
        std::wstring const& path,
        uint64_t const maximumBytes,
        std::string& bytes) noexcept
    {
        bytes.clear();

        ConfigResult result{};

        try
        {
            DWORD systemError{ ERROR_SUCCESS };

            FileHandle file{ OpenWithRetry(path, GENERIC_READ, OPEN_EXISTING, 0, systemError) };

            if (!file.IsValid())
            {
                result.Error = systemError == ERROR_FILE_NOT_FOUND || systemError == ERROR_PATH_NOT_FOUND ?
                    ConfigError::NotFound :
                    ConfigError::ReadFailed;
                result.SystemError = systemError;
                return result;
            }

            return ReadFromHandle(file.Get(), maximumBytes, bytes);
        }
        catch (...)
        {
            bytes.clear();
            result.Error = ConfigError::Unexpected;
            return result;
        }
    }

    _Use_decl_annotations_
    WriteInPlaceResult WriteInPlace(
        std::wstring const& path,
        std::string const& bytes,
        std::string const* expectedBytes) noexcept
    {
        WriteInPlaceResult result{};

        try
        {
            DWORD systemError{ ERROR_SUCCESS };

            // Held open from the read to the write, so nothing can save in between.
            FileHandle file{ OpenWithRetry(
                path,
                GENERIC_READ | GENERIC_WRITE,
                OPEN_ALWAYS,
                FILE_FLAG_OPEN_REPARSE_POINT,
                systemError) };

            if (!file.IsValid())
            {
                result.Error = ConfigError::WriteFailed;
                result.SystemError = systemError;
                return result;
            }

            if (!IsPlainFile(file.Get()))
            {
                result.Error = ConfigError::LinkRefused;
                result.SystemError = ERROR_CANT_ACCESS_FILE;
                return result;
            }

            std::string existing{};

            auto const read = ReadFromHandle(file.Get(), MaximumConfigBytes, existing);

            if (!read.Succeeded())
            {
                result.Error = read.Error;
                result.SystemError = read.SystemError;
                return result;
            }

            if (expectedBytes != nullptr && existing != *expectedBytes)
            {
                result.Error = ConfigError::ChangedSincePlanned;
                return result;
            }

            if (existing == bytes)
            {
                result.Unchanged = true;
                return result;
            }

            if (!WriteToHandle(file.Get(), bytes, systemError))
            {
                result.Error = ConfigError::WriteFailed;
                result.SystemError = systemError;
            }

            return result;
        }
        catch (...)
        {
            result.Error = ConfigError::Unexpected;
            return result;
        }
    }

    _Use_decl_annotations_
    ConfigResult WriteNewFile(
        std::wstring const& path,
        std::string const& bytes) noexcept
    {
        ConfigResult result{};

        try
        {
            DWORD systemError{ ERROR_SUCCESS };

            FileHandle file{ OpenWithRetry(path, GENERIC_WRITE, CREATE_NEW, 0, systemError) };

            if (!file.IsValid())
            {
                result.Error = systemError == ERROR_FILE_EXISTS || systemError == ERROR_ALREADY_EXISTS ?
                    ConfigError::NameTaken :
                    ConfigError::WriteFailed;
                result.SystemError = systemError;
                return result;
            }

            if (!WriteToHandle(file.Get(), bytes, systemError))
            {
                result.Error = ConfigError::WriteFailed;
                result.SystemError = systemError;
            }

            return result;
        }
        catch (...)
        {
            result.Error = ConfigError::Unexpected;
            return result;
        }
    }

    _Use_decl_annotations_
    bool LooksLikeConfiguration(std::string_view const bytes) noexcept
    {
        try
        {
            auto const document = jsonutil::ParseObject(bytes);

            return document != nullptr &&
                jsonutil::ObjectValue(document, L"endpointTransportPluginSettings") != nullptr;
        }
        catch (...)
        {
            return false;
        }
    }

    _Use_decl_annotations_
    ConfigResult DeleteFileAt(std::wstring const& path) noexcept
    {
        ConfigResult result{};

        if (!::DeleteFileW(path.c_str()))
        {
            auto const error = ::GetLastError();

            if (error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND)
            {
                result.Error = ConfigError::DeleteFailed;
                result.SystemError = error;
            }
        }

        return result;
    }
}
