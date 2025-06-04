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
    collections::IVectorView<midi2::Utilities::Update::MidiRuntimeRelease> MidiRuntimeUpdateUtility::GetAvailableReleases()
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

                    auto versionMajor = static_cast<uint32_t>(obj.GetNamedNumber(L"versionMajor", 0));
                    auto versionMinor = static_cast<uint32_t>(obj.GetNamedNumber(L"versionMinor", 0));
                    auto versionRevision = static_cast<uint32_t>(obj.GetNamedNumber(L"versionRevision", 0));
                    auto versionDateNumber = static_cast<uint32_t>(obj.GetNamedNumber(L"versionDateNumber", 0));
                    auto versionTimeNumber = static_cast<uint32_t>(obj.GetNamedNumber(L"versionTimeNumber", 0));

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
                        versionDateNumber,
                        versionTimeNumber,
                        releasePageUri,
                        directDownloadUriX64,
                        directDownloadUriArm64
                    );

                    results.Append(*release);
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
}
