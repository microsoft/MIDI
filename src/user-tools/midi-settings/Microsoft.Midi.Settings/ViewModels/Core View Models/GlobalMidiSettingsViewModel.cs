// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using Microsoft.Extensions.Logging.Abstractions;
using Microsoft.Midi.Settings;
using Microsoft.Midi.Settings.Contracts.Services;
using Microsoft.Midi.Settings.Contracts.ViewModels;
using Microsoft.Midi.Settings.Controls;
using Microsoft.Midi.Settings.Models;
using Microsoft.Midi.Settings.Services;
using Microsoft.UI.Dispatching;
using Microsoft.Windows.Devices.Midi2.Utilities.SysExTransfer;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Diagnostics;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Input;
using Windows.Foundation;
using Windows.Storage;
using Windows.Storage.Pickers;

namespace Microsoft.Midi.Settings.ViewModels
{
    public partial class GlobalMidiSettingsViewModel : ObservableRecipient , ISettingsSearchTarget
    {
        public static IList<string> GetSearchKeywords()
        {
            // TODO: these need to be localized, so should refer to resources instead
            return new[] { "settings", "global", "naming", "service", "auto-start", "startup", "delay" };
        }

        public static string GetSearchPageTitle()
        {
            return "Global MIDI Settings";
        }

        public static string GetSearchPageDescription()
        {
            return "Manage settings which configure the broad MIDI system on this PC.";
        }


        [ObservableProperty]
        private bool useNewStyleWinMMPortNames;

        [ObservableProperty]
        private bool useClassicCompatibleWinMMPortNames;


        public ICommand SetServiceToAutoStartCommand { get; private set; }

        public ICommand SetDefaultNamingApproachCommand { get; private set; }

        public ICommand RestartMidiServiceCommand { get; private set; }

        

        private readonly IMidiServiceRegistrySettingsService _registrySettingsService;
        private readonly IMidiConsoleToolsService _consoleToolsService;

        public GlobalMidiSettingsViewModel(
            IMidiServiceRegistrySettingsService registrySettingsService,
            IMidiConsoleToolsService consoleToolsService)
        {
            _registrySettingsService = registrySettingsService;
            _consoleToolsService = consoleToolsService;

            UseNewStyleWinMMPortNames = _registrySettingsService.GetDefaultUseNewStyleMidi1PortNaming();
            UseClassicCompatibleWinMMPortNames = !UseNewStyleWinMMPortNames;

            SetServiceToAutoStartCommand = new RelayCommand(() =>
            {
                ProcessStartInfo info = new ProcessStartInfo();
                info.FileName = "cmd.exe";
                info.UseShellExecute = true;
                info.Verb = "runas";
                info.Arguments = "/c \"midi service set-auto-start --restart=false\"";

                using (var proc = Process.Start(info))
                {
                    if (proc != null)
                    {
                        proc.WaitForExit();
                    }
                }

            });

            RestartMidiServiceCommand = new RelayCommand(() =>
            {
                _consoleToolsService.RestartMidiService();
            });

            SetDefaultNamingApproachCommand = new RelayCommand(() =>
            {
                bool storedUseNewStyleWinMMPortNames = _registrySettingsService.GetDefaultUseNewStyleMidi1PortNaming();

                if (storedUseNewStyleWinMMPortNames != UseNewStyleWinMMPortNames)
                {
                    if (UseNewStyleWinMMPortNames)
                    {
                        _registrySettingsService.SetDefaultUseNewStyleMidi1PortNaming(Midi1PortNamingApproach.UseNewStyle);
                    }
                    else
                    {
                        _registrySettingsService.SetDefaultUseNewStyleMidi1PortNaming(Midi1PortNamingApproach.UseClassicCompatible);
                    }
                }
            });



        }
    }
}
