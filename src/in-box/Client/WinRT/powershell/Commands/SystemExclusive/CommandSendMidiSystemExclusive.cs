// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using System.IO;
using System.Management.Automation;

using Windows.Devices.Midi2;
using Windows.Devices.Midi2.Utilities.Messages;
using Windows.Devices.Midi2.Utilities.SysExTransfer;
using Windows.Foundation;

namespace WindowsMidiServices
{

    [Cmdlet(VerbsCommunications.Send, "MidiSystemExclusive", SupportsShouldProcess = true, DefaultParameterSetName = PathParameterSet)]
    public class CommandSendMidiSystemExclusive : MidiCmdletBase
    {
        private const string PathParameterSet = "Path";
        private const string BytesParameterSet = "Bytes";

        private IAsyncOperationWithProgress<bool, MidiSystemExclusiveSendProgress>? _operation;

        [Parameter(Mandatory = true, Position = 0)]
        public MidiEndpointConnection? Connection { get; set; }

        // A binary MIDI 1.0 bytestream SysEx file, usually named .syx
        [Parameter(Mandatory = true, Position = 1, ParameterSetName = PathParameterSet, ValueFromPipeline = true)]
        [ValidateNotNullOrWhiteSpace]
        public string Path { get; set; } = string.Empty;

        [Parameter(Mandatory = true, Position = 1, ParameterSetName = BytesParameterSet, ValueFromPipeline = true)]
        [ValidateNotNullOrEmpty]
        public byte[] Bytes { get; set; } = [];

        [Parameter]
        [ValidateRange(0, 15)]
        public byte GroupIndex { get; set; }

        [Parameter]
        [ValidateRange(1, 65535)]
        public uint MessagesPerTransfer { get; set; } = 1000;

        [Parameter]
        [ValidateRange(0, 5000)]
        public ushort DelayBetweenTransfersMilliseconds { get; set; } = 100;

        protected override void ProcessRecord()
        {
            RequireMidiServices();

            var connection = RequireOpenConnection(Connection);

            Stream source;
            long totalBytes;
            string target;

            if (ParameterSetName == PathParameterSet)
            {
                var fullPath = GetUnresolvedProviderPathFromPSPath(Path);

                if (!File.Exists(fullPath))
                {
                    ThrowTerminating(
                        new FileNotFoundException($"The file \"{fullPath}\" was not found.", fullPath),
                        "MidiSysExFileNotFound",
                        ErrorCategory.ObjectNotFound,
                        fullPath);
                }

                totalBytes = new FileInfo(fullPath).Length;
                source = File.OpenRead(fullPath);
                target = fullPath;
            }
            else
            {
                totalBytes = Bytes.LongLength;
                source = new MemoryStream(Bytes, writable: false);
                target = $"{totalBytes} bytes";
            }

            using (source)
            {
                if (!ShouldProcess(connection.ConnectedEndpointDeviceId, $"Send system exclusive data ({target})"))
                {
                    return;
                }

                SendData(connection, source, totalBytes);
            }
        }

        private void SendData(
            Windows.Devices.Midi2.MidiEndpointConnection connection,
            Stream source,
            long totalBytes)
        {
            long bytesRead = 0;
            long messagesSent = 0;

            var progressRecord = new ProgressRecord(0, "Sending system exclusive data", "Preparing")
            {
                RecordType = ProgressRecordType.Processing
            };

            using var completed = new ManualResetEventSlim(false);

            bool success = false;
            Exception? failure = null;

            try
            {
                _operation = MidiSystemExclusiveSender.SendBinarySysEx7ByteDataAsync(
                    connection,
                    new MidiGroup(GroupIndex),
                    source.AsInputStream(),
                    MessagesPerTransfer,
                    DelayBetweenTransfersMilliseconds,
                    new MidiBytestreamToUmpMessageConverterState());

                // The progress and completion callbacks arrive on a background thread, and
                // WriteProgress may only be called from the pipeline thread, so the counts are
                // stashed here and reported from the wait loop below.
                _operation.Progress = (_, progress) =>
                {
                    Interlocked.Exchange(ref bytesRead, (long)progress.CountBytesRead);
                    Interlocked.Exchange(ref messagesSent, (long)progress.CountMessagesSent);
                };

                _operation.Completed = (operation, status) =>
                {
                    try
                    {
                        if (status == AsyncStatus.Completed)
                        {
                            success = operation.GetResults();
                        }
                        else if (status == AsyncStatus.Error)
                        {
                            failure = operation.ErrorCode;
                        }
                    }
                    catch (Exception ex)
                    {
                        failure = ex;
                    }
                    finally
                    {
                        completed.Set();
                    }
                };

                while (!completed.Wait(250))
                {
                    var read = Interlocked.Read(ref bytesRead);

                    progressRecord.StatusDescription = $"Read {read:N0} of {totalBytes:N0} bytes, sent {Interlocked.Read(ref messagesSent):N0} messages";
                    progressRecord.PercentComplete = totalBytes > 0 ? (int)Math.Clamp(read * 100 / totalBytes, 0, 100) : -1;

                    WriteProgress(progressRecord);
                }
            }
            finally
            {
                progressRecord.RecordType = ProgressRecordType.Completed;
                WriteProgress(progressRecord);

                _operation = null;
            }

            if (failure is not null)
            {
                ThrowTerminating(failure, "MidiSysExSendFailed", ErrorCategory.WriteError, connection.ConnectedEndpointDeviceId);
                return;
            }

            if (!success)
            {
                ThrowTerminating(
                    new InvalidOperationException("The system exclusive transfer did not complete."),
                    "MidiSysExSendIncomplete",
                    ErrorCategory.WriteError,
                    connection.ConnectedEndpointDeviceId);

                return;
            }

            WriteVerbose($"Read {Interlocked.Read(ref bytesRead):N0} bytes and sent {Interlocked.Read(ref messagesSent):N0} UMP messages.");
        }

        protected override void StopProcessing()
        {
            _operation?.Cancel();
        }
    }

}
