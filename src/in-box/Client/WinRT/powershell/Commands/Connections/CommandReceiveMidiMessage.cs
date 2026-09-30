// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using System.Diagnostics;
using System.IO;
using System.Management.Automation;

using Windows.Devices.Midi2;
using Windows.Devices.Midi2.Enumeration;
using Windows.Devices.Midi2.Utilities.Files;
using Windows.Devices.Midi2.Utilities.Sequencing;
using Windows.Foundation;
using Windows.Storage.Streams;

namespace WindowsMidiServices
{
    // Records what arrives on an endpoint as a Standard MIDI File, for playing back or loading into
    // a sequencer. Runs until -MessageCount, -TimeoutSeconds or Ctrl+C stops it, and the file is
    // written in each case.
    //
    // A Standard MIDI File declares each track's length in front of the track, so nothing can be
    // written until the capture ends. The file is opened at the start anyway, so a path which
    // cannot be written fails before anything is captured rather than after.
    [Cmdlet(VerbsCommunications.Receive, "MidiMessage", DefaultParameterSetName = ConnectionParameterSet)]
    [OutputType(typeof(FileInfo))]
    public class CommandReceiveMidiMessage : MidiCmdletBase, IDisposable
    {
        private const string ConnectionParameterSet = "Connection";
        private const string EndpointDeviceIdParameterSet = "EndpointDeviceId";

        // The same layout as the MIDI Monitor and console captures: 960 ticks per quarter note at a
        // fixed 120 beats per minute, so one tick is a little over half a millisecond.
        private const ushort TicksPerQuarterNote = 960;
        private const double BeatsPerMinute = 120.0;
        private const double MicrosecondsPerQuarterNote = 60000000.0 / BeatsPerMinute;

        // The capture is held in memory until it ends, so it has a ceiling. Reaching it ends the
        // capture and writes the file.
        private const int MaximumMessages = 1000000;

        private readonly record struct CapturedMessage(ulong Timestamp, byte WordCount, uint Word0, uint Word1, uint Word2, uint Word3);

        // Messages arrive on a background thread, and StopProcessing runs on another.
        private readonly object _lock = new();
        private readonly List<CapturedMessage> _captured = [];
        private readonly ManualResetEventSlim _wake = new(false);
        private volatile bool _stopRequested;
        private bool _capturing;
        private bool _reachedLimit;

        [Parameter(Mandatory = true, Position = 0, ValueFromPipeline = true, ParameterSetName = ConnectionParameterSet)]
        public MidiEndpointConnection? Connection { get; set; }

        // Opens a connection for this command alone, so a capture needs no session first
        [Parameter(Mandatory = true, ParameterSetName = EndpointDeviceIdParameterSet)]
        [ValidateNotNullOrWhiteSpace]
        public string EndpointDeviceId { get; set; } = string.Empty;

        // The Standard MIDI File to write, usually named .mid
        [Parameter(Mandatory = true, Position = 1)]
        [ValidateNotNullOrWhiteSpace]
        public string Path { get; set; } = string.Empty;

        // Number of messages to capture. Without it, the capture runs until stopped.
        [Parameter]
        [ValidateRange(1, MaximumMessages)]
        public int MessageCount { get; set; }

        // Without it, the capture runs until stopped.
        [Parameter]
        [ValidateRange(1, int.MaxValue)]
        public int TimeoutSeconds { get; set; }

        // MIDI clock and active sensing arrive many times a second and bury the music, so they are
        // left out unless asked for. Start, stop and continue are real time messages too.
        [Parameter]
        public SwitchParameter IncludeRealTimeMessages { get; set; }

        [Parameter]
        public SwitchParameter Force { get; set; }

        protected override void ProcessRecord()
        {
            RequireMidiServices();

            var fullPath = GetUnresolvedProviderPathFromPSPath(Path);
            var existed = File.Exists(fullPath);

            if (existed && !Force.IsPresent)
            {
                ThrowTerminating(
                    new IOException(Format(Strings.FileExistsFormat, fullPath)),
                    "MidiCaptureFileExists",
                    ErrorCategory.ResourceExists,
                    fullPath);
            }

            FileStream file;

            try
            {
                // An existing file keeps its contents until there is something to replace them with.
                file = new FileStream(fullPath, existed ? FileMode.Open : FileMode.CreateNew, FileAccess.Write, FileShare.Read);
            }
            catch (Exception ex)
            {
                ThrowTerminating(ex, "MidiCaptureFileUnwritable", ErrorCategory.WriteError, fullPath);
                return;
            }

            MidiTemporaryConnection? temporary = null;
            MidiFileWriteResult? result = null;
            var written = false;

            try
            {
                Windows.Devices.Midi2.MidiEndpointConnection connection;

                if (ParameterSetName == ConnectionParameterSet)
                {
                    connection = RequireOpenConnection(Connection);
                }
                else
                {
                    temporary = OpenTemporaryConnection(EndpointDeviceId);
                    connection = temporary.Connection;
                }

                var endpointName = GetEndpointName(connection.ConnectedEndpointDeviceId);

                Capture(connection, endpointName);

                // Released before the file is built, so the endpoint is not held any longer than
                // the capture itself.
                temporary?.Dispose();

                result = WriteCapture(file, endpointName);
                written = result is not null && result.Succeeded;
            }
            finally
            {
                temporary?.Dispose();
                file.Dispose();

                // A file this command created and had nothing to put in is removed, rather than
                // left behind empty.
                if (!written && !existed)
                {
                    try
                    {
                        File.Delete(fullPath);
                    }
                    catch (IOException)
                    {
                    }
                    catch (UnauthorizedAccessException)
                    {
                    }
                }
            }

            // Nothing may be written to the pipeline once a stop has been requested.
            if (!_stopRequested)
            {
                Report(result, fullPath);
            }
        }

        private void Capture(Windows.Devices.Midi2.MidiEndpointConnection connection, string endpointName)
        {
            var includeRealTimeMessages = IncludeRealTimeMessages.IsPresent;
            var messageCount = MessageCount;

            TypedEventHandler<IMidiMessageReceivedEventSource, MidiMessageReceivedEventArgs> handler =
                (_, args) =>
                {
                    var wordCount = args.FillWords(out var word0, out var word1, out var word2, out var word3);

                    if (wordCount == 0 || !IsRecordable(word0, includeRealTimeMessages))
                    {
                        return;
                    }

                    // Zero is the "send immediately" timestamp, and a loopback endpoint passes it
                    // through unchanged, so the time it arrived is the only time there is.
                    var timestamp = args.Timestamp == 0 ? MidiClock.Now : args.Timestamp;

                    lock (_lock)
                    {
                        if (!_capturing)
                        {
                            return;
                        }

                        _captured.Add(new CapturedMessage(timestamp, wordCount, word0, word1, word2, word3));

                        var reachedCount = messageCount > 0 && _captured.Count >= messageCount;

                        _reachedLimit = !reachedCount && _captured.Count >= MaximumMessages;

                        if (reachedCount || _reachedLimit)
                        {
                            _capturing = false;
                            _wake.Set();
                        }
                    }
                };

            lock (_lock)
            {
                _capturing = true;
            }

            connection.MessageReceived += handler;

            try
            {
                WaitForCapture(endpointName);
            }
            finally
            {
                connection.MessageReceived -= handler;

                // A message already on its way into the handler is turned away here.
                lock (_lock)
                {
                    _capturing = false;
                }
            }
        }

        private void WaitForCapture(string endpointName)
        {
            var elapsed = Stopwatch.StartNew();

            var progress = new ProgressRecord(0, Format(Strings.CaptureProgressActivityFormat, endpointName), Strings.CaptureProgressWaiting)
            {
                RecordType = ProgressRecordType.Processing
            };

            var showProgress = true;

            try
            {
                while (!_stopRequested)
                {
                    int count;
                    bool capturing;

                    lock (_lock)
                    {
                        count = _captured.Count;
                        capturing = _capturing;
                    }

                    if (!capturing)
                    {
                        return;
                    }

                    if (TimeoutSeconds > 0 && elapsed.Elapsed.TotalSeconds >= TimeoutSeconds)
                    {
                        return;
                    }

                    if (showProgress)
                    {
                        progress.StatusDescription = Format(Strings.CaptureProgressStatusFormat, count);

                        if (TimeoutSeconds > 0)
                        {
                            progress.SecondsRemaining = (int)Math.Max(0, TimeoutSeconds - elapsed.Elapsed.TotalSeconds);
                        }

                        showProgress = TryWriteProgress(progress);
                    }

                    _wake.Wait(250);
                }
            }
            finally
            {
                if (showProgress)
                {
                    progress.RecordType = ProgressRecordType.Completed;
                    TryWriteProgress(progress);
                }
            }
        }

        // Ctrl+C can land between the check and the call, and the capture must still be saved when
        // it does, so the stop is caught here instead of abandoning the file.
        private bool TryWriteProgress(ProgressRecord progress)
        {
            if (_stopRequested)
            {
                return false;
            }

            try
            {
                WriteProgress(progress);
                return true;
            }
            catch (PipelineStoppedException)
            {
                return false;
            }
        }

        // Null when nothing was captured.
        private MidiFileWriteResult? WriteCapture(FileStream file, string endpointName)
        {
            List<CapturedMessage> captured;

            lock (_lock)
            {
                captured = _captured;
            }

            if (captured.Count == 0)
            {
                return null;
            }

            var sequence = BuildSequence(captured, endpointName);

            // The writer lays the whole file out in memory before any of it is written, because
            // a partial Standard MIDI File is worse than none at all.
            using var memory = new InMemoryRandomAccessStream();

            var result = MidiStandardFileWriter.WriteAsync(memory, sequence).GetAwaiter().GetResult();

            if (result is null || !result.Succeeded)
            {
                return result;
            }

            memory.Seek(0);

            file.SetLength(0);

            using (var input = memory.AsStreamForRead())
            {
                input.CopyTo(file);
            }

            file.Flush();

            return result;
        }

        private static MidiSequence BuildSequence(List<CapturedMessage> captured, string endpointName)
        {
            var builder = new MidiSequenceBuilder
            {
                TicksPerQuarterNote = TicksPerQuarterNote
            };

            builder.AddTempoChange(0, BeatsPerMinute);

            // One track per group, so which group a message arrived on can still be read once the
            // file is loaded into a sequencer. There is nowhere else in the file to keep it.
            var groupSeen = new bool[16];

            foreach (var message in captured)
            {
                groupSeen[GroupIndexOf(message.Word0)] = true;
            }

            var trackForGroup = new ushort[16];

            for (var group = 0; group < 16; group++)
            {
                if (groupSeen[group])
                {
                    trackForGroup[group] = builder.AddTrack(Format(Strings.CaptureGroupTrackNameFormat, endpointName, group + 1));
                }
            }

            var ticksPerSecond = (double)MidiClock.TimestampFrequency;
            var origin = captured[0].Timestamp;
            uint previousTick = 0;

            // The builder copies the words, so one array of each length is enough.
            uint[][] words = [[], new uint[1], new uint[2], new uint[3], new uint[4]];

            foreach (var message in captured)
            {
                var elapsed = message.Timestamp > origin ? message.Timestamp - origin : 0UL;

                var microseconds = elapsed / ticksPerSecond * 1000000.0;
                var scaled = microseconds * TicksPerQuarterNote / MicrosecondsPerQuarterNote;

                var tick = scaled >= 4294967040.0 ? 0xFFFFFF00u : (uint)(scaled + 0.5);

                // Arrival order is the truth here, so a tick never moves backwards even if a
                // timestamp does.
                if (tick < previousTick)
                {
                    tick = previousTick;
                }

                previousTick = tick;

                var messageWords = words[Math.Min((int)message.WordCount, 4)];

                if (messageWords.Length > 0) { messageWords[0] = message.Word0; }
                if (messageWords.Length > 1) { messageWords[1] = message.Word1; }
                if (messageWords.Length > 2) { messageWords[2] = message.Word2; }
                if (messageWords.Length > 3) { messageWords[3] = message.Word3; }

                builder.AddMessages(trackForGroup[GroupIndexOf(message.Word0)], tick, messageWords);
            }

            return builder.GetSequence();
        }

        private void Report(MidiFileWriteResult? result, string fullPath)
        {
            if (result is null)
            {
                WriteWarning(Strings.CaptureNothingReceived);
                return;
            }

            if (!result.Succeeded)
            {
                if (result.Status == MidiFileWriteStatus.NothingToWrite)
                {
                    WriteWarning(Strings.CaptureNothingStorable);
                    return;
                }

                WriteNonTerminating(
                    new IOException(Format(Strings.CaptureWriteFailedFormat, result.Status)),
                    "MidiCaptureWriteFailed",
                    ErrorCategory.WriteError,
                    fullPath);

                return;
            }

            WriteVerbose(Format(Strings.CaptureWrittenFormat, _captured.Count, result.TrackCount, result.ByteCount));

            if (result.SkippedEventCount > 0)
            {
                WriteWarning(Format(Strings.CaptureSkippedFormat, result.SkippedEventCount));
            }

            if (_reachedLimit)
            {
                WriteWarning(Format(Strings.CaptureLimitReachedFormat, MaximumMessages));
            }

            WriteObject(new FileInfo(fullPath));
        }

        // Utility and stream messages have no group and no MIDI 1.0 form, so a Standard MIDI File
        // has no place for them.
        private static bool IsRecordable(uint word0, bool includeRealTimeMessages)
        {
            var messageType = word0 >> 28;

            if (messageType == 0x0 || messageType == 0xF)
            {
                return false;
            }

            if (!includeRealTimeMessages && messageType == 0x1 && ((word0 >> 16) & 0xFF) >= 0xF8)
            {
                return false;
            }

            return true;
        }

        private static int GroupIndexOf(uint word0) => (int)((word0 >> 24) & 0x0F);

        private static string GetEndpointName(string endpointDeviceId)
        {
            var name = MidiEndpointDeviceInformation.CreateFromEndpointDeviceId(endpointDeviceId)?.Name;

            return string.IsNullOrWhiteSpace(name) ? Strings.CaptureDefaultTrackName : name;
        }

        protected override void StopProcessing()
        {
            // The capture ends and the file is still written.
            _stopRequested = true;
            _wake.Set();
        }

        public void Dispose()
        {
            _wake.Dispose();

            GC.SuppressFinalize(this);
        }
    }

}
