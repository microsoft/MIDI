using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Windows.Midi.Internal.Setup.Schema
{
    public struct SetupHeader
    {
        Guid Id { get; set; }
        string Name { get; set; }
        string Description { get; set; }
        string SchemaVersion { get; set; }
    }

}




