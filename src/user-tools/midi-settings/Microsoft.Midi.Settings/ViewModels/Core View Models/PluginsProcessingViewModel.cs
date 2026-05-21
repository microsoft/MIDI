// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using CommunityToolkit.Mvvm.ComponentModel;
using Microsoft.Devices.Midi2.Tools.Shared.Config;
using Microsoft.Midi.Settings.Contracts.Services;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Midi.Settings.ViewModels
{
    public class MidiServiceProcessingPluginViewModel
    {
        public Guid Id;

        public string Name;
        public string Description;

        public string ImageFileName;
        public string Author;
        //public string Version;

        public string ComImplementationPath;

        public MidiServiceProcessingPluginViewModel(
            Guid id,
            string name,
            string description)
        {
            Id = id;
            Name = name;
            Description = description;
            Author = "Microsoft Corporation"; // author is always Microsoft right now

            var path = RegistryHelper.GetImplementationPathForComClass(Id);

            if (string.IsNullOrEmpty(path))
            {
                ComImplementationPath = "(Error: Implementation path not found)";
            }
            else
            {
                ComImplementationPath = path;
            }
        }
    }

    public class PluginsProcessingViewModel : ObservableRecipient
    {
        public static IList<string> GetSearchKeywords()
        {
            // TODO: these need to be localized, so should refer to resources instead
            return new[] { "processing", "transform", "translation" };
        }

        public static string GetSearchPageTitle()
        {
            return "Processing Plugins Management";
        }
        public static string GetSearchPageDescription()
        {
            return "View all currently installed processing plugins in the MIDI Service";
        }




        public ObservableCollection<MidiServiceProcessingPluginViewModel> Plugins { get; } = [];


        private readonly ILoggingService _loggingService;
        public PluginsProcessingViewModel(ILoggingService loggingService)
        {
            _loggingService = loggingService;

            // these are hard-coded in this app because they are not yet dynamic in the service,
            // and so there's no API to call to get a list of them. 

            Plugins.Clear();

            // BS2UMP
            Plugins.Add(new MidiServiceProcessingPluginViewModel(
                Guid.Parse("{A8798C54-6066-45F0-9ADB-648BC0641ABF}"),
                "MIDI 1.0 Byte Data Format to UMP Data Format Translator",
                "Translates MIDI 1.0 data format to the new UMP data format for compatibility with MIDI 1.0 devices and APIs"
                ));

            // UMP2BS
            Plugins.Add(new MidiServiceProcessingPluginViewModel(
                Guid.Parse("{96121591-8D68-479F-9B48-2BF0B90113F7}"),
                "UMP Data Format to MIDI 1.0 Byte Data Format Translator",
                "Translates UMP data format messages to the MIDI 1.0 byte data format for compatibility with MIDI 1.0 devices and APIs"
                ));

            // Downscaler
            Plugins.Add(new MidiServiceProcessingPluginViewModel(
                Guid.Parse("{dc638b31-cf31-48ed-9e79-02740bf5d013}"),
                "UMP Message Downscaler",
                "Translates high resolution MIDI 2.0 UMP messages to their low-resolution MIDI 1.0 UMP format equivalents to support MIDI 1.0 devices using the new MIDI driver"
                ));

            // Scheduler
            Plugins.Add(new MidiServiceProcessingPluginViewModel(
                Guid.Parse("{a42cde44-7fa9-4597-a8ee-b40b96bcddb1}"),
                "Outbound Message Scheduler",
                "Provides timestamp-based outbound scheduling for messages sent using the new MIDI 2.0 SDK"
                ));

        }








    }
}
