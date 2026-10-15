// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The main window's side of the Configurations window: the toolbar button, opening it on a page
// or with a package, and closing it along with the main window.

#include "pch.h"
#include "MainWindow.xaml.h"
#include "ConfigurationsWindow.xaml.h"

#include "Elevation.h"

namespace native = ::midisettings;

namespace winrt::midisettings::implementation
{
    _Use_decl_annotations_
    void MainWindow::OnConfigurationsClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        ShowConfigurations({});
    }

    _Use_decl_annotations_
    void MainWindow::ShowConfigurations(std::wstring_view const page) noexcept
    {
        try
        {
            if (m_closing)
            {
                return;
            }

            if (m_configurationsWindow == nullptr)
            {
                auto window = winrt::make_self<ConfigurationsWindow>();

                // sized and placed before the first paint, so it doesn't visibly jump
                window->RestoreWindowPlacement();
                window->ShowPage(page);

                window->SetRelaunchHandler([weak = get_weak()](std::wstring const& parameters)
                    {
                        auto strong = weak.get();

                        return strong != nullptr && strong->RelaunchElevated(parameters);
                    });

                m_configurationsWindow = window.as<midisettings::ConfigurationsWindow>();

                m_configurationsWindow.Closed([weak = get_weak()](auto&&, auto&&)
                    {
                        if (auto strong = weak.get())
                        {
                            strong->m_configurationsWindow = nullptr;
                        }
                    });

                m_configurationsWindow.Activate();

                return;
            }

            auto* const window = winrt::get_self<ConfigurationsWindow>(m_configurationsWindow);

            if (!page.empty())
            {
                window->ShowPage(page);
            }

            window->BringToFront();
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to open the Configurations window.")
    }

    _Use_decl_annotations_
    void MainWindow::OpenConfigurationPackage(std::wstring const& path) noexcept
    {
        try
        {
            ShowConfigurations(MidiSettingsConfigurationsPages[3]);

            if (m_configurationsWindow != nullptr)
            {
                winrt::get_self<ConfigurationsWindow>(m_configurationsWindow)->OpenPackage(path);
            }
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to open a configuration package.")
    }

    void MainWindow::CloseConfigurations() noexcept
    {
        // Left open, it would keep the app running with no main window to come back to.
        if (auto const window = std::exchange(m_configurationsWindow, nullptr))
        {
            try
            {
                window.Close();
            }
            MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to close the Configurations window.")
        }
    }

    void MainWindow::ApplyConfigurationsAppearance() noexcept
    {
        if (m_configurationsWindow != nullptr)
        {
            winrt::get_self<ConfigurationsWindow>(m_configurationsWindow)->ApplyAppearance();
        }
    }

    void MainWindow::ApplyConfigurationsAlwaysOnTop() noexcept
    {
        if (m_configurationsWindow != nullptr)
        {
            winrt::get_self<ConfigurationsWindow>(m_configurationsWindow)->ApplyAlwaysOnTop();
        }
    }

    _Use_decl_annotations_
    bool MainWindow::RelaunchElevated(std::wstring const& parameters) noexcept
    {
        try
        {
            // ownership goes to the elevated copy before it starts, or it finds this window
            // still up, raises it, and exits just as this one closes
            ::midiapp::SingleInstance::Release();

            if (native::TryRelaunchElevated(parameters))
            {
                Close();
                return true;
            }

            ::midiapp::SingleInstance::Reacquire();
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to relaunch with administrator rights.")

        return false;
    }
}
