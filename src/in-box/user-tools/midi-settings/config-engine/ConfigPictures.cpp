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

#include "ConfigPictures.h"
#include "ConfigJson.h"
#include "ConfigKeys.h"
#include "ConfigWrite.h"

namespace midisettings::configengine
{
    namespace
    {
        // The picture chooser stores a picture as "ep-" and its own name. A picture that comes
        // back from a package already carries the prefix, so it's taken off again, or the same
        // picture would come back as "ep-ep-..." and never be found to be identical.
        constexpr std::wstring_view StoredPrefix{ L"ep-" };

        std::wstring LeafFor(_In_ std::wstring const& name)
        {
            if (name.size() > StoredPrefix.size() && SameText(std::wstring_view{ name }.substr(0, StoredPrefix.size()), StoredPrefix))
            {
                return name.substr(StoredPrefix.size());
            }

            return name;
        }

        std::wstring RenamedTo(_In_ PictureRenames const& renames, _In_ std::wstring const& name)
        {
            for (auto const& rename : renames)
            {
                if (SameText(rename.first, name))
                {
                    return rename.second;
                }
            }

            return {};
        }

        void RenameImage(_In_ json::JsonObject const& owner, _In_ PictureRenames const& renames)
        {
            if (owner == nullptr)
            {
                return;
            }

            auto const image = jsonutil::StringValue(owner, keys::Image);

            if (image.empty())
            {
                return;
            }

            auto const renamed = RenamedTo(renames, image);

            if (!renamed.empty() && renamed != image)
            {
                owner.SetNamedValue(winrt::hstring{ keys::Image }, json::JsonValue::CreateStringValue(renamed));
            }
        }
    }

    _Use_decl_annotations_
    StoredPictures StorePictures(
        ConfigEnvironment const& environment,
        PackageContents const& contents,
        std::vector<std::wstring> const& names) noexcept
    {
        StoredPictures result{};

        try
        {
            if (names.empty())
            {
                return result;
            }

            if (environment.TemporaryFolder.empty() || !environment.StorePicture)
            {
                result.Failed = static_cast<uint32_t>(names.size());
                return result;
            }

            auto const workFolder = environment.TemporaryFolder + L"midisettings-" + jsonutil::GuidTextWithoutBraces(jsonutil::NewGuidText(false)) + L"\\";

            if (!::CreateDirectoryW(workFolder.c_str(), nullptr))
            {
                result.Failed = static_cast<uint32_t>(names.size());
                return result;
            }

            uint32_t index{ 0 };

            for (auto const& name : names)
            {
                auto const bytes = contents.FindPicture(name);

                // not in the package, so the reference is left as it was
                if (bytes == nullptr || !IsUsablePictureName(name))
                {
                    continue;
                }

                // its own folder, so two pictures whose names differ only by the prefix don't meet
                auto const folder = workFolder + std::to_wstring(index++) + L"\\";

                if (!::CreateDirectoryW(folder.c_str(), nullptr))
                {
                    result.Failed++;
                    continue;
                }

                auto const path = folder + LeafFor(name);

                std::string const data{ reinterpret_cast<char const*>(bytes->data()), bytes->size() };

                std::wstring stored{};

                if (WriteNewFile(path, data).Succeeded())
                {
                    stored = environment.StorePicture(path);
                }

                ::DeleteFileW(path.c_str());
                ::RemoveDirectoryW(folder.c_str());

                if (stored.empty())
                {
                    result.Failed++;
                    continue;
                }

                result.Stored++;
                result.Renames.emplace_back(name, stored);
            }

            ::RemoveDirectoryW(workFolder.c_str());
        }
        catch (...)
        {
            result.Failed++;
        }

        return result;
    }

    _Use_decl_annotations_
    void RenamePictures(json::JsonObject const& document, PictureRenames const& renames) noexcept
    {
        try
        {
            if (renames.empty())
            {
                return;
            }

            auto const settings = jsonutil::ObjectValue(document, keys::TransportPluginSettings);

            if (settings == nullptr)
            {
                return;
            }

            for (auto const& pair : settings)
            {
                if (pair.Value() == nullptr || pair.Value().ValueType() != json::JsonValueType::Object)
                {
                    continue;
                }

                auto const section = pair.Value().GetObject();

                if (auto const update = jsonutil::ArrayValue(section, keys::Update))
                {
                    for (auto const& value : update)
                    {
                        if (value != nullptr && value.ValueType() == json::JsonValueType::Object)
                        {
                            RenameImage(jsonutil::ObjectValue(value.GetObject(), keys::CustomProperties), renames);
                        }
                    }
                }

                if (auto const create = jsonutil::ObjectValue(section, keys::Create))
                {
                    for (auto const& entry : create)
                    {
                        if (entry.Value() == nullptr || entry.Value().ValueType() != json::JsonValueType::Object)
                        {
                            continue;
                        }

                        auto const definition = entry.Value().GetObject();

                        for (auto const end : { keys::LoopbackEndpointA, keys::LoopbackEndpointB, keys::LoopbackEndpoint })
                        {
                            RenameImage(jsonutil::ObjectValue(definition, end), renames);
                        }
                    }
                }
            }
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    std::vector<std::pair<std::wstring, std::wstring>> PictureFiles(
        ConfigEnvironment const& environment,
        std::vector<std::wstring> const& names) noexcept
    {
        std::vector<std::pair<std::wstring, std::wstring>> files{};

        try
        {
            if (!environment.PicturePath)
            {
                return files;
            }

            for (auto const& name : names)
            {
                auto const path = environment.PicturePath(name);

                if (path.empty())
                {
                    continue;
                }

                auto const attributes = ::GetFileAttributesW(path.c_str());

                if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
                {
                    files.emplace_back(name, path);
                }
            }
        }
        catch (...)
        {
            files.clear();
        }

        return files;
    }
}
