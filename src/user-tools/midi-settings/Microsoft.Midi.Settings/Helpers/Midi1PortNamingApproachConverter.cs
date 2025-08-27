// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using Microsoft.UI.Xaml.Data;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Midi.Settings.Helpers
{

    public partial class Midi1PortNamingApproachConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, string language)
        {
            if (value is null)
            {
                return string.Empty;
            }

            if (value is Midi1PortNamingApproach)
            {
                var val = (Midi1PortNamingApproach)value;

                // TODO: Localize

                switch (val)
                {
                    case Midi1PortNamingApproach.Default:
                        return "Midi1PortNamingValue_UseGlobalDefault".GetLocalized();

                    case Midi1PortNamingApproach.UseClassicCompatible:
                        return "Midi1PortNamingValue_UseClassicAPICompatible".GetLocalized();

                    case Midi1PortNamingApproach.UseNewStyle:
                        return "Midi1PortNamingValue_UseNewStyle".GetLocalized();

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
            if (value is null || value is string && string.IsNullOrEmpty((string)value))
            {
                return Midi1PortNamingApproach.Default;
            }

            if (!(value is string))
            {
                return Midi1PortNamingApproach.Default;
            }

            var valueString = (string)value;

            if (valueString == "Midi1PortNamingValue_UseGlobalDefault".GetLocalized())
            {
                return Midi1PortNamingApproach.Default;
            }

            if (valueString == "Midi1PortNamingValue_UseClassicAPICompatible".GetLocalized())
            {
                return Midi1PortNamingApproach.UseClassicCompatible;
            }

            if (valueString == "Midi1PortNamingValue_UseNewStyle".GetLocalized())
            {
                return Midi1PortNamingApproach.UseNewStyle;
            }

            return Midi1PortNamingApproach.Default;

        }
    }

}