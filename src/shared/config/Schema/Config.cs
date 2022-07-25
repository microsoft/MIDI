using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace MidiConfig.Schema
{
    public class Config
    {

        public Header Header { get; set; } = new Header();

        public Locations Locations { get; set; } = new Locations();

        public string CurrentSetupFileNameWithoutPath { get; set; }


        public List<Plugin> TrustedPlugins { get; set; } = new List<Plugin>();



    }
}
