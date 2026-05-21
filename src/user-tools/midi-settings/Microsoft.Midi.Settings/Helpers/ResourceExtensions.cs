// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using Microsoft.Midi.Settings.Contracts.Services;
using Microsoft.Windows.ApplicationModel.Resources;

namespace Microsoft.Midi.Settings.Helpers;

public static class ResourceExtensions
{
    private static readonly ResourceLoader _resourceLoader = new();

    public static string GetLocalized(this string resourceKey)
    {
        var loggingService = App.GetService<ILoggingService>();

        try
        {
            var resourceString = _resourceLoader.GetString(resourceKey);

            if (string.IsNullOrEmpty(resourceString))
            {
                loggingService?.LogError($"Resource not found or is empty: '{resourceKey}'");
            }
            else
            {
                // resource found
                return resourceString;
            }
        }
        catch (Exception ex)
        {
            loggingService?.LogError($"Exception getting resource '{resourceKey}'", ex);
        }

        // when the resource is missing, we return just the key
        return $"MISSING:{resourceKey}";
    }

}
