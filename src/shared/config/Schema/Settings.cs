using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace MidiConfig.Schema
{
    public class Settings
    {
        public int QueueWordCapacityEndpointOutgoing { get; set; }
        public int QueueWordCapacityEndpointIncoming { get; set; }

        public int QueueWordCapacitySessionEndpointOutgoing { get; set; }
        public int QueueWordCapacitySessionEndpointIncoming { get; set; }

    }
}
