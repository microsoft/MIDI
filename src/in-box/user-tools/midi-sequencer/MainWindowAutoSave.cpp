// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Save automatically, in the settings flyout (October 20, Pete). On, as it always was: a sequence
// with a file saves itself a moment after each change, and when the window closes. Off, it's saved
// only when you choose Save, and closing asks about unsaved changes.

#include "pch.h"
#include "MainWindow.xaml.h"

#include "StringResources.h"

namespace res = ::midisequencer::resources;

namespace winrt::midisequencer::implementation
{
    xaml::UIElement MainWindow::MakeAutoSaveSetting()
    {
        controls::ToggleSwitch toggle{};
        toggle.Header(winrt::box_value(res::GetString(L"AutoSaveHeader")));
        toggle.OnContent(winrt::box_value(res::GetString(L"AutoSaveOn")));
        toggle.OffContent(winrt::box_value(res::GetString(L"AutoSaveOff")));
        toggle.IsOn(seq::AppSettings::Current().AutoSave());
        toggle.Margin(xaml::Thickness{ 0, 4, 0, 0 });
        controls::ToolTipService::SetToolTip(toggle, winrt::box_value(res::GetString(L"AutoSaveHelp")));

        toggle.Toggled([weak = get_weak()](foundation::IInspectable const& sender, auto&&)
        {
            auto strong = weak.get();

            if (!strong)
            {
                return;
            }

            try
            {
                auto const on = sender.as<controls::ToggleSwitch>().IsOn();
                seq::AppSettings::Current().AutoSave(on);

                // Turned on with changes waiting: they save in a moment, as any change would.
                if (on && strong->m_dirty && strong->m_autosaveTimer != nullptr && !strong->m_path.empty() && !strong->m_readOnly)
                {
                    strong->m_autosaveTimer.Stop();
                    strong->m_autosaveTimer.Start();
                }
                else if (!on && strong->m_autosaveTimer != nullptr)
                {
                    strong->m_autosaveTimer.Stop();
                }
            }
            MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to change saving automatically.")
        });

        return toggle;
    }
}
