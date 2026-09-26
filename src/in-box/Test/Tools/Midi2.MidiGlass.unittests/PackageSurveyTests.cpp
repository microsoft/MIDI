// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Real files in a temporary folder, because the whole point of the survey is what is on disk
// beside the layout. A made up document would prove nothing.

#include "PackageSurveyTests.h"

#include "LayoutPackage.h"
#include "LayoutSerializer.h"

#include <filesystem>
#include <fstream>
#include <chrono>
#include <string>
#include <vector>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace
{
    std::filesystem::path g_folder{};

    std::wstring WriteFile(_In_ std::wstring const& name, _In_ size_t bytes)
    {
        auto const path = g_folder / name;

        std::ofstream file{ path, std::ios::binary | std::ios::trunc };

        std::vector<char> block(bytes, 'x');

        file.write(block.data(), static_cast<std::streamsize>(block.size()));
        file.close();

        return path.wstring();
    }

    // A layout naming the pictures given, written where the survey will look for it.
    std::wstring WriteLayout(_In_ std::vector<std::wstring> const& pictureNames)
    {
        glass::LayoutDocument document{};

        document.Name = L"Survey";

        glass::Page page{};
        page.Id = L"page";

        for (auto const& name : pictureNames)
        {
            glass::Control control{};

            control.Id = L"image" + std::to_wstring(page.Controls.size());
            control.Kind = glass::ControlKind::Image;
            control.Image.FileName = name;

            page.Controls.push_back(std::move(control));
        }

        document.Pages.push_back(std::move(page));

        auto const path = (g_folder / L"Survey.midilayout.json").wstring();
        auto const json = glass::WriteLayoutToJson(document);

        std::ofstream file{ path, std::ios::binary | std::ios::trunc };

        // Enough for the reader, which only cares that the text parses.
        std::string narrow{};

        for (auto const character : json)
        {
            narrow.push_back(static_cast<char>(character));
        }

        file.write(narrow.data(), static_cast<std::streamsize>(narrow.size()));
        file.close();

        return path;
    }
}

bool PackageSurveyTests::Setup()
{
    std::error_code ignored{};

    g_folder = std::filesystem::temp_directory_path(ignored) /
        (L"glass-survey-" + std::to_wstring(
            std::chrono::steady_clock::now().time_since_epoch().count()));

    std::filesystem::create_directories(g_folder, ignored);

    return true;
}

bool PackageSurveyTests::Cleanup()
{
    std::error_code ignored{};

    std::filesystem::remove_all(g_folder, ignored);

    return true;
}

void PackageSurveyTests::ALayoutOnItsOwnIsOneSmallFile()
{
    auto const layout = WriteLayout({});
    auto const survey = glass::SurveyLayoutPackage(layout);

    VERIFY_ARE_EQUAL(1u, survey.FileCount);
    VERIFY_IS_TRUE(survey.TotalBytes > 0);
    VERIFY_IS_FALSE(survey.TooBig);

    // Nowhere near worth warning about, which is the answer for almost every layout.
    VERIFY_IS_TRUE(survey.TotalBytes < glass::LargePackageBytes);
}

void PackageSurveyTests::APictureBesideTheLayoutIsCounted()
{
    WriteFile(L"clip.mp4", 40000);

    auto const layout = WriteLayout({ L"clip.mp4" });
    auto const survey = glass::SurveyLayoutPackage(layout);

    VERIFY_ARE_EQUAL(2u, survey.FileCount);
    VERIFY_IS_TRUE(survey.TotalBytes > 40000);
}

void PackageSurveyTests::TheBiggestFileIsNamed()
{
    WriteFile(L"small.png", 1000);
    WriteFile(L"big.mp4", 90000);

    auto const layout = WriteLayout({ L"small.png", L"big.mp4" });
    auto const survey = glass::SurveyLayoutPackage(layout);

    VERIFY_ARE_EQUAL(3u, survey.FileCount);

    // The one worth naming to a person is the one that is actually costing them the space.
    VERIFY_ARE_EQUAL(std::wstring{ L"big.mp4" }, survey.LargestName);
    VERIFY_ARE_EQUAL(uint64_t{ 90000 }, survey.LargestBytes);
}

void PackageSurveyTests::APictureThatIsNotThereIsNotCounted()
{
    auto const layout = WriteLayout({ L"gone.mp4" });
    auto const survey = glass::SurveyLayoutPackage(layout);

    // The writer leaves a broken reference out rather than refusing, so the survey has to agree
    // or the customer is warned about space that will never be used.
    VERIFY_ARE_EQUAL(1u, survey.FileCount);
}

void PackageSurveyTests::APictureNamedAsAPathIsNotCounted()
{
    WriteFile(L"clip.mp4", 5000);

    // A layout is untrusted input. A name that is a path never travels, so it never counts.
    auto const layout = WriteLayout({ L"..\\clip.mp4" });
    auto const survey = glass::SurveyLayoutPackage(layout);

    VERIFY_ARE_EQUAL(1u, survey.FileCount);
}

void PackageSurveyTests::AMissingLayoutSurveysAsNothing()
{
    auto const survey = glass::SurveyLayoutPackage(
        (g_folder / L"never written.midilayout.json").wstring());

    VERIFY_ARE_EQUAL(0u, survey.FileCount);
    VERIFY_ARE_EQUAL(uint64_t{ 0 }, survey.TotalBytes);
    VERIFY_IS_FALSE(survey.TooBig);
}

void PackageSurveyTests::VideoIsCountedSeparately()
{
    WriteFile(L"photo.png", 2000);
    WriteFile(L"clip.mp4", 70000);

    auto const layout = WriteLayout({ L"photo.png", L"clip.mp4" });
    auto const survey = glass::SurveyLayoutPackage(layout);

    VERIFY_ARE_EQUAL(1u, survey.VideoCount);
    VERIFY_ARE_EQUAL(uint64_t{ 70000 }, survey.VideoBytes);

    // What the customer is offered as the small backup: everything except the clip.
    VERIFY_ARE_EQUAL(survey.TotalBytes - 70000, survey.BytesWithoutVideo());
    VERIFY_IS_TRUE(survey.BytesWithoutVideo() > 2000);
}

void PackageSurveyTests::AStillPictureIsNotCountedAsVideo()
{
    WriteFile(L"photo.png", 90000);

    auto const layout = WriteLayout({ L"photo.png" });
    auto const survey = glass::SurveyLayoutPackage(layout);

    // Leaving the video out is no help here, so the choice must not be offered.
    VERIFY_ARE_EQUAL(0u, survey.VideoCount);
    VERIFY_ARE_EQUAL(uint64_t{ 0 }, survey.VideoBytes);
    VERIFY_ARE_EQUAL(survey.TotalBytes, survey.BytesWithoutVideo());
}

void PackageSurveyTests::ABackupWithoutVideoLeavesTheClipOut()
{
    WriteFile(L"photo.png", 2000);
    WriteFile(L"clip.mp4", 70000);

    auto const layout = WriteLayout({ L"photo.png", L"clip.mp4" });

    auto const withEverything = glass::WriteLayoutPackage(
        layout, (g_folder / L"all.zip").wstring(), true);

    auto const withoutVideo = glass::WriteLayoutPackage(
        layout, (g_folder / L"small.zip").wstring(), false);

    VERIFY_IS_TRUE(withEverything.Succeeded);
    VERIFY_IS_TRUE(withoutVideo.Succeeded);

    // Layout, photo and clip against layout and photo.
    VERIFY_ARE_EQUAL(3u, withEverything.FileCount);
    VERIFY_ARE_EQUAL(2u, withoutVideo.FileCount);

    std::error_code ignored{};

    VERIFY_IS_TRUE(
        std::filesystem::file_size(withoutVideo.Path, ignored) <
        std::filesystem::file_size(withEverything.Path, ignored));
}

void PackageSurveyTests::ABackupWithoutVideoStillCarriesTheStills()
{
    WriteFile(L"photo.png", 2000);
    WriteFile(L"clip.mp4", 70000);

    auto const layout = WriteLayout({ L"photo.png", L"clip.mp4" });

    auto const written = glass::WriteLayoutPackage(
        layout, (g_folder / L"small.zip").wstring(), false);

    VERIFY_IS_TRUE(written.Succeeded);

    // Unpacked somewhere clean, the layout and the still are both there and the clip is not.
    // That is the whole promise of the small backup: on this PC the clip never moved.
    auto const target = g_folder / L"unpacked";

    std::error_code ignored{};
    std::filesystem::create_directories(target, ignored);

    auto const read = glass::ReadLayoutPackage(written.Path, target.wstring());

    VERIFY_IS_TRUE(read.Succeeded);
    VERIFY_IS_TRUE(std::filesystem::is_regular_file(target / L"photo.png", ignored));
    VERIFY_IS_FALSE(std::filesystem::is_regular_file(target / L"clip.mp4", ignored));
}
