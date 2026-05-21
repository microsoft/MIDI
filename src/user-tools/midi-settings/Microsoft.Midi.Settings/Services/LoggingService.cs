// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using Microsoft.Midi.Settings.Contracts.Services;
using Microsoft.UI.Xaml.Markup;
using System.Runtime.CompilerServices;
using Windows.Foundation.Diagnostics;



namespace Microsoft.Midi.Settings.Services;

public partial class LoggingService : ILoggingService
{
    private const string MIDI_LOG_EVENT_LOCATION_FIELD = "Location";
    private const string MIDI_EVENT_NAME_VERBOSE = "Midi.Verbose";
    private const string MIDI_EVENT_NAME_INFO = "Midi.Info";
    private const string MIDI_EVENT_NAME_WARNING = "Midi.Warning";
    private const string MIDI_EVENT_NAME_ERROR = "Midi.Error";

    //ILogger _logger;

    LoggingChannel _loggingChannel;

    public LoggingService()
    {
        var channelId = new Guid("66eaccfa-db3a-58d6-bf78-778229bf5a18");
        // guid from: PS> [System.Diagnostics.Tracing.EventSource]::new("Microsoft.Midi.Settings").Guid

        _loggingChannel = new LoggingChannel("MIDI Settings", null, channelId);
    }


    ~LoggingService()
    {
        _loggingChannel.Dispose();
    }

    private string BuildLocationName(string filePath, string memberName)
    {
        //var fileName = Path.GetFileName(filePath);

        var className = Path.GetFileNameWithoutExtension(filePath);

        return $"{className}::{memberName}";
    }


    public void LogError(string message, [CallerMemberName]string memberName="", [CallerFilePath]string filePath="")
    {
        string eventName = MIDI_EVENT_NAME_ERROR;

        var fields = new LoggingFields();
        fields.AddString(MIDI_LOG_EVENT_LOCATION_FIELD, BuildLocationName(filePath, memberName));
        fields.AddString("file name", filePath);
        fields.AddString("culture", System.Globalization.CultureInfo.CurrentCulture.ToString());
        fields.AddString("message", message);

        _loggingChannel.LogEvent(eventName, fields, LoggingLevel.Error);
    }

    public void LogError(string message, Exception ex, [CallerMemberName] string memberName = "", [CallerFilePath] string filePath = "")
    {
        string eventName = MIDI_EVENT_NAME_ERROR;

        var fields = new LoggingFields();
        fields.AddString(MIDI_LOG_EVENT_LOCATION_FIELD, BuildLocationName(filePath, memberName));
        fields.AddString("file name", filePath);
        fields.AddString("culture", System.Globalization.CultureInfo.CurrentCulture.ToString());
        fields.AddString("message", message);
        fields.AddString("exception", ex.ToString());

        if (ex.InnerException != null)
        {
            fields.AddString("inner exception", ex.InnerException.ToString());
        }
        
        foreach (var key in ex.Data.Keys)
        {
            fields.AddString(key.ToString(), ex.Data[key]?.ToString());
        }

        _loggingChannel.LogEvent(eventName, fields, LoggingLevel.Error);
    }

    public void LogWarning(string message, [CallerMemberName] string memberName = "", [CallerFilePath] string filePath = "")
    {
        string eventName = MIDI_EVENT_NAME_WARNING;

        var fields = new LoggingFields();
        fields.AddString(MIDI_LOG_EVENT_LOCATION_FIELD, BuildLocationName(filePath, memberName));
        fields.AddString("file name", filePath);
        fields.AddString("culture", System.Globalization.CultureInfo.CurrentCulture.ToString());
        fields.AddString("message", message);

        _loggingChannel.LogEvent(eventName, fields, LoggingLevel.Warning);
    }

    public void LogInfo(string message, [CallerMemberName] string memberName = "", [CallerFilePath] string filePath = "")
    {
        string eventName = MIDI_EVENT_NAME_INFO;

        var fields = new LoggingFields();
        fields.AddString(MIDI_LOG_EVENT_LOCATION_FIELD, BuildLocationName(filePath, memberName));
        fields.AddString("file name", filePath);
        fields.AddString("culture", System.Globalization.CultureInfo.CurrentCulture.ToString());
        fields.AddString("message", message);

        _loggingChannel.LogEvent(eventName, fields, LoggingLevel.Information);
    }

    public void LogVerbose(string message, [CallerMemberName] string memberName = "", [CallerFilePath] string filePath = "")
    {
        string eventName = MIDI_EVENT_NAME_VERBOSE;

        var fields = new LoggingFields();
        fields.AddString(MIDI_LOG_EVENT_LOCATION_FIELD, BuildLocationName(filePath, memberName));
        fields.AddString("file name", filePath);
        fields.AddString("culture", System.Globalization.CultureInfo.CurrentCulture.ToString());
        fields.AddString("message", message);

        _loggingChannel.LogEvent(eventName, fields, LoggingLevel.Verbose);
    }

}
