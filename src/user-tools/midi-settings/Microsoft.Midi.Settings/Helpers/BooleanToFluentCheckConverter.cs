using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Data;

namespace Microsoft.Midi.Settings.Helpers;

public partial class BooleanToFluentCheckConverter : IValueConverter
{
    public BooleanToFluentCheckConverter()
    {
    }

    public object Convert(object value, Type targetType, object parameter, string language)
    {
        if (value is bool val)
        {
            if (val)
            {
                return "\uf16c";
            }

            return "\uf16b";
        }

        throw new ArgumentException("BooleanToFluentCheckConverter object must be a bool");
    }

    public object? ConvertBack(object value, Type targetType, object parameter, string language)
    {
        return null;
    }
}