// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using CommunityToolkit.Mvvm.ComponentModel;
using Microsoft.Midi.Settings.Contracts.ViewModels;

namespace Microsoft.Midi.Settings.ViewModels
{
    public class PluginsTransportViewModel : ObservableRecipient, ISettingsSearchTarget
    {
        public static IList<string> GetSearchKeywords()
        {
            // TODO: these need to be localized, so should refer to resources instead
            return new[] { "transports" };
        }

        public static string GetSearchPageTitle()
        {
            return "Transport Management";
        }
        public static string GetSearchPageDescription()
        {
            return "View all currently installed transports in the MIDI Service";
        }


        public ObservableCollection<MidiServiceTransportPluginInfo> Plugins { get; } = [];

        public PluginsTransportViewModel()
        {
            Plugins.Clear();

            var plugins = MidiReporting.GetInstalledTransportPlugins();

            foreach (var plugin in plugins.OrderBy(x => x.Name)) 
            {
                Plugins.Add(plugin);
            }

        }
    }
}
