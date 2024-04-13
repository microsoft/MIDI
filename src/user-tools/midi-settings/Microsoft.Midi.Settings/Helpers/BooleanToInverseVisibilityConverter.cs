using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Data;

namespace Microsoft.Midi.Settings.Helpers;

public class BooleanToInverseVisibilityConverter : IValueConverter
{
    public BooleanToInverseVisibilityConverter()
    {
    }

    public object Convert(object value, Type targetType, object parameter, string language)
    {
        if (value is bool val)
        {
            if (!val)
            {
                return Visibility.Visible;
            }

            return Visibility.Collapsed;
        }

        throw new ArgumentException("BooleanToInverseVisibilityConverter object must be a bool");
    }

    public object ConvertBack(object value, Type targetType, object parameter, string language)
    {
        return null;
    }
}