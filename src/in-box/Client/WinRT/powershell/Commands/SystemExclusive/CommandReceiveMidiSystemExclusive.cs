// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using System.Collections.Concurrent;
using System.Diagnostics;
using System.IO;
using System.Management.Automation;

using Windows.Devices.Midi2;
using Windows.Devices.Midi2.Utilities.SysExTransfer;
using Windows.Foundation;

namespace WindowsMidiServices
{
    // Receiving is open ended, so this runs until one of -MessageCount, -TimeoutSeconds or
    // Ctrl+C stops it. Whatever has already arrived is kept in either case.
    [Cmdlet(VerbsCommunications.Receive, "MidiSystemExclusive", DefaultParameterSetName = ObjectParameterSet)]
    [OutputType(typeof(MidiSystemExclusiveMessage), ParameterSetName = new[] { ObjectParameterSet })]
    [OutputType(typeof(FileInfo), ParameterSetName = new[] { PathParameterSet })]
    public class CommandReceiveMidiSystemExclusive : MidiCmdletBase, IDisposable
    {
        private const string ObjectParameterSet = "Object";
        private const string PathParameterSet = "Path";

        private readonly CancellationTokenSource _stopRequested = new();

        [Parameter(Mandatory = true, Position = 0, ValueFromPipeline = true)]
        public MidiEndpointConnection? Connection { get; set; }

        // Written as it arrives, so the file is complete even when the cmdlet is interrupted
        [Parameter(Mandatory = true, Position = 1, ParameterSetName = PathParameterSet)]
        [ValidateNotNullOrWhiteSpace]
        public string Path { get; set; } = string.Empty;

        [Parameter]
        [ValidateRange(0, 15)]
        public byte GroupIndex { get; set; }

        // Bounds how much is buffered before a block is handed back, so a long transfer can be
        // reported as it goes rather than only at the end
        [Parameter]
        [ValidateRange(8, 65536)]
        public uint MaximumBytesPerEvent { get; set; } = 256;

        // Number of complete messages to wait for. Zero waits until stopped.
        [Parameter]
        [ValidateRange(1, int.MaxValue)]
        public int MessageCount { get; set; }

        // Zero waits until stopped.
        [Parameter]
        [ValidateRange(1, int.MaxValue)]
        public int TimeoutSeconds { get; set; }

        [Parameter(ParameterSetName = PathParameterSet)]
        public SwitchParameter Force { get; set; }

        protected override void ProcessRecord()
        {
            RequireMidiServices();

            var connection = RequireOpenConnection(Connection);

            var fullPath = ParameterSetName == PathParameterSet ? ResolveOutputPath() : null;

            var received = new BlockingCollection<MidiSystemExclusiveMessage>(new ConcurrentQueue<MidiSystemExclusiveMessage>());

            TypedEventHandler<MidiSystemExclusiveReceiver, MidiSystemExclusiveReceivedEventArgs> handler =
                (_, args) =>
                {
                    try
                    {
                        received.Add(ToMessage(args));
                    }
                    catch (InvalidOperationException)
                    {
                        // the collection was completed while this event was in flight
                    }
                };

            var receiver = new MidiSystemExclusiveReceiver(connection, new MidiGroup(GroupIndex), MaximumBytesPerEvent);

            FileStream? outputStream = null;

            ulong bytesReceived = 0;
            ulong messagesReceived = 0;

            try
            {
                outputStream = fullPath is null
                    ? null
                    : new FileStream(fullPath, FileMode.Create, FileAccess.Write, FileShare.Read);

                receiver.BytesReceived += handler;

                if (!receiver.Start())
                {
                    ThrowTerminating(
                        new InvalidOperationException("Unable to start receiving system exclusive data."),
                        "MidiSysExReceiveStartFailed",
                        ErrorCategory.OpenError,
                        connection.ConnectedEndpointDeviceId);

                    return;
                }

                WriteVerbose($"Listening on group {GroupIndex + 1} of {connection.ConnectedEndpointDeviceId}.");

                Drain(received, outputStream);
            }
            finally
            {
                // Stop flushes whatever is still buffered, which may raise further events, so the
                // handler stays attached until after it returns.
                receiver.Stop();
                receiver.BytesReceived -= handler;

                received.CompleteAdding();

                bytesReceived = receiver.CountBytesReceived;
                messagesReceived = receiver.CountMessagesReceived;

                receiver.Dispose();

                DrainRemaining(received, outputStream);

                outputStream?.Dispose();
            }

            // Nothing may be written to the pipeline once a stop has been requested.
            if (_stopRequested.IsCancellationRequested)
            {
                return;
            }

            WriteVerbose($"Received {bytesReceived:N0} bytes in {messagesReceived:N0} messages.");

            if (fullPath is not null)
            {
                WriteObject(new FileInfo(fullPath));
            }
        }

        private string ResolveOutputPath()
        {
            var fullPath = GetUnresolvedProviderPathFromPSPath(Path);

            if (File.Exists(fullPath) && !Force.IsPresent)
            {
                ThrowTerminating(
                    new IOException($"The file \"{fullPath}\" already exists. Use -Force to overwrite it."),
                    "MidiSysExFileExists",
                    ErrorCategory.ResourceExists,
                    fullPath);
            }

            return fullPath;
        }

        private void Drain(BlockingCollection<MidiSystemExclusiveMessage> received, FileStream? outputStream)
        {
            var elapsed = Stopwatch.StartNew();
            var completeMessages = 0;

            while (true)
            {
                if (TimeoutSeconds > 0 && elapsed.Elapsed.TotalSeconds >= TimeoutSeconds)
                {
                    return;
                }

                MidiSystemExclusiveMessage? message;

                try
                {
                    if (!received.TryTake(out message, 200, _stopRequested.Token))
                    {
                        continue;
                    }
                }
                catch (OperationCanceledException)
                {
                    return;
                }

                Emit(message, outputStream);

                if (message.IsPartial)
                {
                    continue;
                }

                completeMessages++;

                if (MessageCount > 0 && completeMessages >= MessageCount)
                {
                    return;
                }
            }
        }

        private void DrainRemaining(BlockingCollection<MidiSystemExclusiveMessage> received, FileStream? outputStream)
        {
            while (received.TryTake(out var message))
            {
                Emit(message, outputStream);
            }
        }

        private void Emit(MidiSystemExclusiveMessage message, FileStream? outputStream)
        {
            if (outputStream is not null)
            {
                outputStream.Write(message.Bytes, 0, message.Bytes.Length);
                outputStream.Flush();

                return;
            }

            if (!_stopRequested.IsCancellationRequested)
            {
                WriteObject(message);
            }
        }

        private static MidiSystemExclusiveMessage ToMessage(MidiSystemExclusiveReceivedEventArgs args)
        {
            var bytes = new byte[args.Bytes.Count];

            for (var i = 0; i < bytes.Length; i++)
            {
                bytes[i] = args.Bytes[i];
            }

            return new MidiSystemExclusiveMessage
            {
                GroupIndex = args.Group.Index,
                GroupNumber = args.Group.DisplayValue,
                Bytes = bytes,
                IsPartial = args.IsPartial
            };
        }

        protected override void StopProcessing()
        {
            _stopRequested.Cancel();
        }

        public void Dispose()
        {
            _stopRequested.Dispose();

            GC.SuppressFinalize(this);
        }
    }

}
