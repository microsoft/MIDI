using Microsoft.Windows.Midi.PluginModel;
using MidiService.Protocol.Midi;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace MidiService.Model
{
    public class Endpoint
    {
        public Guid Id { get; set; }

        /// <summary>
        /// Messages going out to the device
        /// </summary>
        public IMidiMessageQueue OutgoingMessages { get; set; }
        
        
        /// <summary>
        /// Messages coming in from the device
        /// </summary>
        public IMidiMessageQueue IncomingMessages { get; set; }



    }
}
