using Microsoft.UI.Xaml.Data;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Midi.Settings.Helpers
{
    public class MidiFunctionBlockMidi10Converter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, string language)
        {
            if (value is null)
            {
                return string.Empty;
            }

            if (value is MidiFunctionBlockMidi10)
            {
                var val = (MidiFunctionBlockMidi10)value;

                // TODO: Localize

                switch (val)
                {
                    case MidiFunctionBlockMidi10.Not10:
                        return "Not MIDI 1.0";

                    case MidiFunctionBlockMidi10.YesBandwidthUnrestricted:
                        return "MIDI 1.0 unrestricted bandwidth";

                    case MidiFunctionBlockMidi10.YesBandwidthRestricted:
                        return "MIDI 1.0 restricted bandwidth";

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
