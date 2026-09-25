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

        // The editor is one at a time, so this is one window rather than a list.
        midiglass::EditorWindow g_editorWindow{ nullptr };

        xaml::Window g_libraryWindow{ nullptr };

        bool SamePath(_In_ std::wstring_view left, _In_ std::wstring_view right) noexcept
        {
            return ::CompareStringOrdinal(
                left.data(), static_cast<int32_t>(left.size()),
                right.data(), static_cast<int32_t>(right.size()),
                TRUE) == CSTR_EQUAL;
        }
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

            if (g_editorWindow != nullptr)
            {
                auto* const existing = winrt::get_self<EditorWindow>(g_editorWindow);

                if (existing != nullptr && SamePath(existing->LayoutFilePath(), filePath))
                {
                    g_editorWindow.Activate();
                    return;
                }

                // A different layout. The open one closes, which saves whatever was pending.
                g_editorWindow.Close();
                g_editorWindow = nullptr;
            }

            auto window = winrt::make_self<EditorWindow>();

            if (!window->LoadLayout(filePath))
            {
                return;
            }

            g_editorWindow = window.as<midiglass::EditorWindow>();

            g_editorWindow.Closed([](auto&& sender, auto&&)
                {
                    if (g_editorWindow != nullptr &&
                        sender.template try_as<midiglass::EditorWindow>() == g_editorWindow)
                    {
                        g_editorWindow = nullptr;
                    }
                });

            g_editorWindow.Activate();
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
