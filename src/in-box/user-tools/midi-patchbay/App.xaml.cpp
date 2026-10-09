// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "App.xaml.h"
#include "LibraryWindow.xaml.h"
#include "MainWindow.xaml.h"

#include "AppSettings.h"

using namespace winrt::Microsoft::UI::Xaml;

namespace winrt::midipatchbay::implementation
{
    ::midipatchbay::CommandLineOptions App::s_startupOptions{};

    namespace
    {
        // Strong references, released when a window closes. The library owns the patches and
        // the routing, so an editor is only a view of one patch.
        std::vector<midipatchbay::MainWindow> g_editorWindows{};

        midipatchbay::LibraryWindow g_libraryWindow{ nullptr };

        // Each new editor is nudged down and across from the last, so a second one does not
        // land exactly on top of the first and look like nothing happened.
        constexpr int32_t EditorCascadeStep = 28;
    }

    _Use_decl_annotations_
    void App::OpenEditorWindow(std::wstring const& patchKey)
    {
        try
        {
            if (patchKey.empty())
            {
                return;
            }

            for (auto const& existing : g_editorWindows)
            {
                auto* const implementation = winrt::get_self<MainWindow>(existing);

                if (implementation != nullptr && implementation->PatchKey() == patchKey)
                {
                    existing.Activate();
                    return;
                }
            }

            auto window = winrt::make_self<MainWindow>();

            if (!window->OpenPatch(patchKey))
            {
                return;
            }

            auto const cascade = static_cast<int32_t>(g_editorWindows.size()) * EditorCascadeStep;

            // Sized and positioned before the first paint, so it does not visibly jump.
            window->RestoreWindowPlacement(cascade);

            auto projected = window.as<midipatchbay::MainWindow>();

            g_editorWindows.push_back(projected);

            // Weak, so the window's own event does not keep the window alive.
            projected.Closed([weak = winrt::make_weak(projected)](auto&&, auto&&)
                {
                    if (auto const closed = weak.get())
                    {
                        g_editorWindows.erase(
                            std::remove(g_editorWindows.begin(), g_editorWindows.end(), closed),
                            g_editorWindows.end());
                    }
                });

            projected.Activate();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to open the patch.")
    }

    void App::ActivateLibraryWindow()
    {
        try
        {
            if (g_libraryWindow != nullptr)
            {
                winrt::get_self<LibraryWindow>(g_libraryWindow)->BringForward();
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to bring the library forward.")
    }

    void App::ApplyAppearanceToEditors()
    {
        try
        {
            // A copy, so a window closing part way through cannot change the list underneath.
            auto const editors = g_editorWindows;

            for (auto const& editor : editors)
            {
                winrt::get_self<MainWindow>(editor)->ApplyAppearance();
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to apply the appearance to the editors.")
    }

    void App::CloseAllEditors()
    {
        try
        {
            // A copy, because each close takes itself out of the list.
            auto const editors = g_editorWindows;

            for (auto const& editor : editors)
            {
                editor.Close();
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to close the editors.")
    }

    App::App()
    {
        // XAML objects must not call InitializeComponent during construction; winrt::make does it
        UnhandledException({ this, &App::OnUnhandledException });
    }

    _Use_decl_annotations_
    void App::OnUnhandledException(
        foundation::IInspectable const& sender,
        xaml::UnhandledExceptionEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);

        try
        {
            TraceLoggingWrite(
                MidiPatchbayTelemetryProvider::Provider(),
                MIDI_PATCHBAY_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_PATCHBAY_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingWideString(L"Unhandled XAML exception. Continuing.", MIDI_PATCHBAY_TRACE_MESSAGE_FIELD),
                TraceLoggingHResult(static_cast<HRESULT>(args.Exception()), MIDI_PATCHBAY_TRACE_HRESULT_FIELD),
                TraceLoggingWideString(args.Message().c_str(), MIDI_PATCHBAY_TRACE_ERROR_FIELD));

            // the app stays usable rather than terminating in front of the customer, and the
            // routes it is running stay up
            args.Handled(true);
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    void App::OnLaunched(xaml::LaunchActivatedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(args);

        try
        {
            s_startupOptions = ::midipatchbay::CommandLineOptions::ParseProcessCommandLine();

            ::midipatchbay::AppSettings::Current().Load();

            // The library is the main window. Each patch opens in an editor of its own from there.
            auto window = winrt::make_self<LibraryWindow>();

            // sized and positioned before the first paint, so it does not visibly jump
            window->RestoreWindowPlacement();

            g_libraryWindow = window.as<midipatchbay::LibraryWindow>();
            m_window = window.as<xaml::Window>();

            // Let go of it while XAML is still running, rather than in a static destructor.
            m_window.Closed([](auto&&, auto&&)
                {
                    g_libraryWindow = nullptr;
                });

            m_window.Activate();

            if (s_startupOptions.StartMinimized || ::midipatchbay::AppSettings::Current().StartMinimized())
            {
                window->MinimizeAtStartup();
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to create the main window.")
    }
}
