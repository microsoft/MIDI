// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "App.xaml.h"
#include "MainWindow.xaml.h"
#include "RuntimeWindow.xaml.h"
#include "EditorWindow.xaml.h"

#include "AppSettings.h"
#include "StringResources.h"
#include "CommandLine.h"
#include "OutputRouter.h"

using namespace winrt::Microsoft::UI::Xaml;

namespace winrt::midiglass::implementation
{
    namespace
    {
        // Strong references, released when a window closes. A runtime window is not owned by the
        // library window: closing the library must not silently stop a layout somebody is
        // playing through.
        std::vector<midiglass::RuntimeWindow> g_runtimeWindows{};

        // Every editor carries its own controller, document and undo stack, and its player owns
        // its endpoints under a key built from the file path, so two designers open at once are
        // independent. Opening a second layout used to close the first, which lost the place
        // somebody was working from.
        std::vector<midiglass::EditorWindow> g_editorWindows{};

        xaml::Window g_libraryWindow{ nullptr };

        bool SamePath(_In_ std::wstring_view left, _In_ std::wstring_view right) noexcept
        {
            return ::CompareStringOrdinal(
                left.data(), static_cast<int32_t>(left.size()),
                right.data(), static_cast<int32_t>(right.size()),
                TRUE) == CSTR_EQUAL;
        }

        // Each new designer is nudged down and across from the last, so a second one does not
        // land exactly on top of the first and look like nothing happened.
        constexpr int32_t EditorCascadeStep = 28;
    }

    void App::ActivateLibraryWindow()
    {
        try
        {
            if (g_libraryWindow != nullptr)
            {
                g_libraryWindow.Activate();
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to bring the library forward.")
    }

    _Use_decl_annotations_
    void App::OpenEditorWindow(std::wstring const& filePath)
    {
        try
        {
            if (filePath.empty())
            {
                return;
            }

            for (auto const& existing : g_editorWindows)
            {
                auto* const implementation = winrt::get_self<EditorWindow>(existing);

                if (implementation != nullptr && SamePath(implementation->LayoutFilePath(), filePath))
                {
                    existing.Activate();
                    return;
                }
            }

            auto window = winrt::make_self<EditorWindow>();

            if (!window->LoadLayout(filePath))
            {
                return;
            }

            auto const cascade = static_cast<int32_t>(g_editorWindows.size()) * EditorCascadeStep;

            // Sized and positioned before the first paint, so it does not visibly jump.
            window->RestoreWindowPlacement(cascade);

            auto projected = window.as<midiglass::EditorWindow>();

            g_editorWindows.push_back(projected);

            projected.Closed([projected](auto&&, auto&&)
                {
                    g_editorWindows.erase(
                        std::remove(g_editorWindows.begin(), g_editorWindows.end(), projected),
                        g_editorWindows.end());
                });

            projected.Activate();
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to open the editor.")
    }

    _Use_decl_annotations_
    void App::OpenRuntimeWindow(std::wstring const& filePath)
    {
        try
        {
            if (filePath.empty())
            {
                return;
            }

            for (auto const& existing : g_runtimeWindows)
            {
                auto* const implementation = winrt::get_self<RuntimeWindow>(existing);

                if (implementation != nullptr && SamePath(implementation->LayoutFilePath(), filePath))
                {
                    existing.Activate();
                    return;
                }
            }

            auto window = winrt::make_self<RuntimeWindow>();

            if (!window->LoadLayout(filePath))
            {
                return;
            }

            auto projected = window.as<midiglass::RuntimeWindow>();

            g_runtimeWindows.push_back(projected);

            projected.Closed([projected](auto&&, auto&&)
                {
                    g_runtimeWindows.erase(
                        std::remove(g_runtimeWindows.begin(), g_runtimeWindows.end(), projected),
                        g_runtimeWindows.end());
                });

            projected.Activate();
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to open a runtime window.")
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
                MidiGlassTelemetryProvider::Provider(),
                MIDI_GLASS_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_GLASS_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingWideString(L"Unhandled XAML exception. Continuing.", MIDI_GLASS_TRACE_MESSAGE_FIELD),
                TraceLoggingHResult(static_cast<HRESULT>(args.Exception()), MIDI_GLASS_TRACE_HRESULT_FIELD),
                TraceLoggingWideString(args.Message().c_str(), MIDI_GLASS_TRACE_ERROR_FIELD));

            // the app stays usable rather than terminating in front of the customer
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
            ::midiglass::AppSettings::Current().Load();

            auto window = winrt::make_self<MainWindow>();

            // sized and positioned before the first paint, so it does not visibly jump
            window->RestoreWindowPlacement();

            m_window = window.as<xaml::Window>();
            g_libraryWindow = m_window;
            m_window.Activate();

            // midiglass --run "<layout file>" opens a runtime window beside the library.
            auto const command = ::midiglass::PendingRunLayoutPath();

            if (!command.empty())
            {
                OpenRuntimeWindow(command);
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to create the main window.")
    }
}
