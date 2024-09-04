using Microsoft.UI.Xaml.Data;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Midi.Settings.Helpers
{
    public partial class MidiEndpointNativeDataFormatConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, string language)
        {
            if (value is null)
            {
                return string.Empty;
            }

            if (value is MidiEndpointNativeDataFormat)
            {
                var val = (MidiEndpointNativeDataFormat)value;

                // TODO: Localize

                switch (val)
                {
                    case MidiEndpointNativeDataFormat.UniversalMidiPacketFormat:
                        return "MIDI 2.0 Universal MIDI Packet (UMP) format";

                    case MidiEndpointNativeDataFormat.Midi1ByteFormat:
                        return "MIDI 1.0 byte format";


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
