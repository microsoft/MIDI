// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using Microsoft.UI.Xaml.Media;

namespace Microsoft.Midi.Settings.Contracts.Services;

public interface IMidiEndpointImageService
{
    public string CopyToImageAssetsFolder(string sourceImagePath);

    public string GetImageAssetFullPath(string imageFileName);
    public string GetImageAssetFileName(string imageFilePath);

    public ImageSource GetImageSource(string imageFilePath);

    //public bool ImageAssetExists(string imageFileName);
}
