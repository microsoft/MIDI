// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================



namespace Microsoft.Midi.ConsoleApp
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
