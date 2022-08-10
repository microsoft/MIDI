// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Windows.Midi.Messages.Types.ChannelVoice
{
    public enum Midi2NoteOnOffAttributeType
    {
        NoAttributeData = 0x00,
        ManufacturerSpecific = 0x01,
        ProfileSpecific = 0x02,
        PitchSevenDotNine = 0x03
    }
}
