// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiRuntimeUpdateUtility.h"
#include "Utilities.Update.MidiRuntimeUpdateUtility.g.cpp"

namespace winrt::Microsoft::Windows::Devices::Midi2::Utilities::Update::implementation
{

    _Use_decl_annotations_
    midi2::Utilities::Update::MidiRuntimeRelease MidiRuntimeUpdateUtility::ParseRuntimeReleaseFromJsonObject(json::JsonObject obj)
    {
        auto t = obj.GetNamedString(L"type", L"release");

        MidiRuntimeUpdateReleaseType releaseType{ MidiRuntimeUpdateReleaseType::Preview };

        if (t == L"preview")
        {
            releaseType = MidiRuntimeUpdateReleaseType::Preview;
        }
        else if (t == L"stable")
        {
            releaseType = MidiRuntimeUpdateReleaseType::Stable;
        }

        auto versionMajor = static_cast<uint16_t>(obj.GetNamedNumber(L"versionMajor", 0));
        auto versionMinor = static_cast<uint16_t>(obj.GetNamedNumber(L"versionMinor", 0));
        auto versionRevision = static_cast<uint16_t>(obj.GetNamedNumber(L"versionRevision", 0));
        auto versionBuildNumber = static_cast<uint16_t>(obj.GetNamedNumber(L"versionBuildNumber", 0));

        auto buildSource = obj.GetNamedString(L"source", L"");
        auto versionName = obj.GetNamedString(L"name", L"");
        auto versionFullString = obj.GetNamedString(L"versionFull", L"");
        auto releaseDescription = obj.GetNamedString(L"description", L"");

        auto releasePageUriString = obj.GetNamedString(L"releasePageUri", L"");
        auto directDownloadUriX64String = obj.GetNamedString(L"directDownloadUriX64", L"");
        auto directDownloadUriArm64String = obj.GetNamedString(L"directDownloadUriArm64", L"");

        foundation::Uri releasePageUri{ nullptr };
        foundation::Uri directDownloadUriX64{ nullptr };
        foundation::Uri directDownloadUriArm64{ nullptr };

        if (!releasePageUriString.empty())
        {
            releasePageUri = foundation::Uri(releasePageUriString);
        }

        if (!directDownloadUriX64String.empty())
        {
            directDownloadUriX64 = foundation::Uri(directDownloadUriX64String);
        }

        if (!directDownloadUriArm64String.empty())
        {
            directDownloadUriArm64 = foundation::Uri(directDownloadUriArm64String);
        }

        auto release = winrt::make_self<MidiRuntimeRelease>();

        release->InternalInitialize(
            releaseType,
            buildSource,
            versionName,
            releaseDescription,
            versionFullString,
            versionMajor,
            versionMinor,
            versionRevision,
            versionBuildNumber,
            releasePageUri,
            directDownloadUriX64,
            directDownloadUriArm64
        );

        return *release;
    }


    collections::IVectorView<midi2::Utilities::Update::MidiRuntimeRelease> MidiRuntimeUpdateUtility::GetAllAvailableReleases()
    {
        auto results = winrt::single_threaded_vector<midi2::Utilities::Update::MidiRuntimeRelease>();

        try
        {
            winrt::Windows::Foundation::Uri uri(L"https://aka.ms/MidiServicesLatestSdkVersionJson");
            winrt::Windows::Web::Http::HttpClient client;

            auto jsonString = client.GetStringAsync(uri).get();

            json::JsonObject jsonObject{};
            if (json::JsonObject::TryParse(jsonString, jsonObject))
            {
                // get the array of releases
                auto arr = jsonObject.GetNamedArray(L"releases");

                // loop through releases
                for (auto const& entry : arr)
                {
                    auto obj = entry.GetObject();

                    auto release = ParseRuntimeReleaseFromJsonObject(obj);

                    results.Append(release);
                }
            }
            else
            {
                // log failure
            }
        }
        catch (...)
        {
            // log failure
        }

        return results.GetView();
    }


    // Checks for any release, regardless of type or major version
    // returns null if no match
    midi2::Utilities::Update::MidiRuntimeRelease MidiRuntimeUpdateUtility::GetNewestAvailableRelease()
    {
        auto releases = GetAllAvailableReleases();

        if (releases == nullptr || releases.Size() == 0)
        {
            return nullptr;
        }

        // find one with highest version
        if (releases.Size() == 1)
        {
            return releases.GetAt(0);
        }
        
        midi2::Utilities::Update::MidiRuntimeRelease highestRelease{ nullptr };

        for (auto const& release : releases)
        {
            if (highestRelease == nullptr)
            {
                highestRelease = release;
            }
            else
            {
                if (release.VersionMajor() < highestRelease.VersionMajor())
                {
                    continue;
                }
                else if (release.VersionMajor() > highestRelease.VersionMajor())
                {
                    highestRelease = release;
                    continue;
                }

                // VersionMajor is equal beyond this point

                if (release.VersionMinor() < highestRelease.VersionMinor())
                {
                    continue;
                }
                else if (release.VersionMinor() > highestRelease.VersionMinor())
                {
                    highestRelease = release;
                    continue;
                }

                // VersionMinor is equal beyond this point

                if (release.VersionRevision() < highestRelease.VersionRevision())
                {
                    continue;
                }
                else if (release.VersionRevision() > highestRelease.VersionRevision())
                {
                    highestRelease = release;
                    continue;
                }

                // VersionRevision is equal beyond this point

                if (release.VersionBuildNumber() < highestRelease.VersionBuildNumber())
                {
                    continue;
                }
                else if (release.VersionBuildNumber() > highestRelease.VersionBuildNumber())
                {
                    highestRelease = release;
                    continue;
                }

                // else, everything is exactly the same, which is odd.
            }

        }

        return highestRelease;
    }

    // Checks for a release for the specific type (Stable, Preview)
    // returns null if no match
    _Use_decl_annotations_
    midi2::Utilities::Update::MidiRuntimeRelease MidiRuntimeUpdateUtility::GetNewestAvailableRelease(
        midi2::Utilities::Update::MidiRuntimeUpdateReleaseType releaseType)
    {
        UNREFERENCED_PARAMETER(releaseType);

        auto releases = GetAllAvailableReleases();

        // TODO

        return nullptr;
    }

    // Checks for a release for the specific type (Stable, Preview) and the specific major version.
    // Example: if the major version specified is 1, it will only return the newest release with
    // major version 1, even if there's a 2.x release.
    // returns null if no match
    _Use_decl_annotations_
    midi2::Utilities::Update::MidiRuntimeRelease MidiRuntimeUpdateUtility::GetNewestAvailableRelease(
        uint16_t specificMajorVersion, 
        midi2::Utilities::Update::MidiRuntimeUpdateReleaseType releaseType)
    {
        UNREFERENCED_PARAMETER(specificMajorVersion);
        UNREFERENCED_PARAMETER(releaseType);

        auto releases = GetAllAvailableReleases();

        // TODO

        return nullptr;
    }






}
