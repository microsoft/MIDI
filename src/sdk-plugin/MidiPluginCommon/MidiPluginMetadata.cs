using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Windows.Midi.PluginModel.Plugin
{
    public class MidiPluginMetadata
    {
        public Guid PluginId { get; set; }

        public string Author { get; set; }

        public string IconFileName { get; set; }

        public string Description { get; set; }

        public Version Version { get; set; }
    }
}
