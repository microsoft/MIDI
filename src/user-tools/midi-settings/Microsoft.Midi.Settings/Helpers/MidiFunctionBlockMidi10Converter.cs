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

            if (value is MidiFunctionBlockRepresentsMidi10Connection)
            {
                var val = (MidiFunctionBlockRepresentsMidi10Connection)value;

                // TODO: Localize

                switch (val)
                {
                    case MidiFunctionBlockRepresentsMidi10Connection.Not10:
                        return "Not MIDI 1.0";

                    case MidiFunctionBlockRepresentsMidi10Connection.YesBandwidthUnrestricted:
                        return "MIDI 1.0 unrestricted bandwidth";

                    case MidiFunctionBlockRepresentsMidi10Connection.YesBandwidthRestricted:
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
