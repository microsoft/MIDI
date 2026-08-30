// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using Spectre.Console.Rendering;
using System.Globalization;


namespace Microsoft.Midi.ConsoleApp
{
    public sealed class MessageCountColumn : ProgressColumn
    {
        /// <summary>
        /// Gets or sets the <see cref="CultureInfo"/> to use.
        /// </summary>
        public CultureInfo? Culture { get; set; }

        /// <inheritdoc/>
        /// 
        // example here https://github.com/spectreconsole/spectre.console/blob/main/src/Spectre.Console/Live/Progress/Columns/DownloadedColumn.cs
        public override IRenderable Render(RenderOptions options, ProgressTask task, TimeSpan deltaTime)
        {
            return new Markup(string.Format("{0}", task.Value));
        }
    }
}
