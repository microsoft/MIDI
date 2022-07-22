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
        /// rather than return the session with a complete device graph
        /// </summary>
        public bool DeferDeviceEnumeration { get; set; } = false;
    }
}
