// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Windows.Midi.Session
{
    public sealed class MidiSessionCreateOptions
    {
        /// <summary>
        /// For capturing statistics
        /// </summary>
        public MidiSessionLogLevel LogLevel { get; set; } = MidiSessionLogLevel.None;

        ///// <summary>
        ///// Apps typically should not disable message processor plugins, but it's an option.
        ///// </summary>
        //public bool MessageProcessorsEnabled { get; set; } = true;

        /// <summary>
        /// Set to true if you want to defer device enumeration to an explicit call later, 
        /// rather than return the session with a complete device graph. Also a good idea
        /// to do this when you open multiple concurrent sessions from a single app. In that
        /// case, you only want to enumerate devices once to help limit server traffic.
        /// </summary>
        public bool SkipDeviceEnumeration { get; set; } = false;
    }
}
