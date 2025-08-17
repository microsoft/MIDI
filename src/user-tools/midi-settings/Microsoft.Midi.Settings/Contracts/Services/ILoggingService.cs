// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using System.Runtime.CompilerServices;

namespace Microsoft.Midi.Settings.Contracts.Services;

public interface ILoggingService
{
    void LogError(string message, [CallerMemberName] string memberName = "", [CallerFilePath] string filePath = "");
   
    void LogError(string message, Exception ex, [CallerMemberName] string memberName = "", [CallerFilePath] string filePath = "");

    void LogWarning(string message, [CallerMemberName] string memberName = "", [CallerFilePath] string filePath = "");

    void LogInfo(string message, [CallerMemberName] string memberName = "", [CallerFilePath] string filePath = "");

}
