using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Data;

namespace Microsoft.Midi.Settings.Helpers;

public partial class BooleanToEmojiCheckConverter : IValueConverter
{
    public BooleanToEmojiCheckConverter()
    {
    }

    public object Convert(object value, Type targetType, object parameter, string language)
    {
        if (value is bool val)
        {
            if (val)
            {
                return "☑️";
            }

            return "❌";
        }

        throw new ArgumentException("BooleanToVisibilityConverter object must be a bool");
    }

    public object? ConvertBack(object value, Type targetType, object parameter, string language)
    {
        return null;
    }
}