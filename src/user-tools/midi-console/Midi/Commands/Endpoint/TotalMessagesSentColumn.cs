// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using Spectre.Console.Rendering;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Midi.ConsoleApp
{
    public class TotalMessagesSentColumn : ProgressColumn
    {
        private const int _columnWidth = 7;

        public override IRenderable Render(RenderOptions options, ProgressTask task, TimeSpan deltaTime)
        {
            return new Markup($"[deepskyblue1]{task.Value}[/]");
        }

        public override int? GetColumnWidth(RenderOptions options)
        {
            return _columnWidth;
        }
    }
}
