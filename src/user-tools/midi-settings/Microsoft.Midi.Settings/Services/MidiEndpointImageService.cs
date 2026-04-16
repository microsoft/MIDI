// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using Microsoft.Midi.Settings.Contracts.Services;
using Microsoft.UI.Xaml.Media;
using Microsoft.UI.Xaml.Media.Imaging;
using Microsoft.Windows.Devices.Midi2.Utilities.Metadata;
using System;
using System.Collections.Generic;
using System.Security.Cryptography;
using System.Text;
using Windows.Devices.Enumeration;

namespace Microsoft.Midi.Settings.Services;

public class MidiEndpointImageService : IMidiEndpointImageService
{
    private const string RawImagesFileLocation = @"%allusersprofile%\Microsoft\MIDI\Assets\Endpoints";
    private const string EndpointImagePrefix = "ep-";


    private readonly ILoggingService _loggingService;
    public MidiEndpointImageService(ILoggingService loggingService)
    {
        _loggingService = loggingService;
    }

    private string GetImagesFolder()
    {
        return Environment.ExpandEnvironmentVariables(RawImagesFileLocation);
    }

    private string GetPersistentImageRootFileName(string sourceImagePath)
    {
        _loggingService.LogInfo($"Enter");

        if (String.IsNullOrWhiteSpace(sourceImagePath)) return string.Empty;

        return EndpointImagePrefix + Path.GetFileName(sourceImagePath);
    }


    private bool FilesAreIdentical(string fullPath1, string fullPath2)
    {
        _loggingService.LogInfo($"Enter");

        const uint bufferSize = 2048;

        using var stream1 = new FileStream(fullPath1, FileMode.Open, FileAccess.Read);
        using var stream2 = new FileStream(fullPath2, FileMode.Open, FileAccess.Read);

        // if different file sizes, they aren't identical
        if (stream1.Length != stream2.Length) return false;

        Span<byte> buffer1 = new byte[bufferSize];
        Span<byte> buffer2 = new byte[bufferSize];

        while (true)
        {
            var countBytesRead1 = stream1.Read(buffer1);
            var countBytesRead2 = stream2.Read(buffer2);

            // count of bytes read is unequal. Shouldn't happen since we already tested stream size
            if (countBytesRead1 != countBytesRead2)
                return false;

            // no data
            if (countBytesRead1 == 0)
                return true;

            if (!buffer1.SequenceEqual(buffer2))
                return false;
        }

    }


    public string CopyToImageAssetsFolder(string sourceImagePath)
    {
        _loggingService.LogInfo($"Enter");

        if (String.IsNullOrWhiteSpace(sourceImagePath)) return string.Empty;

        string bareFileName = GetPersistentImageRootFileName(sourceImagePath);
        if (String.IsNullOrWhiteSpace(bareFileName)) return string.Empty;

        var destinationPath = Path.Combine(GetImagesFolder(), bareFileName);

        if (destinationPath.ToLower() == sourceImagePath.ToLower())
        {
            // trying to copy over itself.

            _loggingService.LogError($"Cannot copy image over itself.");
            return string.Empty;
        }

        // find an unused name

        string newDestinationPath = destinationPath;

        short index = 1;
        while (File.Exists(newDestinationPath))
        {
            if (FilesAreIdentical(sourceImagePath, newDestinationPath))
            {
                // if the file is already in the folder, just use it. This avoids multiple copies of identical files
                return newDestinationPath; 
            }

            if (index > 500)
            {
                _loggingService.LogError($"Tried to find a unique name, but we hit the limit of the number of attempts.");
                return string.Empty;
            }

            string newFileName = Path.GetFileName(newDestinationPath);
            string newFileNameWithoutExtension = Path.GetFileNameWithoutExtension(newFileName);
            string extension = Path.GetExtension(newFileName);

            newDestinationPath = Path.Combine(GetImagesFolder(), $"{newFileNameWithoutExtension} ({index}){extension}");

            index++;
        }

        // we have a name we can use. Copy the file.

        try
        {
            File.Copy(sourceImagePath, newDestinationPath, false);

            return newDestinationPath;
        }
        catch (Exception ex)
        {
            _loggingService.LogError($"Could not copy image asset.", ex);

            return string.Empty;
        }

    }

    public bool ImageAssetExists(string imageFileName)
    {
        _loggingService.LogInfo($"Enter");

        string fullPath = Path.Combine(GetImagesFolder(), Path.GetFileName(imageFileName));

        //return Microsoft.Windows.Devices.Midi2.Utilities.Metadata.MidiImageAssetHelper.EndpointHasValidCustomImageAsset(endpoint);


        return File.Exists(fullPath);
    }

    public string GetImageAssetFileName(string imageFilePath)
    {
        _loggingService.LogInfo($"Enter");

        return Path.GetFileName(imageFilePath);
    }


    public string GetImageAssetFullPath(string imageFileName)
    {
        _loggingService.LogInfo($"Enter");

        return Path.Combine(GetImagesFolder(), Path.GetFileName(imageFileName));
    }

    public ImageSource GetImageSource(string imageFilePath)
    {
        _loggingService.LogInfo($"Enter");

        if (Path.GetExtension(imageFilePath).ToLower() == ".svg")
        {
            // SVG requires a specific decoder
            var source = new SvgImageSource(new Uri(imageFilePath, UriKind.Absolute));
            return source;
        }
        else
        {
            // this works with PNG, JPG, etc.
            var source = new BitmapImage(new Uri(imageFilePath, UriKind.Absolute));
            return source;
        }
    }

}
