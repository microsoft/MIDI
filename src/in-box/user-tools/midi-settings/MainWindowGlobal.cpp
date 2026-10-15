// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MainWindow.xaml.h"

#include "App.xaml.h"
#include "BackgroundWork.h"
#include "Elevation.h"
#include "StringResources.h"
#include "Feature_Servicing_MIDI2PortNamingRework.h"

namespace native = ::midisettings;
namespace res = ::midisettings::resources;

namespace winrt::midisettings::implementation
{
    // The whole point of the banner is that a customer who has never opened this app has no
    // configuration file, so nothing they set here would survive a restart.
    void MainWindow::ShowFirstRunInvitation() noexcept
    {
        try
        {
            FirstRunBar().IsOpen(native::config::CurrentFullPath().empty());
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to check for a configuration file.")
    }

    void MainWindow::RefreshGlobalSettings() noexcept
    {
        try
        {
            auto const elevated = ::winrt::midisettings::implementation::App::IsElevated();

            GlobalElevationBar().IsOpen(!elevated);

            PortNamingClassicRadio().IsEnabled(elevated);
            PortNamingNewStyleRadio().IsEnabled(elevated);
            RestartServiceButton().IsEnabled(elevated);

            // Only the name of the one in use. Everything else is in the Configurations window.
            auto const currentPath = native::config::CurrentFullPath();

            if (currentPath.empty())
            {
                GlobalConfigNameText().Text(res::GetString(L"GlobalConfigNone"));
                GlobalConfigInUseChip().Visibility(xaml::Visibility::Collapsed);
            }
            else
            {
                auto name = native::config::ReadConfigName(currentPath);

                if (name.empty())
                {
                    name = native::config::CurrentFileName();
                }

                GlobalConfigNameText().Text(winrt::hstring{ name });
                GlobalConfigInUseChip().Visibility(xaml::Visibility::Visible);
            }

            m_suppressPortNamingHandling = true;

            auto const naming = native::config::DefaultMidi1PortNaming();

            // A service without the naming rework would ignore the automatic choice, so do not
            // offer it there.
            PortNamingAutomaticRadio().Visibility(
                Feature_Servicing_MIDI2PortNamingRework::IsEnabled() ?
                    winrt::Microsoft::UI::Xaml::Visibility::Visible :
                    winrt::Microsoft::UI::Xaml::Visibility::Collapsed);

            PortNamingAutomaticRadio().IsChecked(naming == native::Midi1PortNaming::Automatic);
            PortNamingNewStyleRadio().IsChecked(naming == native::Midi1PortNaming::NewStyle);
            PortNamingClassicRadio().IsChecked(naming == native::Midi1PortNaming::ClassicCompatible);

            m_suppressPortNamingHandling = false;

            RefreshSynthSettings();

            GlobalStatusText().Text({});
        }
        catch (...)
        {
            m_suppressPortNamingHandling = false;

            MIDI_SETTINGS_LOG_GENERAL_EXCEPTION(L"Unable to load the global settings.");
        }
    }


    namespace
    {
        // Index order must match the ComboBoxItem order in MainWindow.xaml.
        constexpr midi2synth::MidiSynthRenderMode RenderModeByIndex[]
        {
            midi2synth::MidiSynthRenderMode::Compatible,
            midi2synth::MidiSynthRenderMode::Modern,
        };

        // Exclusive mode is in the API but not implemented, so it is not offered here.
        constexpr midi2synth::MidiSynthAudioOutputMode AudioModeByIndex[]
        {
            midi2synth::MidiSynthAudioOutputMode::WasapiShared,
            midi2synth::MidiSynthAudioOutputMode::WasapiSharedLowLatency,
        };

        constexpr midi2synth::MidiSynthBankSelectMode BankSelectByIndex[]
        {
            midi2synth::MidiSynthBankSelectMode::Automatic,
            midi2synth::MidiSynthBankSelectMode::RolandGS,
            midi2synth::MidiSynthBankSelectMode::YamahaXG,
            midi2synth::MidiSynthBankSelectMode::GeneralMidi2,
        };

        template <typename TEnum, size_t TCount>
        int32_t IndexForValue(_In_ TEnum const (&table)[TCount], _In_ TEnum value) noexcept
        {
            for (size_t i = 0; i < TCount; i++)
            {
                if (table[i] == value)
                {
                    return static_cast<int32_t>(i);
                }
            }

            return -1;
        }
    }


    void MainWindow::RefreshSynthSettings() noexcept
    {
        m_suppressSynthHandling = true;

        auto restore = wil::scope_exit([&]() { m_suppressSynthHandling = false; });

        try
        {
            auto const status = midi2synth::MidiSynthManager::IsTransportAvailable()
                ? midi2synth::MidiSynthManager::GetStatus()
                : nullptr;

            // A machine without the synthesizer transport installed is not an error. Hide the
            // controls rather than showing ones which cannot do anything.
            if (status == nullptr)
            {
                SynthEnabledToggle().Visibility(xaml::Visibility::Collapsed);
                SynthOptionsPanel().Visibility(xaml::Visibility::Collapsed);
                SynthStatusText().Text(res::GetString(L"SynthNotAvailable"));
                return;
            }

            SynthEnabledToggle().Visibility(xaml::Visibility::Visible);
            SynthOptionsPanel().Visibility(xaml::Visibility::Visible);

            auto const enabled = status.IsEnabled();

            SynthEnabledToggle().IsOn(enabled);
            SynthStatusText().Text(res::GetString(enabled ? L"SynthStateOn" : L"SynthStateOff"));

            SynthRenderModeCombo().SelectedIndex(IndexForValue(RenderModeByIndex, status.RenderMode()));
            SynthAudioModeCombo().SelectedIndex(IndexForValue(AudioModeByIndex, status.AudioOutputMode()));
            SynthBankSelectCombo().SelectedIndex(IndexForValue(BankSelectByIndex, status.BankSelectMode()));

            SynthVolumeSlider().Value(status.VolumeDecibels());
            SynthVolumeText().Text(res::FormatString(L"SynthVolumeFormat", status.VolumeDecibels()));

            SynthEffectsCheck().IsChecked(status.AreEffectsEnabled());

            auto const soundSet = midi2synth::MidiSynthManager::GetSoundSetInfo();

            SynthSoundSetText().Text(soundSet == nullptr
                ? winrt::hstring{}
                : res::FormatString(L"SynthSoundSetFormat",
                    soundSet.Name(),
                    soundSet.MelodicInstrumentCount(),
                    soundSet.DrumKits() == nullptr ? 0u : soundSet.DrumKits().Size()));
        }
        catch (...)
        {
            SynthEnabledToggle().Visibility(xaml::Visibility::Collapsed);
            SynthOptionsPanel().Visibility(xaml::Visibility::Collapsed);
            SynthStatusText().Text(res::GetString(L"SynthNotAvailable"));

            MIDI_SETTINGS_LOG_GENERAL_EXCEPTION(L"Unable to read the synthesizer settings.");
        }
    }


    // Builds the configuration from what the controls show and both sends and saves it. Every
    // handler funnels through here so a change to one control cannot drop another.
    winrt::fire_and_forget MainWindow::ApplySynthConfigAsync() noexcept
    {
        auto lifetime = get_strong();

        try
        {
            auto const status = midi2synth::MidiSynthManager::GetStatus();

            if (status == nullptr)
            {
                SynthStatusText().Text(res::GetString(L"SynthChangeFailed"));
                co_return;
            }

            midi2synth::MidiSynthConfig config{ status };

            auto const enabled = SynthEnabledToggle().IsOn();
            config.IsEnabled(enabled);

            auto const renderIndex = SynthRenderModeCombo().SelectedIndex();
            auto const audioIndex = SynthAudioModeCombo().SelectedIndex();
            auto const bankIndex = SynthBankSelectCombo().SelectedIndex();

            if (renderIndex >= 0 && renderIndex < static_cast<int32_t>(std::size(RenderModeByIndex)))
            {
                config.RenderMode(RenderModeByIndex[renderIndex]);
            }

            if (audioIndex >= 0 && audioIndex < static_cast<int32_t>(std::size(AudioModeByIndex)))
            {
                config.AudioOutputMode(AudioModeByIndex[audioIndex]);
            }

            if (bankIndex >= 0 && bankIndex < static_cast<int32_t>(std::size(BankSelectByIndex)))
            {
                config.BankSelectMode(BankSelectByIndex[bankIndex]);
            }

            config.VolumeDecibels(SynthVolumeSlider().Value());
            config.AreEffectsEnabled(SynthEffectsCheck().IsChecked().GetBoolean());

            auto const response =
                midi2config::MidiServiceTransportPluginConfigManager::SendUpdate(config);

            if (response == nullptr ||
                response.Status() != midi2config::MidiServiceConfigResponseStatus::Success)
            {
                SynthStatusText().Text(res::GetString(L"SynthChangeFailed"));
                RefreshSynthSettings();
                co_return;
            }

            auto const saveResponse =
                midi2config::MidiServiceTransportPluginConfigManager::SaveUpdate(config);

            SynthStatusText().Text((saveResponse != nullptr && saveResponse.Success())
                ? res::GetString(enabled ? L"SynthStateOn" : L"SynthStateOff")
                : res::GetString(L"SynthNotSaved"));
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to change the synthesizer setting.")

        co_return;
    }


    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnSynthEnabledToggled(
        foundation::IInspectable const&,
        xaml::RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        if (m_suppressSynthHandling)
        {
            co_return;
        }

        ApplySynthConfigAsync();

        co_return;
    }


    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnSynthOptionChanged(
        foundation::IInspectable const&,
        xaml::RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        if (m_suppressSynthHandling)
        {
            co_return;
        }

        ApplySynthConfigAsync();

        co_return;
    }


    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnSynthVolumeChanged(
        foundation::IInspectable const&,
        controls::Primitives::RangeBaseValueChangedEventArgs const& args)
    {
        auto lifetime = get_strong();

        // The caption follows the slider even while suppressed, so a refresh shows the right value.
        SynthVolumeText().Text(res::FormatString(L"SynthVolumeFormat", args.NewValue()));

        if (m_suppressSynthHandling)
        {
            co_return;
        }

        ApplySynthConfigAsync();

        co_return;
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnGlobalSettingsClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        try
        {
            RefreshGlobalSettings();

            GlobalSettingsDialog().XamlRoot(Content().XamlRoot());

            co_await GlobalSettingsDialog().ShowAsync();

            if (m_closing)
            {
                co_return;
            }

            ShowFirstRunInvitation();
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to show the global settings.")
    }

    _Use_decl_annotations_
    void MainWindow::OnManageConfigurationsClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            // The window is the place for it now, and a dialog in front of it would be in the way.
            GlobalSettingsDialog().Hide();

            ShowConfigurations({});
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to open the Configurations window.")
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnPortNamingChanged(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        try
        {
            if (m_suppressPortNamingHandling)
            {
                co_return;
            }

            auto const newStyle = PortNamingNewStyleRadio().IsChecked();
            auto const automatic = PortNamingAutomaticRadio().IsChecked();

            auto value = native::Midi1PortNaming::ClassicCompatible;

            if (newStyle && newStyle.Value())
            {
                value = native::Midi1PortNaming::NewStyle;
            }
            else if (automatic && automatic.Value())
            {
                value = native::Midi1PortNaming::Automatic;
            }

            std::wstring errorMessage{};

            if (!native::config::SetDefaultMidi1PortNaming(value, errorMessage))
            {
                GlobalStatusText().Text(res::FormatString(
                    L"PortNamingFailedFormat", winrt::hstring{ errorMessage }));

                RefreshGlobalSettings();

                co_return;
            }

            GlobalStatusText().Text(res::GetString(L"PortNamingChangedRestartNeeded"));
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to change the default MIDI 1.0 port naming.")

        co_return;
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnRestartServiceClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        try
        {
            RestartServiceButton().IsEnabled(false);

            GlobalStatusText().Text(res::GetString(L"ServiceRestarting"));

            std::wstring errorMessage{};
            bool succeeded{ false };

            co_await native::RunOnBackgroundAsync([&succeeded, &errorMessage]()
                {
                    succeeded = native::config::RestartService(errorMessage);
                });

            if (m_closing)
            {
                co_return;
            }

            RestartServiceButton().IsEnabled(::winrt::midisettings::implementation::App::IsElevated());

            GlobalStatusText().Text(succeeded ?
                res::GetString(L"ServiceRestarted") :
                res::FormatString(L"ServiceRestartFailedFormat", winrt::hstring{ errorMessage }));

            // The watchers are bound to the old service instance, so they are rebuilt rather
            // than left listening to something that is gone.
            StopWatchers();

            m_serviceAvailable = false;

            CheckServiceHealthAsync();
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to restart the MIDI service.")
    }

    _Use_decl_annotations_
    void MainWindow::OnRestartElevatedClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            // ownership goes to the elevated copy before it starts, or it finds this window
            // still up, raises it, and exits just as this one closes
            ::midiapp::SingleInstance::Release();

            if (native::TryRelaunchElevated())
            {
                Close();
            }
            else
            {
                ::midiapp::SingleInstance::Reacquire();

                GlobalStatusText().Text(res::GetString(L"ElevationDeclined"));
            }
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to relaunch with administrator rights.")
    }
}
