// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using CommunityToolkit.Mvvm.ComponentModel;
//using CommunityToolkit.WinUI.UI.Controls;
using Microsoft.Midi.Settings.Contracts.ViewModels;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using System.Threading.Tasks;
using Windows.Devices.Enumeration;
using Windows.Devices.Midi;


namespace Microsoft.Midi.Settings.ViewModels
{
    public class WinMMMidi1DevicesViewModel : ObservableRecipient, INavigationAware
    {
        public WinMMMidi1DevicesViewModel()
        {

        }

        public void OnNavigatedFrom()
        {
        }

        public void OnNavigatedTo(object parameter)
        {
        }
    }
}