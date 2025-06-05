// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

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

        throw new ArgumentException("BooleanToEmojiCheckConverter object must be a bool");
    }

    public object? ConvertBack(object value, Type targetType, object parameter, string language)
    {
        return null;
    }
}