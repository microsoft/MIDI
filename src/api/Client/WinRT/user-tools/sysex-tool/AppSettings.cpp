// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "AppSettings.h"

#include <ShlObj.h>

namespace midisysextool
{
    namespace
    {
        constexpr wchar_t SettingsKeyPath[] = LR"(Software\Microsoft\Windows MIDI Services\Tools\midisysextool)";

        constexpr wchar_t ValueSingleTransferMessageCount[] = L"SingleTransferMessageCount";
        constexpr wchar_t ValueTransferSpacing[] = L"TransferSpacingMilliseconds";
        constexpr wchar_t ValueInitialReceiveBufferKb[] = L"InitialReceiveBufferKb";
        constexpr wchar_t ValueLibraryFolder[] = L"LibraryFolder";
    }

    AppSettings::AppSettings() noexcept :
        midiapp::MidiAppSettings(SettingsKeyPath)
    {
    }

    AppSettings& AppSettings::Current() noexcept
    {
        static AppSettings instance{};
        return instance;
    }

    std::wstring AppSettings::DefaultLibraryFolder() noexcept
    {
        try
        {
            wil::unique_cotaskmem_string documents{};

            if (SUCCEEDED(::SHGetKnownFolderPath(FOLDERID_Documents, KF_FLAG_DEFAULT, nullptr, &documents)))
            {
                return std::wstring{ documents.get() } + LR"(\SysEx Library)";
            }
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }

        return {};
    }

    void AppSettings::EnsureLibraryFolderExists() const noexcept
    {
        try
        {
            if (m_libraryFolder.empty())
            {
                return;
            }

            // creates intermediate folders, and returns success when it already exists
            auto const result = ::SHCreateDirectoryExW(nullptr, m_libraryFolder.c_str(), nullptr);

            if (result != ERROR_SUCCESS &&
                result != ERROR_FILE_EXISTS &&
                result != ERROR_ALREADY_EXISTS)
            {
                LOG_WIN32(result);
            }
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }
    }

    void AppSettings::Load() noexcept
    {
        LoadShared();

        m_singleTransferMessageCount = std::clamp(
            ReadDword(ValueSingleTransferMessageCount, DefaultSingleTransferMessageCount),
            MinimumSingleTransferMessageCount, MaximumSingleTransferMessageCount);

        m_transferSpacingMilliseconds = std::clamp(
            ReadDword(ValueTransferSpacing, DefaultTransferSpacingMilliseconds),
            MinimumTransferSpacingMilliseconds, MaximumTransferSpacingMilliseconds);

        m_initialReceiveBufferKb = std::clamp(
            ReadDword(ValueInitialReceiveBufferKb, DefaultInitialReceiveBufferKb),
            MinimumInitialReceiveBufferKb, MaximumInitialReceiveBufferKb);

        m_libraryFolder = ReadString(ValueLibraryFolder, DefaultLibraryFolder());
    }

    void AppSettings::SingleTransferMessageCount(uint32_t value) noexcept
    {
        m_singleTransferMessageCount = std::clamp(value,
            MinimumSingleTransferMessageCount, MaximumSingleTransferMessageCount);

        WriteDword(ValueSingleTransferMessageCount, m_singleTransferMessageCount);
    }

    void AppSettings::TransferSpacingMilliseconds(uint32_t value) noexcept
    {
        m_transferSpacingMilliseconds = std::clamp(value,
            MinimumTransferSpacingMilliseconds, MaximumTransferSpacingMilliseconds);

        WriteDword(ValueTransferSpacing, m_transferSpacingMilliseconds);
    }

    void AppSettings::InitialReceiveBufferKb(uint32_t value) noexcept
    {
        m_initialReceiveBufferKb = std::clamp(value,
            MinimumInitialReceiveBufferKb, MaximumInitialReceiveBufferKb);

        WriteDword(ValueInitialReceiveBufferKb, m_initialReceiveBufferKb);
    }

    void AppSettings::LibraryFolder(std::wstring const& value) noexcept
    {
        m_libraryFolder = value;
        WriteString(ValueLibraryFolder, value);
    }
}
