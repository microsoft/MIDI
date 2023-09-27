using Microsoft.Devices.Midi2.ConsoleApp.Resources;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Devices.Midi2.ConsoleApp
{
    public class LocalizedDescriptionAttribute : DescriptionAttribute
    {
        public override string Description
        {
            get
            {
                //return Localize(base.Description);

                string? value = Strings.ResourceManager.GetString(base.Description);

                //System.Diagnostics.Debug.WriteLine("Resource {0}={1}", base.Description, value);

                if (value != null)
                {
                    return value;
                }
                else
                {
                    return "Missing resource: " + base.Description;
                }

            }
        }

        public LocalizedDescriptionAttribute(string key) : base(key) { }

    }
}
