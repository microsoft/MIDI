// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Data;
using System.Globalization;

namespace Microsoft.Midi.Settings.Helpers;

public partial class UIntDisplayConverter : IValueConverter
{
    public UIntDisplayConverter()
    {
    }

    public object Convert(object value, Type targetType, object parameter, string language)
    {
        if (targetType == typeof(string))
        {
            if (value is UInt64 bigInt64)
            {
                return bigInt64.ToString("N0");
            }
            else if (value is UInt32 bigInt32)
            {
                return bigInt32.ToString("N0");
            }
            else if (value is UInt16 bigInt16)
            {
                return bigInt16.ToString("N0");
            }

            //throw new ArgumentException("Invalid parameter type. Expected UInt64.");

            return "(error)";
        }

        return value;
    }

    public object ConvertBack(object value, Type targetType, object parameter, string language)
    {
        throw new NotImplementedException();
    }
}
