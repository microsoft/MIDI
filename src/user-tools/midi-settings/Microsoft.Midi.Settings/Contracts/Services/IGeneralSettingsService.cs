// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

namespace Microsoft.Midi.Settings.Contracts.Services;

public class WindowRect
{
    public int Left { get; set; }
    public int Top { get; set; }
    public int Width { get; set; }
    public int Height { get; set; }

    public WindowRect()
    {
        Left = 0; 
        Top = 0;
        Width = 1366;
        Height = 700;
    }

    public WindowRect(int left, int top, int width, int height)
    {
        Left = left;
        Top = top;
        Width = width;
        Height = height;
    }

    public override string ToString()
    {
        return $"{Left},{Top},{Width},{Height}";
    }

    public static WindowRect Parse(string stringValue)
    {
        var values = stringValue.Split(',');

        if (values.Length != 4)
        {
            return new WindowRect();
        }

        int left = 0;
        int top = 0;
        int width = 1366;
        int height = 700;

        int.TryParse(values[0], out left);
        int.TryParse(values[1], out top);
        int.TryParse(values[2], out width);
        int.TryParse(values[3], out height);

        return new WindowRect(left, top, width, height);
    }
}

public enum EndpointListView
{
    Default = 0,
    CardView,
    ListView
}

public interface IGeneralSettingsService
{
    //bool ShowDeveloperOptions
    //{
    //    get; set;
    //}

    public WindowRect? GetMainWindowPositionAndSize();
    public void SetMainWindowPositionAndSize(WindowRect value);


    public EndpointListView GetEndpointListLastUsedView();
    public void SetEndpointListLastUsedView(EndpointListView view);


    Task InitializeAsync();

//    event EventHandler SettingsChanged;
}
