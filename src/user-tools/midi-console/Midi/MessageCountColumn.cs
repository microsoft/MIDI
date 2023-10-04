using Spectre.Console.Rendering;
using Spectre.Console;
using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Windows.UI.StartScreen;

namespace Microsoft.Devices.Midi2.ConsoleApp
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
