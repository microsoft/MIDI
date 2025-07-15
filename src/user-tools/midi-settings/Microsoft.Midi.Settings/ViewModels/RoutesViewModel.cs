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
using Microsoft.Midi.Settings.Models;
using Microsoft.Midi.Settings.ViewModels.Data;
using Microsoft.UI.Dispatching;
using Windows.Devices.Enumeration;

namespace Microsoft.Midi.Settings.ViewModels
{
    public class RoutesViewModel : ObservableRecipient, INavigationAware
    {


        public void OnNavigatedFrom()
        {
            throw new NotImplementedException();
        }

        public void OnNavigatedTo(object parameter)
        {
            throw new NotImplementedException();
        }
    }
}
