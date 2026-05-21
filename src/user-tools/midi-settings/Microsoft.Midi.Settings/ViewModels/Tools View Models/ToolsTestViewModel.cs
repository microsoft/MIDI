// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using CommunityToolkit.Mvvm.ComponentModel;
using Microsoft.Midi.Settings.Contracts.Services;
using Microsoft.Midi.Settings.Controls;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Midi.Settings.ViewModels
{
    public partial class ToolsTestViewModel : ObservableRecipient
    {

        [ObservableProperty]
        MidiEndpointWrapper? selectedEndpoint;

        [ObservableProperty]
        MidiGroupForDisplay? selectedGroup;

        public ObservableCollection<MidiEndpointWrapper> MidiEndpoints { get; } = [];


        public ToolsTestViewModel()
        {
            //_loggingService.LogInfo($"Enter");

        }
    }
}
