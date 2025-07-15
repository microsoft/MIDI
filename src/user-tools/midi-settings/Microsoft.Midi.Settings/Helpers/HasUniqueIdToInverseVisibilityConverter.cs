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

public partial class HasUniqueIdToInverseVisibilityConverter : IValueConverter
{
    public HasUniqueIdToInverseVisibilityConverter()
    {
    }

    public object Convert(object value, Type targetType, object parameter, string language)
    {
        if (value is string id)
        {
            if (string.IsNullOrEmpty(id))
            {
                return Visibility.Visible;
            }

            return Visibility.Collapsed;
        }

        throw new ArgumentException("HasUniqueIdToInverseVisibilityConverter object must be a string");
    }

    public object? ConvertBack(object value, Type targetType, object parameter, string language)
    {
        return null;
    }
}