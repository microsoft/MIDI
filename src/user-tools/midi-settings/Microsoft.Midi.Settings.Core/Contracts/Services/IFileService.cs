// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using System.Text.Json;

namespace Microsoft.Midi.Settings.Core.Contracts.Services;

public interface IFileService
{
    global::Windows.Data.Json.JsonObject Read(string folderPath, string fileName);

    void Save(string folderPath, string fileName, global::Windows.Data.Json.JsonObject content);

    void Delete(string folderPath, string fileName);
}
