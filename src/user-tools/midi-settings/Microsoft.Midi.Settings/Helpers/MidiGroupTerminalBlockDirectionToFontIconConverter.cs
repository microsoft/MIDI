using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Data;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Midi.Settings.Helpers
{
    public partial class MidiGroupTerminalBlockDirectionToFontIconConverter : IValueConverter
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
                        return "\ue898";

                    case MidiGroupTerminalBlockDirection.BlockOutput:
                        return "\ue896";

                    case MidiGroupTerminalBlockDirection.Bidirectional:
                        return "\ue8ab";

                    default:
                        return "\ue711";
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
