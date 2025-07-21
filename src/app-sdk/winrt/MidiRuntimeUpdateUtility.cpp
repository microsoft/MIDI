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
        if (releaseB.Version().IsGreaterThan(releaseA.Version()))
        {
            return releaseB;
        }

        return releaseA;
    }

    _Use_decl_annotations_
    midi2::Utilities::Update::MidiRuntimeRelease MidiRuntimeUpdateUtility::ParseRuntimeReleaseFromJsonObject(json::JsonObject const& obj) noexcept
    {
        auto t = obj.GetNamedString(L"type", L"release");

        midi2::Utilities::RuntimeInformation::MidiRuntimeReleaseTypes releaseType{ midi2::Utilities::RuntimeInformation::MidiRuntimeReleaseTypes::Preview };

        if (t == L"preview")
        {
            releaseType = midi2::Utilities::RuntimeInformation::MidiRuntimeReleaseTypes::Preview;
        }
        else if (t == L"stable")
        {
            releaseType = midi2::Utilities::RuntimeInformation::MidiRuntimeReleaseTypes::Stable;
        }

        auto buildDate = internal::DateTimeFromISO8601(obj.GetNamedString(L"buildDate", L""));

        auto versionMajor = static_cast<uint16_t>(obj.GetNamedNumber(L"versionMajor", 0));
        auto versionMinor = static_cast<uint16_t>(obj.GetNamedNumber(L"versionMinor", 0));
        auto versionPatch = static_cast<uint16_t>(obj.GetNamedNumber(L"versionPatch", 0));
        auto versionBuildNumber = static_cast<uint16_t>(obj.GetNamedNumber(L"versionBuildNumber", 0));
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
        else
        {
            releasePageUri = foundation::Uri(L"https://aka.ms/MidiServicesLatestSdkRuntimeInstaller");
        }

        if (!directDownloadUriX64String.empty())
        {
            directDownloadUriX64 = foundation::Uri(directDownloadUriX64String);
        }
        else
        {
            directDownloadUriX64 = foundation::Uri(L"https://aka.ms/MidiServicesLatestSdkRuntimeInstaller_Directx64");
        }

        if (!directDownloadUriArm64String.empty())
        {
            directDownloadUriArm64 = foundation::Uri(directDownloadUriArm64String);
        }
        else
        {
            directDownloadUriArm64 = foundation::Uri(L"https://aka.ms/MidiServicesLatestSdkRuntimeInstaller_DirectArm64");
        }

        auto release = winrt::make_self<MidiRuntimeRelease>();

        auto version = winrt::make_self<midi2::Utilities::RuntimeInformation::implementation::MidiRuntimeVersion>();

        version->InternalInitialize(
            versionMajor,
            versionMinor,
            versionPatch,
            versionBuildNumber,
            preview,
            versionFullString
        );
        
        release->InternalInitialize(
            releaseType,
            buildSource,
            versionName,
            releaseDescription,
            buildDate,
            *version,
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
        midi2::Utilities::RuntimeInformation::MidiRuntimeReleaseTypes inScopeReleaseTypes
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
                        if (release.Version().Major() != majorVersion)
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
        catch (winrt::hresult_error const& ex)
        {
            LOG_IF_FAILED(static_cast<HRESULT>(ex.code()));

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingWideString(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Failed to get latest available updates from network.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingHResult(static_cast<HRESULT>(ex.code()), MIDI_SDK_TRACE_HRESULT_FIELD),
                TraceLoggingWideString(ex.message().c_str(), "error message")
            );
        }
        catch (...)
        {
            LOG_IF_FAILED(E_FAIL);

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingWideString(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Unexpected exception trying to get latest available updates from network.", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );
        }

        return results;

    }


    collections::IVectorView<midi2::Utilities::Update::MidiRuntimeRelease> MidiRuntimeUpdateUtility::GetAllAvailableReleases() noexcept
    {
        return InternalGetAvailableReleases(
            false, 0,
            midi2::Utilities::RuntimeInformation::MidiRuntimeReleaseTypes::Preview | midi2::Utilities::RuntimeInformation::MidiRuntimeReleaseTypes::Stable
        ).GetView();
    }


    // Checks for any release, regardless of type or major version
    // returns null if no match
    midi2::Utilities::Update::MidiRuntimeRelease MidiRuntimeUpdateUtility::GetHighestAvailableRelease() noexcept
    {
        auto releases = InternalGetAvailableReleases(
            false, 0, 
            midi2::Utilities::RuntimeInformation::MidiRuntimeReleaseTypes::Preview | midi2::Utilities::RuntimeInformation::MidiRuntimeReleaseTypes::Stable);


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
        midi2::Utilities::RuntimeInformation::MidiRuntimeReleaseTypes inScopeReleaseTypes) noexcept
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
        midi2::Utilities::RuntimeInformation::MidiRuntimeReleaseTypes inScopeReleaseTypes) noexcept
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



    _Use_decl_annotations_
    bool MidiRuntimeUpdateUtility::IsReleaseNewerThanInstalled(
        midi2::Utilities::Update::MidiRuntimeRelease release,
        bool ignoreReleaseType) noexcept
    {
        // TODO

        UNREFERENCED_PARAMETER(release);
        UNREFERENCED_PARAMETER(ignoreReleaseType);

        // TEMP
        return true;
    }

    _Use_decl_annotations_
    midi2::Utilities::RuntimeInformation::MidiRuntimeArchitecture MidiRuntimeUpdateUtility::InstalledRuntimeArchitecture()
    {
        // TODO
        return  midi2::Utilities::RuntimeInformation::MidiRuntimeArchitecture::x64;
    }



}
