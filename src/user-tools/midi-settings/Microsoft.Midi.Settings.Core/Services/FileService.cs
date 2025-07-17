// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using System.Text;
using System.Text.Json;
using Microsoft.Midi.Settings.Core.Contracts.Services;

using Newtonsoft.Json;

namespace Microsoft.Midi.Settings.Core.Services;

public class FileService : IFileService
{
    public global::Windows.Data.Json.JsonObject Read(string folderPath, string fileName)
    {
        var path = Path.Combine(folderPath, fileName);
        if (File.Exists(path))
        {
            var json = File.ReadAllText(path);

            return global::Windows.Data.Json.JsonObject.Parse(json);

            //return JsonConvert.DeserializeObject<T>(json);
        }

        return default;
    }

    public void Save(string folderPath, string fileName, global::Windows.Data.Json.JsonObject content)
    {
        if (!Directory.Exists(folderPath))
        {
            Directory.CreateDirectory(folderPath);
        }

        var fileContent = content.Stringify();

        File.WriteAllText(Path.Combine(folderPath, fileName), fileContent, Encoding.UTF8);
    }

    public void Delete(string folderPath, string fileName)
    {
        if (fileName != null && File.Exists(Path.Combine(folderPath, fileName)))
        {
            File.Delete(Path.Combine(folderPath, fileName));
        }
    }
}
