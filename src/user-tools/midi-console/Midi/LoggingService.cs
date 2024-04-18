using Microsoft.Extensions.Logging;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Text;
using System.Threading.Tasks;
using Windows.Foundation.Diagnostics;


namespace Microsoft.Devices.Midi2.ConsoleApp
{
    public class LoggingService
    {
        private LoggingChannel _loggingChannel;

        private static LoggingService? _current;

        public static LoggingService Current
        {
            get
            {
                if (_current == null)
                    _current = new LoggingService();

                return _current;
            }
        }


        public LoggingService()
        {
            var channelId = new Guid("ce8536f1-a223-5028-7c7f-c64452906687");
            // guid from: PS> [System.Diagnostics.Tracing.EventSource]::new("Microsoft.Midi.Console").Guid

            _loggingChannel = new LoggingChannel("MIDI Console", null, channelId);
        }

        private string BuildEventName(string filePath, string memberName)
        {
            //var fileName = Path.GetFileName(filePath);

            var className = Path.GetFileNameWithoutExtension(filePath);

            return $"{className}::{memberName}";
        }


        public void LogError(string message, [CallerMemberName] string memberName = "", [CallerFilePath] string filePath = "")
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
