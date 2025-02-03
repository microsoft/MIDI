using Microsoft.UI.Xaml.Data;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Midi.Settings.Helpers
{
    public partial class MidiGroupTerminalBlockDirectionConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, string language)
        {
            if (value is null)
            {
                return string.Empty;
            }

            if (value is MidiGroupTerminalBlockDirection)
            {
                var val = (MidiGroupTerminalBlockDirection)value;

                // TODO: Localize

                switch (val)
                {
                    case MidiGroupTerminalBlockDirection.BlockInput:
                        return "Message Destination";

                    case MidiGroupTerminalBlockDirection.BlockOutput:
                        return "Message Source";

                    case MidiGroupTerminalBlockDirection.Bidirectional:
                        return "Message Source and Destination";

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
