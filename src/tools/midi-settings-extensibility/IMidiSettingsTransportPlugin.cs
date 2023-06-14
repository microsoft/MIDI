// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services Settings app and should be used
// via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;



namespace Microsoft.Midi.Settings.Extensibility
{
    public interface IMidiSettingsTransportPlugin
    {
        // TODO: Expose a MidiTransportInformation object from the SDK 

        Microsoft.Devices.Midi2.MidiTransportInformation TransportInformation { get; }

        // TODO: return a winui object
        Windows.UI.Xaml.FrameworkElement GetTransportSettingsPanel();


        // TODO: Have this take an endpoint info object and settings and return a winui object
        Windows.UI.Xaml.FrameworkElement GetEndpointViewSettingsPanel(string settings);
        Windows.UI.Xaml.FrameworkElement GetEndpointCreateSettingsPanel(string settings);
        Windows.UI.Xaml.FrameworkElement GetEndpointUpdateSettingsPanel(string settings);
    }
}
