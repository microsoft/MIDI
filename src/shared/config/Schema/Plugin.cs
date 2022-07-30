using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Windows.Midi.Internal.Config.Schema
{
    public class Plugin
    {
        public Guid Id { get; set; }

        // just the filename in the trusted plugins subfolder. Can't load from anywhere else
        public string FileName { get; set; }

        // TODO: Any other metadata we need? Version info? something else?

    }
}
