// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace WindowsMidiServices
{
    public class MidiEndpointDeviceInfo
    {
        public string Name => BackingDeviceInformation.Name;
        public string EndpointDeviceId => BackingDeviceInformation.EndpointDeviceId;
      //  public string DeviceInstanceId => BackingDeviceInformation.DeviceInstanceId;
        public string TransportCode => BackingDeviceInformation.GetTransportSuppliedInfo().TransportCode;
        public Microsoft.Windows.Devices.Midi2.MidiEndpointNativeDataFormat NativeDataFormat => BackingDeviceInformation.GetTransportSuppliedInfo().NativeDataFormat;

        internal Microsoft.Windows.Devices.Midi2.MidiEndpointDeviceInformation BackingDeviceInformation { get; set; }

        public MidiEndpointDeviceInfo(Microsoft.Windows.Devices.Midi2.MidiEndpointDeviceInformation backingDeviceInformation)
        {
            BackingDeviceInformation = backingDeviceInformation;
        }

    }
}
