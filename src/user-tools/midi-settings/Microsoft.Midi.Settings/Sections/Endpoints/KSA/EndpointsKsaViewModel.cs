﻿using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using Microsoft.Midi.Settings.Contracts.Services;
using Microsoft.Midi.Settings.Contracts.ViewModels;
using Microsoft.Midi.Settings.Models;
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
    public partial class EndpointsKsaViewModel : SingleTransportEndpointViewModelBase
    {
        public EndpointsKsaViewModel(
            INavigationService navigationService,
            IMidiEndpointEnumerationService enumerationService
            ) : base("KSA", navigationService, enumerationService)
        {
        }

    }
}
