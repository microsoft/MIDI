// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using System.Management.Automation;

using Windows.Devices.Midi2;
using Windows.Devices.Midi2.Transports.Synth;
using Windows.Devices.Midi2.Utilities.Files;
using Windows.Devices.Midi2.Utilities.Sequencing;

namespace WindowsMidiServices
{
    // Plays a Standard MIDI File. Waits for the file to finish unless told not to, because that is
    // what a script usually wants and it means nothing has to be kept alive by hand.
    //
    // Stopping with Ctrl+C silences the instrument rather than leaving notes sounding.
    [Cmdlet(VerbsLifecycle.Start, "MidiFilePlayback")]
    [OutputType(typeof(MidiFilePlayback))]
    public class CommandStartMidiFilePlayback : MidiCmdletBase
    {
        [Parameter(Mandatory = true, Position = 0, ValueFromPipeline = true, ValueFromPipelineByPropertyName = true)]
        [Alias("FullName", "Path")]
        public string FilePath { get; set; } = string.Empty;

        // Defaults to the built-in synthesizer, so "play this file" needs nothing else.
        [Parameter(Position = 1)]
        public string? EndpointDeviceId { get; set; }

        [Parameter()]
        [ValidateRange(0, 15)]
        public byte Group { get; set; } = 0;

        // How far into the file to begin. Each channel's bank, program and controllers are sent
        // as they stand at that point, so the music starts on the right sounds.
        [Parameter()]
        [ValidateRange(ValidateRangeKind.NonNegative)]
        public double StartAtSeconds { get; set; }

        // Returns as soon as playback starts, and hands back an object to stop it with. Keep that
        // object: dropping it ends playback.
        [Parameter()]
        public SwitchParameter NoWait { get; set; }

        // StopProcessing runs on a different thread from the pipeline, so the wait loop and the
        // tear-down have to agree on who is holding the player.
        private readonly object _lock = new();
        private Windows.Devices.Midi2.MidiSession? _session;
        private MidiSequencePlayer? _player;

        protected override void ProcessRecord()
        {
            RequireMidiServices();

            var resolved = ResolveFilePath();

            if (resolved is null)
            {
                return;
            }

            var endpointDeviceId = ResolveEndpointDeviceId();

            if (endpointDeviceId is null)
            {
                return;
            }

            var sequence = ReadSequence(resolved);

            if (sequence is null)
            {
                return;
            }

            var startMicroseconds = (ulong)(StartAtSeconds * 1000000.0);

            // Past the end, the player would quietly start again from the top instead.
            if (startMicroseconds > 0 && startMicroseconds >= sequence.DurationMicroseconds)
            {
                WriteNonTerminating(
                    new ArgumentOutOfRangeException(
                        nameof(StartAtSeconds),
                        Format(Strings.PlaybackStartBeyondEndFormat, sequence.DurationMicroseconds / 1000000.0)),
                    "MidiPlaybackStartBeyondEnd",
                    ErrorCategory.InvalidArgument,
                    StartAtSeconds);

                return;
            }

            _session = Windows.Devices.Midi2.MidiSession.Create(
                Format(Strings.PlaybackSessionNameFormat, System.IO.Path.GetFileName(resolved)));

            if (_session is null)
            {
                ThrowTerminating(
                    new InvalidOperationException(Strings.SessionCreationFailed),
                    "MidiSessionFailed",
                    ErrorCategory.ResourceUnavailable);

                return;
            }

            _player = MidiSequencePlayer.CreateForEndpointAsync(
                _session, endpointDeviceId, new MidiGroup(Group)).GetAwaiter().GetResult();

            if (_player is null)
            {
                CleanUp();

                ThrowTerminating(
                    new InvalidOperationException(Format(Strings.PlaybackEndpointFailedFormat, endpointDeviceId)),
                    "MidiPlaybackEndpointFailed",
                    ErrorCategory.OpenError,
                    endpointDeviceId);

                return;
            }

            _player.SetSequenceAsync(sequence).GetAwaiter().GetResult();

            if (startMicroseconds > 0)
            {
                _player.SeekToMicroseconds(startMicroseconds);
            }

            _player.Play();

            WriteVerbose(Format(Strings.PlaybackStartedFormat, resolved, endpointDeviceId, Group + 1));

            if (NoWait.IsPresent)
            {
                // Ownership moves to the caller, so this cmdlet must not tear it down on the way
                // out.
                MidiFilePlayback handle;

                lock (_lock)
                {
                    handle = new MidiFilePlayback(_player, _session, resolved, endpointDeviceId);

                    _player = null;
                    _session = null;
                }

                WriteObject(handle);

                return;
            }

            WaitForPlayback(sequence, resolved);
        }

        private void WaitForPlayback(MidiSequence sequence, string resolvedPath)
        {
            var totalSeconds = sequence.DurationMicroseconds / 1000000.0;

            var progress = new ProgressRecord(0, Strings.PlaybackProgressActivity, System.IO.Path.GetFileName(resolvedPath));

            try
            {
                while (true)
                {
                    MidiSequencePlayerPosition position;

                    lock (_lock)
                    {
                        if (_player is null || _player.State != MidiSequencePlayerState.Playing)
                        {
                            break;
                        }

                        position = _player.Position;
                    }

                    if (Stopping)
                    {
                        WriteVerbose(Strings.PlaybackStoppedEarly);
                        break;
                    }

                    if (totalSeconds > 0)
                    {
                        var elapsed = position.Microseconds / 1000000.0;

                        progress.PercentComplete = (int)Math.Clamp(elapsed / totalSeconds * 100.0, 0, 100);
                        progress.SecondsRemaining = (int)Math.Max(0, totalSeconds - elapsed);
                        progress.StatusDescription = Format(
                            Strings.PlaybackProgressStatusFormat, position.Bar, position.Beat, position.BeatsPerMinute);

                        WriteProgress(progress);
                    }

                    System.Threading.Thread.Sleep(100);
                }
            }
            finally
            {
                progress.RecordType = ProgressRecordType.Completed;
                WriteProgress(progress);
            }
        }

        private string? ResolveFilePath()
        {
            try
            {
                var resolved = GetUnresolvedProviderPathFromPSPath(FilePath);

                if (!System.IO.File.Exists(resolved))
                {
                    WriteNonTerminating(
                        new System.IO.FileNotFoundException(Strings.PlaybackFileNotFound, resolved),
                        "MidiFileNotFound",
                        ErrorCategory.ObjectNotFound,
                        resolved);

                    return null;
                }

                return resolved;
            }
            catch (Exception ex)
            {
                WriteNonTerminating(ex, "MidiFilePathInvalid", ErrorCategory.InvalidArgument, FilePath);

                return null;
            }
        }

        private string? ResolveEndpointDeviceId()
        {
            if (!string.IsNullOrWhiteSpace(EndpointDeviceId))
            {
                return EndpointDeviceId;
            }

            if (!MidiSynthManager.IsTransportAvailable)
            {
                ThrowTerminating(
                    new InvalidOperationException(Strings.PlaybackNoEndpoint),
                    "MidiSynthUnavailable",
                    ErrorCategory.ResourceUnavailable);

                return null;
            }

            var synthEndpoint = MidiSynthManager.EndpointDeviceId;

            if (string.IsNullOrEmpty(synthEndpoint))
            {
                ThrowTerminating(
                    new InvalidOperationException(Strings.PlaybackSynthDisabled),
                    "MidiSynthDisabled",
                    ErrorCategory.ResourceUnavailable);

                return null;
            }

            return synthEndpoint;
        }

        private MidiSequence? ReadSequence(string path)
        {
            try
            {
                var file = global::Windows.Storage.StorageFile.GetFileFromPathAsync(path).GetAwaiter().GetResult();

                var result = MidiStandardFileReader.ReadFromFileAsync(file).GetAwaiter().GetResult();

                if (result is null || !result.Succeeded)
                {
                    WriteNonTerminating(
                        new InvalidOperationException(result is null
                            ? Strings.PlaybackFileUnreadable
                            : Format(Strings.PlaybackFileUnreadableFormat, result.Status)),
                        "MidiFileUnreadable",
                        ErrorCategory.InvalidData,
                        path);

                    return null;
                }

                if (result.Truncated)
                {
                    WriteWarning(Strings.PlaybackFileTruncated);
                }

                return result.Sequence;
            }
            catch (Exception ex)
            {
                WriteNonTerminating(ex, "MidiFileReadFailed", ErrorCategory.ReadError, path);

                return null;
            }
        }

        protected override void EndProcessing()
        {
            CleanUp();
        }

        protected override void StopProcessing()
        {
            // Ctrl+C. Silencing has to happen here or the last chord sounds forever.
            CleanUp();
        }

        private void CleanUp()
        {
            lock (_lock)
            {
                if (_player is not null)
                {
                    _player.Stop();
                    _player.Dispose();
                    _player = null;
                }

                if (_session is not null)
                {
                    _session.Dispose();
                    _session = null;
                }
            }
        }
    }
}
