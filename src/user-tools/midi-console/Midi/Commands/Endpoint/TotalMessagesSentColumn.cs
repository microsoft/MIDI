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
