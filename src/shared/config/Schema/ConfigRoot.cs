using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Windows.Midi.Internal.Config.Schema
{
    public class ConfigRoot
    {

        public ConfigHeader Header { get; set; } = new ConfigHeader();

        public Locations Locations { get; set; } = new Locations();

        public string CurrentSetupFileNameWithoutPath { get; set; }

        public Settings Settings { get; set; } = new Settings();

        public List<Plugin> TrustedPlugins { get; set; } = new List<Plugin>();

    }
}
