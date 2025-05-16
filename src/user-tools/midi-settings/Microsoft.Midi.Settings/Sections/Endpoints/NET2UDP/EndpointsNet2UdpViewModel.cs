using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using Microsoft.Midi.Settings.Contracts.Services;
using Microsoft.Midi.Settings.Contracts.ViewModels;
using Microsoft.Midi.Settings.Models;
using Microsoft.Midi.Settings.Services;
using Microsoft.UI.Dispatching;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Input;

namespace Microsoft.Midi.Settings.ViewModels
{
    public partial class EndpointsNet2UdpViewModel : SingleTransportEndpointViewModelBase
    {
        private IMidiConfigFileService m_midiConfigFileService;

        public bool IsConfigFileActive
        {
            get { return m_midiConfigFileService.IsConfigFileActive; }
        }


        public ICommand CreateNetworkHostCommand
        {
            get; private set;
        }

        [ObservableProperty]
        public string newHostName;

        [ObservableProperty]
        public string newHostUniqueId;

        [ObservableProperty]
        public string newHostPort;

        [ObservableProperty]
        public string newHostServiceId;

        [ObservableProperty]
        public bool newHostIsPersistent;

        [ObservableProperty]
        public bool newNetworkHostSettingsAreValid;

        [ObservableProperty]
        public string validationErrorMessage;


        public EndpointsNet2UdpViewModel(INavigationService navigationService) : base("NET2UDP", navigationService)
        {




        }

    }
}
