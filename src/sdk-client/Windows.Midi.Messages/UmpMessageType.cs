// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Windows.Midi.Helpers.Messages
{
    public enum UmpMessageType
    {
        UtilityMessage = 0x0,
        SystemMessage = 0x1,
        Midi1ChannelVoiceMessage = 0x2,
        SysExAndData64bitMessage = 0x3,
        Midi2ChannelVoiceMessage = 0x4,
        Data128bitMessage = 0x5

    }
}
