using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

using Microsoft.Devices.Midi2.ConsoleApp.Resources;

namespace Microsoft.Devices.Midi2.ConsoleApp
{
    public class EnumLocalizedDescriptionAttribute : DescriptionAttribute
    {
        private Type _t;

        public override string Description
        {
            get
            {
                // this gets the prefix
                string? value = Strings.ResourceManager.GetString(base.Description);

                if (value != null)
                {
                    // this gets the enum

                    value += " " + string.Join(", ", Enum.GetNames(_t));


                    return value;
                }
                else
                {
                    return "Missing resource: " + base.Description;
                }

            }
        }

        public EnumLocalizedDescriptionAttribute(string key, Type t) : base(key) { _t = t; }

    }
}