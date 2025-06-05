// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using Microsoft.UI.Xaml.Data;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Midi.Settings.Helpers
{
    public partial class MidiEndpointDevicePurposeConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, string language)
        {
            if (value is null)
            {
                return string.Empty;
            }

            if (value is MidiEndpointDevicePurpose)
            {
                var val = (MidiEndpointDevicePurpose)value;

                // TODO: Localize

                switch (val)
                {
                    case MidiEndpointDevicePurpose.DiagnosticPing:
                        return "Internal endpoint for the service ping feature";

                    case MidiEndpointDevicePurpose.DiagnosticLoopback:
                        return "Loopback endpoint for testing and development";

                    case MidiEndpointDevicePurpose.InBoxGeneralMidiSynth:
                        return "In-box General MIDI 1.0 Synth";

                    case MidiEndpointDevicePurpose.VirtualDeviceResponder:
                        return "The private device-side connection of an app-to-app MIDI setup";

                    case MidiEndpointDevicePurpose.NormalMessageEndpoint:
                        return "Normal message endpoint";

                    default:
                        return "Unknown";
                }
            }
            else
            {
                return string.Empty;
            }

        }

        public object ConvertBack(object value, Type targetType, object parameter, string language)
        {
            throw new NotImplementedException();
        }
    }

}
