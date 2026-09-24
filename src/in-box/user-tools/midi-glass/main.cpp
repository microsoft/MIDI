// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "App.xaml.h"

#include "CommandLine.h"
#include "LayoutStore.h"
#include "ThemeModel.h"
#include "ThumbnailLayout.h"
#include "ThumbnailRenderer.h"

namespace midiglass
{
    namespace
    {
        std::wstring g_pendingRunLayoutPath{};
    }

    std::wstring const& PendingRunLayoutPath() noexcept
    {
        return g_pendingRunLayoutPath;
    }

    _Use_decl_annotations_
    void SetPendingRunLayoutPath(std::wstring path) noexcept
    {
        g_pendingRunLayoutPath = std::move(path);
    }
}

namespace
{
    // midiglass --thumbnail <layout file> <output png> [width]
    //
    // Draws a card for a layout without starting the app. The library needs one for every layout
    // it lists, including layouts that have never been opened on this PC, so this path has to
    // work with no window, no compositor and no MIDI device.
    int RunThumbnail(_In_ std::vector<std::wstring> const& arguments)
    {
        // [0] is the executable and [1] is the switch itself, so the layout is [2].
        if (arguments.size() < 4)
        {
            return 2;
        }

        auto const width = arguments.size() > 4
            ? std::max(16, _wtoi(arguments[4].c_str()))
            : glass::LargeThumbnailWidth;

        auto const height = static_cast<int32_t>(
            std::lround(width * static_cast<double>(glass::LargeThumbnailHeight) / glass::LargeThumbnailWidth));

        auto const layout = glass::ReadLayoutFile(arguments[2]);

        if (!layout.Succeeded)
        {
            return 3;
        }

        auto const* theme = glass::FindBuiltInTheme(layout.Document.ThemeName);

        if (theme == nullptr)
        {
            theme = &glass::BuiltInThemes()[0];
        }

        auto const plan = glass::PlanThumbnail(layout.Document, *theme, width, height);

        return glass::RenderThumbnailToFile(plan, arguments[3]).Succeeded ? 0 : 4;
    }

    std::vector<std::wstring> CommandLineArguments()
    {
        std::vector<std::wstring> arguments{};

        int count{ 0 };
        auto** raw = ::CommandLineToArgvW(::GetCommandLineW(), &count);

        if (raw == nullptr)
        {
            return arguments;
        }

        for (int i = 0; i < count; ++i)
        {
            arguments.emplace_back(raw[i]);
        }

        ::LocalFree(raw);

        return arguments;
    }
}

// The XAML compiler emits its own wWinMain; we supply this one so startup stays under our
// control. The apartment must stay STA: an MTA UI thread makes UI Automation fail with
// E_UNEXPECTED and then faults, which would leave the app inaccessible to screen readers.
// The MIDI SDK's session and connection calls block on the service, so they are never made
// from this thread.
int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    winrt::init_apartment(winrt::apartment_type::single_threaded);

    auto const arguments = CommandLineArguments();

    // Before single instance, and before any window: this mode draws a file and exits, so it must
    // not hand itself off to a running copy or start a UI.
    if (arguments.size() > 1 && ::CompareStringOrdinal(
        arguments[1].c_str(), -1, L"--thumbnail", -1, TRUE) == CSTR_EQUAL)
    {
        return RunThumbnail(arguments);
    }

    // One process, however many windows. Two copies would each open their own connection to the
    // same instrument and neither would know what the other had sent, so a running layout and a
    // Panic have to mean the same thing across all of them.
    if (!::midiapp::SingleInstance::AcquireOrActivateExisting(L"Glass"))
    {
        return 0;
    }

    // midiglass --run "<layout file>" opens the layout beside the library.
    if (arguments.size() > 2 && ::CompareStringOrdinal(
        arguments[1].c_str(), -1, L"--run", -1, TRUE) == CSTR_EQUAL)
    {
        ::midiglass::SetPendingRunLayoutPath(arguments[2]);
    }

    ::winrt::Microsoft::UI::Xaml::Application::Start([](auto&&)
        {
            ::winrt::make<::winrt::midiglass::implementation::App>();
        });

    ::midiapp::SingleInstance::Release();

    return 0;
}
