using Microsoft.Windows.Midi.PluginModel;
using MidiService.Protocol.Midi;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace MidiService.Model
{

    // TODO: Need to have a version of this for MIDI 1.0 as well

    public class UmpEndpoint
    {
        public Guid Id { get; set; }

        /// <summary>
        /// Messages going out to the device
        /// </summary>
        public IMidiUmpMessageQueue OutgoingMessages { get; set; }
        
        
        /// <summary>
        /// Messages coming in from the device
        /// </summary>
        public IMidiUmpMessageQueue IncomingMessages { get; set; }



    }
}
