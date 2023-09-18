using Spectre.Console;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Windows.Devices.Enumeration;
using Windows.Devices.Midi2;

using Microsoft.Devices.Midi2.ConsoleApp.Resources;

namespace Microsoft.Devices.Midi2.ConsoleApp
{


    internal class FunctionBlockPicker
    {
        public static MidiFunctionBlock? PickFunctionBlock(List<MidiFunctionBlock> functionBlocks)
        {
            if (functionBlocks.Count > 0)
            {
                var result = AnsiConsole.Prompt(
                    new SelectionPrompt<MidiFunctionBlock>()
                        .Title("Please select a function block")
                        .AddChoices(functionBlocks)
                    );


                if (result != null)
                {
                    return result;
                }
            }

            return null;
        }


    }
}
