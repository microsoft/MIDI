using System;
using System.Collections.Generic;
using System.Text;

namespace MidiSettings.Core.Models;

public class SamplePluginBase
{
    public bool IsEnabled
    {
        get; set;
    }


    public string Id
    {
        get; set;
    }

    public string Name
    {
        get; set;
    }

    public string Description
    {
        get; set;
    }
    public string Author
    {
        get; set;
    }

    public string Version
    {
        get; set;
    }

    public string FullPath
    {
        get; set;
    }

    // Need a better way of representing icons. Could be a path to an image, or a resource, or a font glyph
    public string IconPath
    {
        get; set;
    }

    public string IconGlyph
    {
        get; set;
    }


    // JSON string adhering to the UI scheme for plugins
    public string ConfigurationUI
    {
        get; set;
    }
}
