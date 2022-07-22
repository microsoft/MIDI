using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Windows.Midi.Session
{
    // TODO: Do we inherit from the data class (or compose an instance of it)
    // to make it easier to keep the API separate from the serialization,
    // and also ensure the two stay in sync?

    public sealed class MidiSession
    {
        //private MidiSessionSerializableData _data;

        private string _processName;
        private int _processId;

        public string Id { get; internal set; }
        public string Name { get; set; }
        
        public DateTime CreatedTime { get; internal set; }


        // TODO: Device and endpoint lifetime / CRUD


        // TODO: Enumeration of devices and endpoints


        // TODO: Utility functions

    }
}
