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
    namespace
    {
        // The Win32 dialog rather than Windows.Storage.Pickers, for the same reason the picture
        // picker uses it: the WinRT picker's completion never resumes when it is raised over an
        // open ContentDialog, and this one always is.
        std::wstring ShowSaveDialog(
            _In_ HWND const owner,
            _In_ std::wstring const& suggestedFileName,
            _In_ std::wstring const& startFolder) noexcept
        {
            try
            {
                winrt::com_ptr<IFileSaveDialog> dialog{};

                if (FAILED(::CoCreateInstance(
                    CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(dialog.put()))))
                {
                    return {};
                }

                COMDLG_FILTERSPEC const filters[]
                {
                    { L"MIDI configuration", L"*.midiconfig.json" },
                    { L"All files", L"*.*" },
                };

                LOG_IF_FAILED(dialog->SetFileTypes(ARRAYSIZE(filters), filters));
                LOG_IF_FAILED(dialog->SetOptions(FOS_OVERWRITEPROMPT | FOS_PATHMUSTEXIST | FOS_FORCEFILESYSTEM));

                if (!suggestedFileName.empty())
                {
                    LOG_IF_FAILED(dialog->SetFileName(suggestedFileName.c_str()));
                }

                if (!startFolder.empty())
                {
                    winrt::com_ptr<IShellItem> folder{};

                    if (SUCCEEDED(::SHCreateItemFromParsingName(
                        startFolder.c_str(), nullptr, IID_PPV_ARGS(folder.put()))))
                    {
                        LOG_IF_FAILED(dialog->SetDefaultFolder(folder.get()));
                    }
                }

                // canceling is reported as a failure hresult, so this is not logged as an error
                if (FAILED(dialog->Show(owner)))
                {
                    return {};
                }

                winrt::com_ptr<IShellItem> item{};

                if (FAILED(dialog->GetResult(item.put())))
                {
                    return {};
                }

                wil::unique_cotaskmem_string path{};

                if (FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, path.put())))
                {
                    return {};
                }

                return std::wstring{ path.get() };
            }
            catch (...)
            {
                return {};
            }
        }

        std::wstring ParentFolder(_In_ std::wstring const& path) noexcept
        {
            auto const separator = path.find_last_of(L"\\/");

            return separator == std::wstring::npos ? std::wstring{} : path.substr(0, separator);
        }
    }

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

            ConfigFileApplyButton().IsEnabled(elevated);
            CreateConfigButton().IsEnabled(elevated);
            NewConfigNameTextBox().IsEnabled(elevated);
            PortNamingClassicRadio().IsEnabled(elevated);
            PortNamingNewStyleRadio().IsEnabled(elevated);
            RestartServiceButton().IsEnabled(elevated);

            auto const files = native::config::EnumerateFiles();

            m_configFileChoices.Clear();

            int32_t selectedIndex{ -1 };
            int32_t index{ 0 };

            for (auto const& file : files)
            {
                m_configFileChoices.Append(winrt::make<implementation::ConfigFileChoice>(
                    file.IsCurrent ?
                        res::FormatString(L"ConfigFileCurrentFormat", winrt::hstring{ file.ConfigName }) :
                        winrt::hstring{ file.ConfigName },
                    winrt::hstring{ file.FileName }));

                if (file.IsCurrent)
                {
                    selectedIndex = index;
                }

                index++;
            }

            ConfigFileComboBox().SelectedIndex(selectedIndex);

            CopyConfigButton().IsEnabled(!native::config::CurrentFullPath().empty());

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

        // Exclusive and ASIO are in the API but not implemented, so they are not offered here.
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
    winrt::fire_and_forget MainWindow::OnApplyConfigFileClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        try
        {
            auto const selected = ConfigFileComboBox().SelectedItem()
                .try_as<midisettings::ConfigFileChoice>();

            if (selected == nullptr)
            {
                GlobalStatusText().Text(res::GetString(L"ConfigFileNoSelection"));
                co_return;
            }

            std::wstring errorMessage{};

            if (!native::config::SetCurrentFileName(std::wstring{ selected.FileName() }, errorMessage))
            {
                GlobalStatusText().Text(res::FormatString(
                    L"ConfigFileFailedFormat", winrt::hstring{ errorMessage }));
                co_return;
            }

            GlobalStatusText().Text(res::GetString(L"ConfigFileChangedRestartNeeded"));

            RefreshGlobalSettings();
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to change the configuration file.")

        co_return;
    }

    _Use_decl_annotations_
    void MainWindow::OnCreateConfigFileClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            std::wstring const configName{ NewConfigNameTextBox().Text() };

            auto const fileName = native::config::FileNameFromConfigName(configName);

            if (fileName.empty())
            {
                GlobalStatusText().Text(res::GetString(L"ConfigNameNotUsable"));
                return;
            }

            std::wstring errorMessage{};

            if (!native::config::CreateFile(configName, fileName, errorMessage))
            {
                GlobalStatusText().Text(res::FormatString(
                    L"ConfigFileFailedFormat", winrt::hstring{ errorMessage }));
                return;
            }

            // A file nobody is using helps no one, so creating one also makes it the active one.
            if (!native::config::SetCurrentFileName(fileName, errorMessage))
            {
                GlobalStatusText().Text(res::FormatString(
                    L"ConfigFileFailedFormat", winrt::hstring{ errorMessage }));
                return;
            }

            NewConfigNameTextBox().Text({});

            RefreshGlobalSettings();

            GlobalStatusText().Text(res::GetString(L"ConfigFileCreatedRestartNeeded"));
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to create the configuration file.")
    }

    _Use_decl_annotations_
    void MainWindow::OnCopyConfigFileClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            auto const source = native::config::CurrentFullPath();

            if (source.empty())
            {
                GlobalStatusText().Text(res::GetString(L"ConfigFileNoneActive"));
                return;
            }

            std::wstring suggested{ source };

            auto const separator = suggested.find_last_of(L"\\/");

            if (separator != std::wstring::npos)
            {
                suggested = suggested.substr(separator + 1);
            }

            auto const destination = ShowSaveDialog(
                WindowHandle(), suggested, native::AppSettings::Current().LastConfigCopyFolder());

            if (destination.empty())
            {
                return;
            }

            std::wstring errorMessage{};

            if (!native::config::CopyCurrentFileTo(destination, errorMessage))
            {
                GlobalStatusText().Text(res::FormatString(
                    L"ConfigFileFailedFormat", winrt::hstring{ errorMessage }));
                return;
            }

            native::AppSettings::Current().LastConfigCopyFolder(ParentFolder(destination));

            GlobalStatusText().Text(res::FormatString(
                L"ConfigFileCopiedFormat", winrt::hstring{ destination }));
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to copy the configuration file.")
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
            if (native::TryRelaunchElevated())
            {
                Close();
            }
            else
            {
                GlobalStatusText().Text(res::GetString(L"ElevationDeclined"));
            }
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to relaunch with administrator rights.")
    }
}
