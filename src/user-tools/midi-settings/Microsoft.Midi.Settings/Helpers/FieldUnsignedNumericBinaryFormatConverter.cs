using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Microsoft.UI.Xaml.Data;

namespace Microsoft.Midi.Settings.Helpers;
public class FieldUnsignedNumericBinaryFormatConverter : IValueConverter
{
    public object Convert(object value, Type targetType, object parameter, string language)
    {
        if (value is null)
        {
            return null;
        }

        if (parameter is null)
        {
            return null;
        }

        // Retrieve the number of digits specified in the parameter
        var digitsString = parameter as string;

        if (string.IsNullOrEmpty(digitsString))
        {
            return null;
        }

        try
        {
            int digits = int.Parse(digitsString);
            var number = (UInt32)(Int32)value;      // quirk of xaml. the uint gets converted to int before we see it

            var padded = System.Convert.ToString(number, 2).PadLeft(digits, '0');

            if (padded.Length > digits)
            {
                return padded.Substring(padded.Length - digits, digits);
            }
            else
            {
                return padded;
            }
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
