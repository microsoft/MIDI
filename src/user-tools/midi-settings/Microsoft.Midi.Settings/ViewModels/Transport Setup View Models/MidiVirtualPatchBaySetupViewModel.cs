using CommunityToolkit.Mvvm.ComponentModel;
using Microsoft.Midi.Settings.Contracts.ViewModels;
using System;
using System.Collections.Generic;
using System.Text;

namespace Microsoft.Midi.Settings.ViewModels
{
    public partial class MidiVirtualPatchBaySetupViewModel : ObservableRecipient, ISettingsSearchTarget
    {
        public static IList<string> GetSearchKeywords()
        {
            // TODO: these need to be localized, so should refer to resources instead
            return new[] { "endpoints", "ports", "route", "rules", "filter", "transform" };
        }

        public static string GetSearchPageTitle()
        {
            return "Manage Routes Between Endpoints";
        }

        public static string GetSearchPageDescription()
        {
            return "Set up routing between endpoints using a virtual patch bay and rules for filtering and transforming messages.";
        }




        public MidiVirtualPatchBaySetupViewModel()
        {

        }



    }
}
