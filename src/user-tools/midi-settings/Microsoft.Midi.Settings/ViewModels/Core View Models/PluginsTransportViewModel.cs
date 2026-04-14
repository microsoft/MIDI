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
using Microsoft.Devices.Midi2.Tools.Shared.Config;
using Microsoft.Midi.Settings.Contracts.Services;
using Microsoft.Midi.Settings.Contracts.ViewModels;

namespace Microsoft.Midi.Settings.ViewModels
{
    public class MidiServiceTransportPluginViewModel
    {
        public Guid Id;
        public string TransportCode;

        public string Name;
        public string Description;

        public string ImageFileName;
        public string Author;
        public string Version;

        public string ComImplementationPath;
    }

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
            return "View all currently installed transport plugins in the MIDI Service";
        }


        public ObservableCollection<MidiServiceTransportPluginViewModel> Plugins { get; } = [];

        public PluginsTransportViewModel()
        {
            App.GetService<ILoggingService>().LogInfo($"Enter");

            Plugins.Clear();

            var plugins = MidiReporting.GetInstalledTransportPlugins();

            foreach (var plugin in plugins.OrderBy(x => x.Name)) 
            {
                var vm = new MidiServiceTransportPluginViewModel();

                vm.Id = plugin.Id;
                vm.TransportCode = plugin.TransportCode;
                vm.Name = plugin.Name;
                vm.Description = plugin.Description;
                vm.Author = plugin.Author;
                vm.Version = plugin.Version;

                // Get the COM registration information
                var path = RegistryHelper.GetImplementationPathForComClass(plugin.Id);

                if (path != null)
                {
                    vm.ComImplementationPath = path;
                }
                
                Plugins.Add(vm);
            }
        }




    }
}
