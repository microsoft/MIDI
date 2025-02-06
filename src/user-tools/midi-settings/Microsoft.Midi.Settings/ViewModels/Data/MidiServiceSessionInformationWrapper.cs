using CommunityToolkit.Mvvm.ComponentModel;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Midi.Settings.ViewModels
{

    public partial class MidiServiceSessionConnectionInfoWrapper : ObservableRecipient
    {
        [ObservableProperty]
        public string endpointName;

        public MidiServiceSessionConnectionInfo ConnectionInfo;
    }

    public partial class MidiServiceSessionInformationWrapper : ObservableRecipient
    {
        [ObservableProperty]
        public MidiServiceSessionInfo? sessionInfo;

        public ObservableCollection<MidiServiceSessionConnectionInfoWrapper> SessionConnections { get; } = [];
    }
}
