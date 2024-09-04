using CommunityToolkit.Mvvm.ComponentModel;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Midi.Settings.ViewModels
{
    public partial class MidiServiceSessionInformationWrapper : ObservableRecipient
    {
        [ObservableProperty]
        public MidiServiceSessionInfo? sessionInfo;

        public ObservableCollection<MidiServiceSessionConnectionInfo> SessionConnections { get; } = [];
    }
}
