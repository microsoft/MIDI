using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace MidiService.Data
{
    public struct MidiSessionSerializableData
    {
        public Guid Id;
        public string Name;

        public string ProcessName;
        public int ProcessId;
        public DateTime CreatedTime;

        public Guid ClientId;

        // todo: log level

    }
}
