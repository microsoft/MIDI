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
    public partial class MidiFunctionBlockUIHintConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, string language)
        {
            if (value is null)
            {
                return string.Empty;
            }

            if (value is MidiFunctionBlockUIHint)
            {
                var val = (MidiFunctionBlockUIHint)value;

                // TODO: Localize

                switch (val)
                {
                    case MidiFunctionBlockUIHint.Sender:
                        return "Sender";

                    case MidiFunctionBlockUIHint.Receiver:
                        return "Receiver";

                    case MidiFunctionBlockUIHint.Bidirectional:
                        return "Bidirectional";

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
            throw new NotImplementedException();
        }
    }

}
