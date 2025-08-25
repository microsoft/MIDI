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
        public bool useNewStyleWinMMPortNames;


        public ICommand SetServiceToAutoStartCommand { get; private set; }

        public ICommand SetDefaultNamingApproachCommand { get; private set; }


        private readonly IMidiServiceRegistrySettingsService _registrySettingsService;

        public GlobalMidiSettingsViewModel(
            IMidiServiceRegistrySettingsService registrySettingsService)
        {
            _registrySettingsService = registrySettingsService;

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


            SetDefaultNamingApproachCommand = new RelayCommand(() =>
            {
                if (UseNewStyleWinMMPortNames)
                {
                    bool storedUseNewStyleWinMMPortNames = _registrySettingsService.GetDefaultUseNewStyleMidi1PortNaming();

                    if (storedUseNewStyleWinMMPortNames != UseNewStyleWinMMPortNames)
                    {
                        if (UseNewStyleWinMMPortNames)
                        {
                            // TODO: This needs to change to the simplified naming form
                            _registrySettingsService.SetDefaultUseNewStyleMidi1PortNaming(Midi1PortNameSelectionProperty.PortName_UseInterfacePlusPinName);
                        }
                        else
                        {
                            _registrySettingsService.SetDefaultUseNewStyleMidi1PortNaming(Midi1PortNameSelectionProperty.PortName_UseLegacyWinMM);
                        }
                    }
                }
            });



        }
    }
}
