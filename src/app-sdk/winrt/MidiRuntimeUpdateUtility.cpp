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
    midi2::Utilities::Update::MidiRuntimeRelease MidiRuntimeUpdateUtility::GetHigherReleaseValue(
        midi2::Utilities::Update::MidiRuntimeRelease const& releaseA,
        midi2::Utilities::Update::MidiRuntimeRelease const& releaseB
        ) noexcept
    {
        // both are null, so just return null
        if (releaseA == nullptr && releaseB == nullptr)
        {
            return nullptr;
        }

        // only A is null, so return B
        if (releaseA == nullptr)
        {
            return releaseB;
        }
        
        // only B is null, so return A
        if (releaseB == nullptr)
        {
            return releaseA;
        }

        // nothing is null beyond this point

        if (releaseA.VersionMajor() < releaseB.VersionMajor())
        {
            return releaseB;
        }
        else if (releaseA.VersionMajor() > releaseB.VersionMajor())
        {
            return releaseA;
        }

        // VersionMajor is equal beyond this point

        if (releaseA.VersionMinor() < releaseB.VersionMinor())
        {
            return releaseB;
        }
        else if (releaseA.VersionMinor() > releaseB.VersionMinor())
        {
            return releaseA;
        }

        // VersionMinor is equal beyond this point

        if (releaseA.VersionPatch() < releaseB.VersionPatch())
        {
            return releaseB;
        }
        else if (releaseA.VersionPatch() > releaseB.VersionPatch())
        {
            return releaseA;
        }

        // else, everything is exactly the same, so we just return releaseA

        return releaseA;
    }


    _Use_decl_annotations_
    midi2::Utilities::Update::MidiRuntimeRelease MidiRuntimeUpdateUtility::ParseRuntimeReleaseFromJsonObject(json::JsonObject const& obj) noexcept
    {
        auto t = obj.GetNamedString(L"type", L"release");

        MidiRuntimeUpdateReleaseTypes releaseType{ MidiRuntimeUpdateReleaseTypes::Preview };

        if (t == L"preview")
        {
            releaseType = MidiRuntimeUpdateReleaseTypes::Preview;
        }
        else if (t == L"stable")
        {
            releaseType = MidiRuntimeUpdateReleaseTypes::Stable;
        }

        auto buildDateString = obj.GetNamedString(L"buildDate", L"");

        foundation::DateTime buildDate{};

        // TODO: Parse the ISO 8601 format date string into a foundation::DateTime








        auto versionMajor = static_cast<uint16_t>(obj.GetNamedNumber(L"versionMajor", 0));
        auto versionMinor = static_cast<uint16_t>(obj.GetNamedNumber(L"versionMinor", 0));
        auto versionPatch = static_cast<uint16_t>(obj.GetNamedNumber(L"versionPatch", 0));
        auto preview = obj.GetNamedString(L"preview", L"");

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
            buildDate,
            versionFullString,
            versionMajor,
            versionMinor,
            versionPatch,
            preview,
            releasePageUri,
            directDownloadUriX64,
            directDownloadUriArm64
        );

        return *release;
    }


    _Use_decl_annotations_
    collections::IVector<midi2::Utilities::Update::MidiRuntimeRelease> MidiRuntimeUpdateUtility::InternalGetAvailableReleases(
        bool restrictToMajorVersion,
        uint16_t majorVersion,
        midi2::Utilities::Update::MidiRuntimeUpdateReleaseTypes inScopeReleaseTypes
        ) noexcept
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

                    // if we're restricting to a major version, only add if the major version matches
                    if (restrictToMajorVersion)
                    {
                        if (release.VersionMajor() != majorVersion)
                        {
                            continue;
                        }
                    }

                    // only add if it's in scope
                    if ((inScopeReleaseTypes & release.Type()) != release.Type())
                    {
                        continue;
                    }


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

        return results;

    }


    collections::IVectorView<midi2::Utilities::Update::MidiRuntimeRelease> MidiRuntimeUpdateUtility::GetAllAvailableReleases() noexcept
    {
        return InternalGetAvailableReleases(
            false, 0,
            midi2::Utilities::Update::MidiRuntimeUpdateReleaseTypes::Preview | midi2::Utilities::Update::MidiRuntimeUpdateReleaseTypes::Stable
        ).GetView();
    }


    // Checks for any release, regardless of type or major version
    // returns null if no match
    midi2::Utilities::Update::MidiRuntimeRelease MidiRuntimeUpdateUtility::GetHighestAvailableRelease() noexcept
    {
        auto releases = InternalGetAvailableReleases(
            false, 0, 
            midi2::Utilities::Update::MidiRuntimeUpdateReleaseTypes::Preview | midi2::Utilities::Update::MidiRuntimeUpdateReleaseTypes::Stable);


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
            highestRelease = GetHigherReleaseValue(highestRelease, release);
        }

        return highestRelease;
    }

    // Checks for a release for the specific type (Stable, Preview)
    // returns null if no match
    _Use_decl_annotations_
    midi2::Utilities::Update::MidiRuntimeRelease MidiRuntimeUpdateUtility::GetHighestAvailableRelease(
        midi2::Utilities::Update::MidiRuntimeUpdateReleaseTypes inScopeReleaseTypes) noexcept
    {
        auto releases = InternalGetAvailableReleases(false, 0, inScopeReleaseTypes);

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
            highestRelease = GetHigherReleaseValue(highestRelease, release);
        }

        return highestRelease;
    }

    // Checks for a release for the specific type (Stable, Preview) and the specific major version.
    // Example: if the major version specified is 1, it will only return the newest release with
    // major version 1, even if there's a 2.x release.
    // returns null if no match
    _Use_decl_annotations_
    midi2::Utilities::Update::MidiRuntimeRelease MidiRuntimeUpdateUtility::GetHighestAvailableRelease(
        uint16_t specificMajorVersion, 
        midi2::Utilities::Update::MidiRuntimeUpdateReleaseTypes inScopeReleaseTypes) noexcept
    {
        auto releases = InternalGetAvailableReleases(true, specificMajorVersion, inScopeReleaseTypes);

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
            highestRelease = GetHigherReleaseValue(highestRelease, release);
        }

        return highestRelease;
    }






}
