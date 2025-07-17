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

namespace Microsoft.Midi.Settings.ViewModels
{
    public class PluginsTransportViewModel : ObservableRecipient
    {

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
