// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using Newtonsoft.Json;

namespace Microsoft.Midi.Settings.Core.Helpers;

public static class Json
{
    public static T ToObject<T>(string value)
    {
        return JsonConvert.DeserializeObject<T>(value);
    }

    public static async Task<T> ToObjectAsync<T>(string value)
    {
        return await Task.Run<T>(() =>
        {
            return JsonConvert.DeserializeObject<T>(value);
        });
    }

    public static async Task<string> StringifyAsync(object value)
    {
        return await Task.Run<string>(() =>
        {
            var serializerSettings = new JsonSerializerSettings();
            serializerSettings.StringEscapeHandling = StringEscapeHandling.EscapeNonAscii;           

            var serialized = JsonConvert.SerializeObject(value, serializerSettings);

            System.Diagnostics.Debug.WriteLine("Serialized: " + serialized);
            return serialized;
        });
    }
}
