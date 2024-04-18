using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Logging.EventLog;
using Microsoft.Midi.Settings.Contracts.Services;
using Microsoft.UI.Xaml.Automation;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Text;
using System.Threading.Tasks;
using Windows.Foundation.Diagnostics;
using WinUIEx.Messaging;


namespace Microsoft.Midi.Settings.Services
{
    public partial class LoggingService : ILoggingService
    {
        //ILogger _logger;

        LoggingChannel _loggingChannel;

        public LoggingService()
        {
            var channelId = new Guid("66eaccfa-db3a-58d6-bf78-778229bf5a18");
            // guid from: PS> [System.Diagnostics.Tracing.EventSource]::new("Microsoft.Midi.Settings").Guid

            _loggingChannel = new LoggingChannel("MIDI Settings", null, channelId);
        }

        private string BuildEventName(string filePath, string memberName)
        {
            //var fileName = Path.GetFileName(filePath);

            var className = Path.GetFileNameWithoutExtension(filePath);

            return $"{className}::{memberName}";
        }


        public void LogError(string message, [CallerMemberName]string memberName="", [CallerFilePath]string filePath="")
        {
            string eventName = BuildEventName(filePath, memberName);

            var fields = new LoggingFields();
            fields.AddString("file name", filePath);
            fields.AddString("message", message);

            _loggingChannel.LogEvent(eventName, fields, LoggingLevel.Error);
        }

        public void LogError(string message, Exception ex, [CallerMemberName] string memberName = "", [CallerFilePath] string filePath = "")
        {
            string eventName = BuildEventName(filePath, memberName);

            var fields = new LoggingFields();
            fields.AddString("file name", filePath);
            fields.AddString("message", message);
            fields.AddString("exception", ex.ToString());

            _loggingChannel.LogEvent(eventName, fields, LoggingLevel.Error);
        }


        public void LogWarning(string message, [CallerMemberName] string memberName = "", [CallerFilePath] string filePath = "")
        {
            string eventName = BuildEventName(filePath, memberName);

            var fields = new LoggingFields();
            fields.AddString("file name", filePath);
            fields.AddString("message", message);

            _loggingChannel.LogEvent(eventName, fields, LoggingLevel.Warning);
        }

        public void LogInfo(string message, [CallerMemberName] string memberName = "", [CallerFilePath] string filePath = "")
        {
            string eventName = BuildEventName(filePath, memberName);

            var fields = new LoggingFields();
            fields.AddString("file name", filePath);
            fields.AddString("message", message);

            _loggingChannel.LogEvent(eventName, fields, LoggingLevel.Information);
        }

    }
}
