using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Microsoft.UI.Xaml.Data;

namespace Microsoft.Midi.Settings.Helpers;
public class FieldUnsignedNumericDecimalFormatConverter : IValueConverter
{
    public object Convert(object value, Type targetType, object parameter, string language)
    {
        if (value is null)
        {
            return null;
        }

        try
        {
            UInt32 number;

            if (value is Int32 || value is Int16 || value is char)
            {
                number = (UInt32)(Int32)value;      // quirk of xaml. the uint gets converted to int before we see it
            }
            else
            {
                number = (UInt32)value;
            }

            CultureInfo culture = string.IsNullOrWhiteSpace(language) ? CultureInfo.InvariantCulture : new CultureInfo(language);

            return string.Format(culture, "{0:N0}", number);
        }
        catch
        {
            return "Error Converting Value";
        }
    }

    public object ConvertBack(object value, Type targetType, object parameter, string language)
    {
        throw new NotImplementedException();
    }
}
