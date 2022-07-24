using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace MidiSetup.Schema
{
    public struct Header
    {
        Guid Id { get; set; }
        string Name { get; set; }
        string Description { get; set; }
        string SchemaVersion { get; set; }
    }

}




