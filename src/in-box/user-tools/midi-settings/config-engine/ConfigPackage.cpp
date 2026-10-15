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

#include "ConfigPackage.h"
#include "ConfigInventory.h"
#include "ConfigJson.h"
#include "ConfigWrite.h"

#include "ZipArchive.h"

#include <algorithm>

namespace midisettings::configengine
{
    namespace
    {
        constexpr std::wstring_view PackageEntryName{ L"package.json" };
        constexpr std::wstring_view ConfigurationEntryName{ L"configuration.json" };
        constexpr std::wstring_view SettingsEntryName{ L"settings.json" };
        constexpr std::wstring_view PicturesFolderName{ L"pictures/" };

        // A real package has a handful of entries. These are far beyond that, and small enough
        // that a hostile file can't make the app use much memory or time.
        constexpr uint32_t MaximumEntries{ 1024 };
        constexpr uint32_t MaximumPictures{ 512 };
        constexpr size_t MaximumSmallEntryBytes{ 256 * 1024 };
        constexpr size_t MaximumPictureBytes{ 16 * 1024 * 1024 };
        constexpr uint64_t MaximumTotalBytes{ 256ull * 1024 * 1024 };

        // text read out of package.json is shown, so a long string can't take over the window
        constexpr size_t MaximumTextLength{ 1024 };
        constexpr size_t MaximumNoteLength{ 2000 };

        namespace packagekeys
        {
            constexpr std::wstring_view FormatVersion{ L"formatVersion" };
            constexpr std::wstring_view Kind{ L"kind" };
            constexpr std::wstring_view ConfigName{ L"configName" };
            constexpr std::wstring_view ConfigFileName{ L"configFileName" };
            constexpr std::wstring_view Note{ L"note" };
            constexpr std::wstring_view ComputerName{ L"computerName" };
            constexpr std::wstring_view Created{ L"created" };
            constexpr std::wstring_view Reason{ L"reason" };
            constexpr std::wstring_view ReasonComputerName{ L"reasonComputerName" };
            constexpr std::wstring_view ReasonConfigName{ L"reasonConfigName" };
            constexpr std::wstring_view AppVersion{ L"appVersion" };
            constexpr std::wstring_view Parts{ L"parts" };
            constexpr std::wstring_view PortNames{ L"portNames" };
        }

        struct PartName
        {
            ConfigPart Part;
            std::wstring_view Name;
        };

        constexpr PartName PartNames[]
        {
            { ConfigPart::DeviceNames, L"deviceNames" },
            { ConfigPart::Loopbacks, L"loopbacks" },
            { ConfigPart::NetworkMidi2, L"networkMidi2" },
            { ConfigPart::RtpMidi, L"rtpMidi" },
            { ConfigPart::Bluetooth, L"bluetooth" },
            { ConfigPart::OtherSettings, L"otherSettings" },
        };

        std::wstring_view KindName(_In_ PackageKind const kind) noexcept
        {
            return kind == PackageKind::Backup ? std::wstring_view{ L"backup" } : std::wstring_view{ L"share" };
        }

        std::wstring_view ReasonName(_In_ BackupReason const reason) noexcept
        {
            switch (reason)
            {
            case BackupReason::YouBackedItUp:   return L"youBackedItUp";
            case BackupReason::BeforeImport:    return L"beforeImport";
            case BackupReason::BeforeRestore:   return L"beforeRestore";
            case BackupReason::BeforeDelete:    return L"beforeDelete";
            default:                            return {};
            }
        }

        BackupReason ReasonFromName(_In_ std::wstring_view const name) noexcept
        {
            for (auto const reason : { BackupReason::YouBackedItUp, BackupReason::BeforeImport, BackupReason::BeforeRestore, BackupReason::BeforeDelete })
            {
                if (SameText(name, ReasonName(reason)))
                {
                    return reason;
                }
            }

            return BackupReason::None;
        }

        std::wstring Limited(_In_ std::wstring text, _In_ size_t const maximumLength)
        {
            if (text.size() > maximumLength)
            {
                text.resize(maximumLength);
            }

            return text;
        }

        ConfigError ErrorForZip(_In_ midiapp::ZipStatus const status) noexcept
        {
            switch (status)
            {
            case midiapp::ZipStatus::Read:
            case midiapp::ZipStatus::Written:
                return ConfigError::None;
            case midiapp::ZipStatus::NotAZip:
                return ConfigError::NotAPackage;
            case midiapp::ZipStatus::TooManyEntries:
            case midiapp::ZipStatus::TooLarge:
                return ConfigError::TooLarge;
            case midiapp::ZipStatus::CannotOpen:
                return ConfigError::ReadFailed;
            case midiapp::ZipStatus::CannotWrite:
                return ConfigError::WriteFailed;
            default:
                return ConfigError::Damaged;
            }
        }

        std::span<uint8_t const> AsBytes(_In_ std::string const& text) noexcept
        {
            return std::span<uint8_t const>{ reinterpret_cast<uint8_t const*>(text.data()), text.size() };
        }

        std::string PackageJson(_In_ PackageSource const& source, _In_ uint32_t const pictureCount)
        {
            auto const inventory = TakeInventory(std::string_view{ source.ConfigurationBytes });
            auto const& info = source.Info;

            json::JsonObject package{};

            package.SetNamedValue(winrt::hstring{ packagekeys::FormatVersion }, json::JsonValue::CreateNumberValue(PackageFormatVersion));
            package.SetNamedValue(winrt::hstring{ packagekeys::Kind }, json::JsonValue::CreateStringValue(KindName(info.Kind)));
            package.SetNamedValue(winrt::hstring{ packagekeys::ConfigName }, json::JsonValue::CreateStringValue(info.ConfigName));

            if (!info.ConfigFileName.empty())
            {
                package.SetNamedValue(winrt::hstring{ packagekeys::ConfigFileName }, json::JsonValue::CreateStringValue(info.ConfigFileName));
            }

            if (!info.Note.empty())
            {
                package.SetNamedValue(winrt::hstring{ packagekeys::Note }, json::JsonValue::CreateStringValue(info.Note));
            }

            package.SetNamedValue(winrt::hstring{ packagekeys::ComputerName }, json::JsonValue::CreateStringValue(info.ComputerName));

            auto const created = info.HasCreated ? info.Created : CurrentFileTime();
            package.SetNamedValue(winrt::hstring{ packagekeys::Created }, json::JsonValue::CreateStringValue(FileTimeToUtcText(created)));

            if (info.Kind == PackageKind::Backup && info.Reason != BackupReason::None)
            {
                package.SetNamedValue(winrt::hstring{ packagekeys::Reason }, json::JsonValue::CreateStringValue(ReasonName(info.Reason)));

                if (!info.ReasonComputerName.empty())
                {
                    package.SetNamedValue(winrt::hstring{ packagekeys::ReasonComputerName }, json::JsonValue::CreateStringValue(info.ReasonComputerName));
                }

                if (!info.ReasonConfigName.empty())
                {
                    package.SetNamedValue(winrt::hstring{ packagekeys::ReasonConfigName }, json::JsonValue::CreateStringValue(info.ReasonConfigName));
                }
            }

            package.SetNamedValue(winrt::hstring{ packagekeys::AppVersion }, json::JsonValue::CreateStringValue(info.AppVersion));

            // Which parts are inside, with counts, so the other PC can say what's coming before
            // it reads the configuration itself.
            json::JsonObject parts{};

            auto const count = [&parts](std::wstring_view const name, size_t const value)
                {
                    parts.SetNamedValue(winrt::hstring{ name }, json::JsonValue::CreateNumberValue(static_cast<double>(value)));
                };

            auto const included = [&info](ConfigPart const part) { return (info.Parts & PartBit(part)) != 0; };

            if (included(ConfigPart::DeviceNames))
            {
                count(L"deviceNames", inventory.DeviceNames.size());
                count(L"pictures", pictureCount);
            }

            if (included(ConfigPart::Loopbacks))
            {
                count(L"loopbacks", inventory.Loopbacks.size());
            }

            if (included(ConfigPart::NetworkMidi2))
            {
                count(L"networkMidi2Hosts", inventory.HostCount(NetworkKind::NetworkMidi2));
                count(L"networkMidi2Clients", inventory.ClientCount(NetworkKind::NetworkMidi2));
            }

            if (included(ConfigPart::RtpMidi))
            {
                count(L"rtpMidiHosts", inventory.HostCount(NetworkKind::RtpMidi));
                count(L"rtpMidiClients", inventory.ClientCount(NetworkKind::RtpMidi));
            }

            if (included(ConfigPart::Bluetooth))
            {
                count(L"bluetoothDevices", inventory.BluetoothDevices.size());
            }

            if (included(ConfigPart::OtherSettings))
            {
                parts.SetNamedValue(L"synthesizer", json::JsonValue::CreateBooleanValue(inventory.HasSynthesizer));
                parts.SetNamedValue(L"portNames", json::JsonValue::CreateBooleanValue(source.PortNamingValue != PortNaming::None));
            }

            json::JsonArray includedParts{};

            for (auto const& part : PartNames)
            {
                if (included(part.Part))
                {
                    includedParts.Append(json::JsonValue::CreateStringValue(part.Name));
                }
            }

            parts.SetNamedValue(L"included", includedParts);
            package.SetNamedValue(winrt::hstring{ packagekeys::Parts }, parts);

            return jsonutil::Serialize(package);
        }

        bool ReadPackageJson(_In_ std::string_view const bytes, _Out_ PackageInfo& info)
        {
            info = {};

            auto const package = jsonutil::ParseObject(bytes);

            if (package == nullptr)
            {
                return false;
            }

            auto const version = package.TryLookup(winrt::hstring{ packagekeys::FormatVersion });

            if (version == nullptr || version.ValueType() != json::JsonValueType::Number)
            {
                return false;
            }

            auto const versionNumber = version.GetNumber();

            if (versionNumber < 1 || versionNumber > 1000000)
            {
                return false;
            }

            info.FormatVersion = static_cast<uint32_t>(versionNumber);
            info.Kind = SameText(jsonutil::StringValue(package, packagekeys::Kind), L"backup") ? PackageKind::Backup : PackageKind::Share;
            info.ConfigName = Limited(jsonutil::StringValue(package, packagekeys::ConfigName), MaximumTextLength);
            info.ConfigFileName = Limited(jsonutil::StringValue(package, packagekeys::ConfigFileName), MaximumTextLength);
            info.Note = Limited(jsonutil::StringValue(package, packagekeys::Note), MaximumNoteLength);
            info.ComputerName = Limited(jsonutil::StringValue(package, packagekeys::ComputerName), MaximumTextLength);
            info.HasCreated = TryParseUtcText(jsonutil::StringValue(package, packagekeys::Created), info.Created);
            info.Reason = ReasonFromName(jsonutil::StringValue(package, packagekeys::Reason));
            info.ReasonComputerName = Limited(jsonutil::StringValue(package, packagekeys::ReasonComputerName), MaximumTextLength);
            info.ReasonConfigName = Limited(jsonutil::StringValue(package, packagekeys::ReasonConfigName), MaximumTextLength);
            info.AppVersion = Limited(jsonutil::StringValue(package, packagekeys::AppVersion), MaximumTextLength);

            if (auto const parts = jsonutil::ObjectValue(package, packagekeys::Parts))
            {
                if (auto const included = jsonutil::ArrayValue(parts, L"included"))
                {
                    for (auto const& value : included)
                    {
                        if (value == nullptr || value.ValueType() != json::JsonValueType::String)
                        {
                            continue;
                        }

                        for (auto const& part : PartNames)
                        {
                            if (SameText(value.GetString(), part.Name))
                            {
                                info.Parts |= PartBit(part.Part);
                            }
                        }
                    }
                }
            }

            return true;
        }

        // Reads what's in the zip into contents. The reader has the zip open already.
        ConfigResult ReadFromZip(_In_ midiapp::ZipReader const& reader, _Out_ PackageContents& contents)
        {
            contents = {};

            ConfigResult result{};

            midiapp::ZipItem const* packageItem{ nullptr };
            midiapp::ZipItem const* configurationItem{ nullptr };
            midiapp::ZipItem const* settingsItem{ nullptr };
            std::vector<std::pair<std::wstring, midiapp::ZipItem const*>> pictureItems{};

            uint64_t total{ 0 };

            for (auto const& item : reader.Items())
            {
                if (item.Name == PackageEntryName)
                {
                    packageItem = &item;
                }
                else if (item.Name == ConfigurationEntryName)
                {
                    configurationItem = &item;
                }
                else if (item.Name == SettingsEntryName)
                {
                    settingsItem = &item;
                }
                else if (item.Name.size() > PicturesFolderName.size() &&
                    std::wstring_view{ item.Name }.substr(0, PicturesFolderName.size()) == PicturesFolderName)
                {
                    std::wstring const pictureName{ item.Name.substr(PicturesFolderName.size()) };

                    if (!IsUsablePictureName(pictureName) || pictureItems.size() >= MaximumPictures)
                    {
                        continue;
                    }

                    auto const duplicate = std::find_if(pictureItems.begin(), pictureItems.end(),
                        [&pictureName](auto const& existing) { return SameText(existing.first, pictureName); });

                    if (duplicate == pictureItems.end())
                    {
                        pictureItems.emplace_back(pictureName, &item);
                    }
                }
                else
                {
                    // anything else is ignored, never extracted
                    continue;
                }

                total += item.Size;
            }

            if (total > MaximumTotalBytes)
            {
                result.Error = ConfigError::TooLarge;
                return result;
            }

            if (packageItem == nullptr || configurationItem == nullptr)
            {
                result.Error = ConfigError::NotAPackage;
                return result;
            }

            std::vector<uint8_t> bytes{};

            auto status = reader.Extract(*packageItem, MaximumSmallEntryBytes, bytes);

            if (status != midiapp::ZipStatus::Read)
            {
                result.Error = ErrorForZip(status);
                return result;
            }

            if (!ReadPackageJson(std::string_view{ reinterpret_cast<char const*>(bytes.data()), bytes.size() }, contents.Info))
            {
                result.Error = ConfigError::NotAPackage;
                return result;
            }

            if (contents.Info.FormatVersion > PackageFormatVersion)
            {
                result.Error = ConfigError::NewerPackage;
                return result;
            }

            status = reader.Extract(*configurationItem, static_cast<size_t>(MaximumConfigBytes), bytes);

            if (status != midiapp::ZipStatus::Read)
            {
                result.Error = ErrorForZip(status);
                return result;
            }

            contents.ConfigurationBytes.assign(reinterpret_cast<char const*>(bytes.data()), bytes.size());

            if (!LooksLikeConfiguration(contents.ConfigurationBytes))
            {
                contents.ConfigurationBytes.clear();
                result.Error = ConfigError::NotAConfiguration;
                return result;
            }

            if (settingsItem != nullptr &&
                reader.Extract(*settingsItem, MaximumSmallEntryBytes, bytes) == midiapp::ZipStatus::Read)
            {
                auto const settings = jsonutil::ParseObject(std::string_view{ reinterpret_cast<char const*>(bytes.data()), bytes.size() });

                contents.PortNamingValue = PortNamingFromName(jsonutil::StringValue(settings, packagekeys::PortNames));
                contents.Info.HasPortNaming = contents.PortNamingValue != PortNaming::None;
            }

            for (auto const& picture : pictureItems)
            {
                std::vector<uint8_t> pictureBytes{};

                // A damaged picture is left out rather than failing the whole package, because the
                // device names it belongs to are still worth having.
                if (reader.Extract(*picture.second, MaximumPictureBytes, pictureBytes) == midiapp::ZipStatus::Read)
                {
                    contents.Pictures.emplace_back(picture.first, std::move(pictureBytes));
                }
            }

            contents.Info.PictureCount = static_cast<uint32_t>(contents.Pictures.size());

            return result;
        }

        bool StartsLikeZip(_In_ std::span<uint8_t const> const bytes) noexcept
        {
            return bytes.size() >= 4 && bytes[0] == 'P' && bytes[1] == 'K' && bytes[2] == 3 && bytes[3] == 4;
        }

        // A configuration file saved before packages existed.
        ConfigResult ReadPlainConfiguration(_In_ std::string&& bytes, _Out_ PackageContents& contents)
        {
            contents = {};

            ConfigResult result{};

            if (!LooksLikeConfiguration(bytes))
            {
                result.Error = ConfigError::NotAPackage;
                return result;
            }

            contents.IsPlainConfiguration = true;
            contents.ConfigurationBytes = std::move(bytes);

            auto const inventory = TakeInventory(std::string_view{ contents.ConfigurationBytes });

            contents.Info.Kind = PackageKind::Share;
            contents.Info.ConfigName = Limited(inventory.ConfigName, MaximumTextLength);
            contents.Info.Parts = inventory.PartsPresent();

            return result;
        }
    }

    _Use_decl_annotations_
    std::vector<uint8_t> const* PackageContents::FindPicture(std::wstring_view const name) const noexcept
    {
        for (auto const& picture : Pictures)
        {
            if (SameText(picture.first, name))
            {
                return &picture.second;
            }
        }

        return nullptr;
    }

    _Use_decl_annotations_
    bool IsUsablePictureName(std::wstring_view const name) noexcept
    {
        if (name.size() > 200 || !IsUsableFileName(name) || name.find(L"..") != std::wstring_view::npos)
        {
            return false;
        }

        for (auto const extension : { std::wstring_view{ L".png" }, std::wstring_view{ L".jpg" }, std::wstring_view{ L".jpeg" }, std::wstring_view{ L".svg" } })
        {
            if (name.size() > extension.size() && EndsWithText(name, extension))
            {
                return true;
            }
        }

        return false;
    }

    _Use_decl_annotations_
    ConfigResult WritePackage(std::wstring const& path, PackageSource const& source) noexcept
    {
        ConfigResult result{};

        try
        {
            if (!LooksLikeConfiguration(source.ConfigurationBytes))
            {
                result.Error = ConfigError::NotAConfiguration;
                return result;
            }

            // only pictures that are really there, once each
            std::vector<std::pair<std::wstring, std::wstring>> pictures{};

            for (auto const& picture : source.Pictures)
            {
                if (!IsUsablePictureName(picture.first) || picture.second.empty())
                {
                    continue;
                }

                auto const attributes = ::GetFileAttributesW(picture.second.c_str());

                if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
                {
                    continue;
                }

                auto const duplicate = std::find_if(pictures.begin(), pictures.end(),
                    [&picture](auto const& existing) { return SameText(existing.first, picture.first); });

                if (duplicate == pictures.end())
                {
                    pictures.push_back(picture);
                }
            }

            midiapp::ZipWriter writer{};

            auto status = writer.Create(std::filesystem::path{ path });

            if (status != midiapp::ZipStatus::Written)
            {
                result.Error = ConfigError::WriteFailed;
                return result;
            }

            uint32_t written{ 0 };

            // The pictures first, because one that can't be read is left out, and the count in
            // package.json has to say what's really there.
            for (auto const& picture : pictures)
            {
                auto const compression = EndsWithText(picture.first, L".svg") ?
                    midiapp::ZipCompression::Deflate :
                    midiapp::ZipCompression::Store;

                status = writer.AddFile(std::wstring{ PicturesFolderName } + picture.first, std::filesystem::path{ picture.second }, compression);

                if (status == midiapp::ZipStatus::CannotOpen || status == midiapp::ZipStatus::BadName)
                {
                    continue;
                }

                if (status != midiapp::ZipStatus::Written)
                {
                    result.Error = ErrorForZip(status);
                    return result;
                }

                written++;
            }

            auto const packageJson = PackageJson(source, written);

            status = writer.AddBytes(PackageEntryName, AsBytes(packageJson), midiapp::ZipCompression::Deflate);

            if (status == midiapp::ZipStatus::Written)
            {
                status = writer.AddBytes(ConfigurationEntryName, AsBytes(source.ConfigurationBytes), midiapp::ZipCompression::Deflate);
            }

            if (status == midiapp::ZipStatus::Written && source.PortNamingValue != PortNaming::None)
            {
                json::JsonObject settings{};
                settings.SetNamedValue(winrt::hstring{ packagekeys::PortNames }, json::JsonValue::CreateStringValue(PortNamingName(source.PortNamingValue)));

                auto const settingsJson = jsonutil::Serialize(settings);

                status = writer.AddBytes(SettingsEntryName, AsBytes(settingsJson), midiapp::ZipCompression::Deflate);
            }

            if (status == midiapp::ZipStatus::Written)
            {
                status = writer.Finish();
            }

            if (status != midiapp::ZipStatus::Written)
            {
                result.Error = ErrorForZip(status);
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
    ConfigResult ReadPackageFromBytes(std::span<uint8_t const> const bytes, PackageContents& contents) noexcept
    {
        contents = {};

        ConfigResult result{};

        try
        {
            if (!StartsLikeZip(bytes))
            {
                if (bytes.size() > MaximumConfigBytes)
                {
                    result.Error = ConfigError::TooLarge;
                    return result;
                }

                return ReadPlainConfiguration(std::string{ reinterpret_cast<char const*>(bytes.data()), bytes.size() }, contents);
            }

            midiapp::ZipReader reader{};

            auto const status = reader.Open(bytes, MaximumEntries);

            if (status != midiapp::ZipStatus::Read)
            {
                result.Error = ErrorForZip(status);
                return result;
            }

            return ReadFromZip(reader, contents);
        }
        catch (...)
        {
            contents = {};
            result.Error = ConfigError::Unexpected;
            return result;
        }
    }

    _Use_decl_annotations_
    ConfigResult ReadPackage(std::wstring const& path, PackageContents& contents) noexcept
    {
        contents = {};

        ConfigResult result{};

        try
        {
            // The first bytes decide: a zip is read through its directory, a plain configuration
            // is read whole. Neither is ever allowed to be larger than the limits above.
            std::string start{};

            {
                auto const handle = ::CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

                if (handle == INVALID_HANDLE_VALUE)
                {
                    auto const error = ::GetLastError();

                    result.Error = error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND ? ConfigError::NotFound : ConfigError::ReadFailed;
                    result.SystemError = error;
                    return result;
                }

                uint8_t header[4]{};
                DWORD read{ 0 };

                auto const readOk = ::ReadFile(handle, header, sizeof(header), &read, nullptr);

                ::CloseHandle(handle);

                if (!readOk)
                {
                    result.Error = ConfigError::ReadFailed;
                    return result;
                }

                if (read == 0)
                {
                    result.Error = ConfigError::Empty;
                    return result;
                }

                if (!StartsLikeZip(std::span<uint8_t const>{ header, read }))
                {
                    auto const plain = ReadWholeFile(path, MaximumConfigBytes, start);

                    if (!plain.Succeeded())
                    {
                        return plain;
                    }

                    return ReadPlainConfiguration(std::move(start), contents);
                }
            }

            midiapp::ZipReader reader{};

            auto const status = reader.OpenFile(std::filesystem::path{ path }, MaximumEntries);

            if (status != midiapp::ZipStatus::Read)
            {
                result.Error = ErrorForZip(status);
                return result;
            }

            return ReadFromZip(reader, contents);
        }
        catch (...)
        {
            contents = {};
            result.Error = ConfigError::Unexpected;
            return result;
        }
    }

    _Use_decl_annotations_
    ConfigResult ReadPackageInfo(std::wstring const& path, PackageInfo& info) noexcept
    {
        info = {};

        ConfigResult result{};

        try
        {
            midiapp::ZipReader reader{};

            auto status = reader.OpenFile(std::filesystem::path{ path }, MaximumEntries);

            if (status != midiapp::ZipStatus::Read)
            {
                result.Error = ErrorForZip(status);
                return result;
            }

            for (auto const& item : reader.Items())
            {
                if (item.Name != PackageEntryName)
                {
                    continue;
                }

                std::vector<uint8_t> bytes{};

                status = reader.Extract(item, MaximumSmallEntryBytes, bytes);

                if (status != midiapp::ZipStatus::Read)
                {
                    result.Error = ErrorForZip(status);
                    return result;
                }

                if (!ReadPackageJson(std::string_view{ reinterpret_cast<char const*>(bytes.data()), bytes.size() }, info))
                {
                    result.Error = ConfigError::NotAPackage;
                }

                return result;
            }

            result.Error = ConfigError::NotAPackage;
            return result;
        }
        catch (...)
        {
            info = {};
            result.Error = ConfigError::Unexpected;
            return result;
        }
    }
}
