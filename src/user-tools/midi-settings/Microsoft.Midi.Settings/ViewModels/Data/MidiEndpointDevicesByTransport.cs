﻿// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using CommunityToolkit.Mvvm.ComponentModel;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Midi.Settings.ViewModels
{
    public partial class MidiEndpointDevicesByTransport : ObservableRecipient
    {
        public MidiEndpointDevicesByTransport(MidiServiceTransportPluginInfo transport)
        {
            Transport = transport;
        }

        [ObservableProperty]
        public MidiServiceTransportPluginInfo transport;

        public ObservableCollection<MidiEndpointDeviceListItem> EndpointDevices { get; } = [];

    }
}
