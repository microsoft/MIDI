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

#include "ConfigShare.h"
#include "ConfigFiles.h"
#include "ConfigJson.h"
#include "ConfigPackage.h"
#include "ConfigPictures.h"
#include "ConfigWrite.h"

namespace midisettings::configengine
{
    namespace
    {
        std::wstring StemOf(_In_ std::wstring const& configFileName)
        {
            if (EndsWithText(configFileName, ConfigFileSuffix))
            {
                return configFileName.substr(0, configFileName.size() - ConfigFileSuffix.size());
            }

            return configFileName;
        }

        PortNaming ReadPortNaming(_In_ ConfigEnvironment const& environment)
        {
            return environment.ReadPortNaming ? environment.ReadPortNaming() : PortNaming::None;
        }

        // Copied beside the final name and renamed over it when it's complete, the way ZipWriter
        // saves, so a failed copy never leaves half a package where the customer would find it.
        ConfigResult CopyWholeFile(_In_ std::wstring const& source, _In_ std::wstring const& destination) noexcept
        {
            ConfigResult result{};

            try
            {
                auto const partial = destination + L".partial";

                if (!::CopyFileW(source.c_str(), partial.c_str(), FALSE))
                {
                    result.Error = ConfigError::WriteFailed;
                    result.SystemError = ::GetLastError();
                    return result;
                }

                if (!::MoveFileExW(partial.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
                {
                    result.Error = ConfigError::WriteFailed;
                    result.SystemError = ::GetLastError();

                    ::DeleteFileW(partial.c_str());
                }

                return result;
            }
            catch (...)
            {
                result.Error = ConfigError::Unexpected;
                return result;
            }
        }
    }

    _Use_decl_annotations_
    uint32_t ShareableParts(ConfigEnvironment const& environment, ConfigInventory const& inventory) noexcept
    {
        try
        {
            auto parts = inventory.PartsPresent();

            if (ReadPortNaming(environment) != PortNaming::None)
            {
                parts |= PartBit(ConfigPart::OtherSettings);
            }

            return parts;
        }
        catch (...)
        {
            return 0;
        }
    }

    _Use_decl_annotations_
    ShareResult ShareConfiguration(
        ConfigEnvironment const& environment,
        std::wstring const& configFileName,
        uint32_t const parts,
        std::wstring const& packageName,
        std::wstring const& note,
        std::wstring const& packagePath) noexcept
    {
        ShareResult result{};

        try
        {
            if (!IsUsableFileName(configFileName) || !EndsWithText(configFileName, ConfigFileSuffix) || packagePath.empty())
            {
                result.Error = ConfigError::NameNotUsable;
                return result;
            }

            std::string bytes{};

            auto const read = ReadWholeFile(environment.ConfigFolder + configFileName, MaximumConfigBytes, bytes);

            if (!read.Succeeded())
            {
                result.Error = read.Error;
                result.SystemError = read.SystemError;
                return result;
            }

            auto const document = LooksLikeConfiguration(bytes) ? jsonutil::ParseObject(bytes) : nullptr;

            if (document == nullptr)
            {
                result.Error = ConfigError::NotAConfiguration;
                return result;
            }

            auto const inventory = TakeInventory(document);
            auto const present = inventory.PartsPresent();
            auto const chosen = parts & ShareableParts(environment, inventory);

            if (chosen == 0)
            {
                result.Error = ConfigError::Empty;
                return result;
            }

            PackageSource source{};

            source.Info.Kind = PackageKind::Share;
            source.Info.ConfigName = CleanConfigName(packageName);

            if (source.Info.ConfigName.empty())
            {
                source.Info.ConfigName = inventory.ConfigName.empty() ? StemOf(configFileName) : inventory.ConfigName;
            }

            source.Info.ConfigFileName = configFileName;
            source.Info.Note = note;
            source.Info.ComputerName = environment.ComputerName;
            source.Info.Created = CurrentFileTime();
            source.Info.HasCreated = true;
            source.Info.AppVersion = environment.AppVersion;
            source.Info.Parts = chosen;

            // Everything it has is going, so the bytes go exactly as they are.
            if ((chosen & present) == present)
            {
                source.ConfigurationBytes = std::move(bytes);
            }
            else
            {
                auto const filtered = WithOnlyParts(document, chosen);

                if (filtered == nullptr)
                {
                    result.Error = ConfigError::Unexpected;
                    return result;
                }

                source.ConfigurationBytes = jsonutil::Serialize(filtered);
            }

            if ((chosen & PartBit(ConfigPart::OtherSettings)) != 0)
            {
                source.PortNamingValue = ReadPortNaming(environment);
            }

            auto const pictureNames = PicturesUsedBy(inventory, chosen);

            source.Pictures = PictureFiles(environment, pictureNames);

            result.Pictures = static_cast<uint32_t>(source.Pictures.size());
            result.PicturesMissing = static_cast<uint32_t>(pictureNames.size() - source.Pictures.size());

            auto const written = WritePackage(packagePath, source);

            result.Error = written.Error;
            result.SystemError = written.SystemError;

            return result;
        }
        catch (...)
        {
            result.Error = ConfigError::Unexpected;
            return result;
        }
    }

    _Use_decl_annotations_
    ShareResult ShareBackup(
        ConfigEnvironment const& environment,
        BackupEntry const& backup,
        std::wstring const& packagePath) noexcept
    {
        ShareResult result{};

        try
        {
            if (packagePath.empty())
            {
                result.Error = ConfigError::NameNotUsable;
                return result;
            }

            if (backup.Kind == BackupKind::Full)
            {
                PackageContents contents{};

                // It's read whole first, so a damaged backup is refused rather than passed on.
                auto const read = ReadPackage(backup.Path, contents);

                if (!read.Succeeded())
                {
                    result.Error = read.Error;
                    result.SystemError = read.SystemError;
                    return result;
                }

                auto const copied = CopyWholeFile(backup.Path, packagePath);

                result.Error = copied.Error;
                result.SystemError = copied.SystemError;
                result.Pictures = static_cast<uint32_t>(contents.Pictures.size());

                return result;
            }

            std::string bytes{};

            auto const read = ReadWholeFile(backup.Path, MaximumConfigBytes, bytes);

            if (!read.Succeeded())
            {
                result.Error = read.Error;
                result.SystemError = read.SystemError;
                return result;
            }

            if (!LooksLikeConfiguration(bytes))
            {
                result.Error = ConfigError::NotAConfiguration;
                return result;
            }

            auto const inventory = TakeInventory(std::string_view{ bytes });

            PackageSource source{};

            source.Info.Kind = PackageKind::Share;
            source.Info.ConfigName = backup.ConfigName.empty() ? StemOf(backup.ConfigFileName) : backup.ConfigName;
            source.Info.ConfigFileName = backup.ConfigFileName;
            source.Info.ComputerName = environment.ComputerName;
            source.Info.Created = CurrentFileTime();
            source.Info.HasCreated = true;
            source.Info.AppVersion = environment.AppVersion;
            source.Info.Parts = inventory.PartsPresent();
            source.ConfigurationBytes = std::move(bytes);
            source.Pictures = PictureFiles(environment, inventory.Pictures);

            result.Pictures = static_cast<uint32_t>(source.Pictures.size());
            result.PicturesMissing = static_cast<uint32_t>(inventory.Pictures.size() - source.Pictures.size());

            auto const written = WritePackage(packagePath, source);

            result.Error = written.Error;
            result.SystemError = written.SystemError;

            return result;
        }
        catch (...)
        {
            result.Error = ConfigError::Unexpected;
            return result;
        }
    }
}
