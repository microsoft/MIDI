// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

// Windows MIDI Services sample code
//
// FOCUS OF THIS SAMPLE: reading a Standard MIDI File and playing it, and setting
// the volume of the General MIDI synthesizer while it plays.
//
// MidiStandardFileReader turns a .mid file into a MidiSequence, with its tracks,
// tempo changes, meter, text and notes. MidiSequencePlayer plays the sequence.
// Under WinMM, the MCI sequencer could play a file, but it did not show you
// what was in it.
//
// The synthesizer has a volume of its own, set with MidiSynthConfig. It is
// separate from the volume messages in a file, so a file that resets the
// synthesizer cannot undo it, and changing it does not interrupt what is
// playing. The synthesizer is shared by every application on the PC, so this
// sample puts the volume back the way it found it before it exits.
//
// Pass the path of a .mid file on the command line. Without one, this plays a
// file that comes with Windows.

#include <windows.h>
#include <conio.h>
#include <io.h>
#include <fcntl.h>

#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <filesystem>
#include <atomic>
#include <thread>
#include <chrono>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Storage.h>

#include <winrt/Windows.Devices.Midi2.h>
#include <winrt/Windows.Devices.Midi2.ServiceConfig.h>
#include <winrt/Windows.Devices.Midi2.Transports.Synth.h>
#include <winrt/Windows.Devices.Midi2.Utilities.Sequencing.h>
#include <winrt/Windows.Devices.Midi2.Utilities.Files.h>

using namespace winrt::Windows::Devices::Midi2;
using namespace winrt::Windows::Devices::Midi2::ServiceConfig;
using namespace winrt::Windows::Devices::Midi2::Transports::Synth;
using namespace winrt::Windows::Devices::Midi2::Utilities::Sequencing;
using namespace winrt::Windows::Devices::Midi2::Utilities::Files;

using namespace winrt::Windows::Storage;


// The endpoint to play on. Leave empty to use the General MIDI synthesizer. The
// volume keys only work on the synthesizer.
const winrt::hstring DestinationEndpointId = L"";

// The synthesizer's volume while this sample plays, and how far each key press
// moves it. The service keeps the volume inside the range the synthesizer
// supports, which is -60 to +12 decibels today.
constexpr double StartingVolumeDecibels = -6.0;
constexpr double VolumeStepDecibels = 3.0;

constexpr int EscapeKey = 27;

std::atomic<bool> g_stopRequested{ false };


// Ctrl+C stops playback the same way Esc does, so the volume still gets put back.
BOOL WINAPI OnConsoleControl(DWORD const controlType)
{
    if (controlType == CTRL_C_EVENT || controlType == CTRL_BREAK_EVENT)
    {
        g_stopRequested = true;
        return TRUE;
    }

    return FALSE;
}

std::filesystem::path DefaultFilePath()
{
    wchar_t windowsFolder[MAX_PATH]{};

    GetWindowsDirectoryW(windowsFolder, MAX_PATH);

    return std::filesystem::path{ windowsFolder } / L"Media" / L"flourish.mid";
}

std::wstring_view ReadStatusText(MidiFileReadStatus const status)
{
    switch (status)
    {
    case MidiFileReadStatus::NotAMidiFile: return L"this is not a MIDI file";
    case MidiFileReadStatus::UnsupportedFormat: return L"this kind of MIDI file is not supported";
    case MidiFileReadStatus::CorruptData: return L"the file is damaged";
    case MidiFileReadStatus::NoPlayableData: return L"the file has nothing in it to play";
    case MidiFileReadStatus::TooLarge: return L"the file is larger than the limits in MidiFileReadOptions";
    case MidiFileReadStatus::ReadError: return L"the file could not be read";
    default: return L"unknown status";
    }
}

std::wstring MinutesAndSeconds(uint64_t const microseconds)
{
    auto const seconds = microseconds / 1'000'000;

    std::wostringstream text;
    text << seconds / 60 << L":" << std::setw(2) << std::setfill(L'0') << seconds % 60;

    return text.str();
}

// Bit zero is channel 1.
std::wstring ChannelNumbers(uint16_t const channelMask)
{
    std::wstring text;

    for (int channel = 0; channel < 16; channel++)
    {
        if ((channelMask & (1 << channel)) != 0)
        {
            text += text.empty() ? L"" : L", ";
            text += std::to_wstring(channel + 1);
        }
    }

    return text;
}

void PrintSequence(MidiFileReadResult const& result)
{
    auto const sequence = result.Sequence();

    // Title and copyright come from text in the file, and many files have neither.
    if (!sequence.Title().empty())
    {
        std::wcout << L"  Title      " << sequence.Title().c_str() << std::endl;
    }

    if (!sequence.Copyright().empty())
    {
        std::wcout << L"  Copyright  " << sequence.Copyright().c_str() << std::endl;
    }

    std::wcout << L"  Length     " << MinutesAndSeconds(sequence.DurationMicroseconds()) << std::endl;

    // A file does not have one tempo. It has a map of them, and many files change
    // tempo as they go, so show where it starts and how often it changes.
    auto const tempoMap = sequence.TempoMap();

    std::wcout << L"  Tempo      " << tempoMap.GetAt(0).BeatsPerMinute << L" BPM at the start";

    if (tempoMap.Size() > 1)
    {
        std::wcout << L", changes after that: " << tempoMap.Size() - 1;
    }

    std::wcout << std::endl;

    auto const meter = sequence.TimeSignatureMap().GetAt(0);

    std::wcout << L"  Meter      " << static_cast<int>(meter.Numerator) << L"/" << static_cast<int>(meter.Denominator) << L" at the start" << std::endl;
    std::wcout << L"  Notes      " << sequence.NoteCount() << std::endl;

    if (sequence.IsKaraoke())
    {
        std::wcout << L"  Karaoke    lines of words: " << sequence.LyricLines().Size() << std::endl;
    }

    // A file that says it has more tracks than it holds is common enough to be
    // worth reporting.
    std::wcout << L"  Tracks     " << result.ReadTrackCount();

    if (result.DeclaredTrackCount() != result.ReadTrackCount())
    {
        std::wcout << L" (the file says " << result.DeclaredTrackCount() << L")";
    }

    std::wcout << std::endl;

    std::wcout << L"     #  " << std::left << std::setw(24) << L"Name" << std::right << std::setw(6) << L"Notes" << L"  Channels" << std::endl;

    for (auto const& track : sequence.Tracks())
    {
        // A track with no notes is usually the one that holds the tempo map.
        if (track.NoteCount() == 0)
        {
            continue;
        }

        std::wcout << L"    " << std::setw(2) << track.TrackIndex() + 1 << L"  "
            << std::left << std::setw(24) << (track.Name().empty() ? L"(no name)" : track.Name().c_str()) << std::right
            << std::setw(6) << track.NoteCount() << L"  " << ChannelNumbers(track.UsedChannelMask()) << std::endl;
    }
}

// Sets the synthesizer's volume, and returns the volume it really took.
double SetSynthVolume(double const decibels)
{
    // Start from the current settings and change only the volume. A config holds
    // every setting, so one made from nothing would put all of the others back
    // to their defaults.
    MidiSynthConfig config{ MidiSynthManager::GetStatus() };

    config.VolumeDecibels(decibels);

    // SendUpdate changes the running service and saves nothing, so the
    // customer's own setting comes back when the service restarts. Call
    // SaveUpdate as well only when the customer asked for the change to stay.
    auto const response = MidiServiceTransportPluginConfigManager::SendUpdate(config);

    if (response.Status() != MidiServiceConfigResponseStatus::Success)
    {
        std::wcout << std::endl << L"The service did not accept the volume change." << std::endl;
    }

    // A value out of range is moved to the nearest limit rather than refused, so
    // read back what was used.
    auto const status = MidiSynthManager::GetStatus();

    return status == nullptr ? decibels : status.VolumeDecibels();
}


int wmain(int argc, wchar_t* argv[])
{
    winrt::init_apartment();

    // Names and text come from the file, in any language. Writing UTF-16 straight
    // to the console shows all of them, where the default narrow conversion stops
    // printing anything at the first character it cannot convert.
    (void)_setmode(_fileno(stdout), _O_U16TEXT);

    std::wcout << std::fixed << std::setprecision(1);

    // GetFileFromPathAsync needs a full path.
    auto const path = std::filesystem::absolute(argc > 1 ? std::filesystem::path{ argv[1] } : DefaultFilePath());

    std::wcout << L"Reading " << path.c_str() << std::endl;

    StorageFile file{ nullptr };

    try
    {
        file = StorageFile::GetFileFromPathAsync(path.wstring()).get();
    }
    catch (winrt::hresult_error const& ex)
    {
        std::wcout << L"Could not open the file: " << ex.message().c_str() << std::endl;
        return 1;
    }

    // A MIDI file usually comes from somewhere you do not control, so the reader
    // checks every length a file gives it against the bytes that are really
    // there. MidiFileReadOptions sets the limits, if the defaults do not suit.
    auto const result = MidiStandardFileReader::ReadFromFileAsync(file).get();

    if (!result.Succeeded())
    {
        std::wcout << L"Could not read the file: " << ReadStatusText(result.Status()) << std::endl;
        return 1;
    }

    // A file that ends early, or stops making sense partway through, is read up
    // to that point, and what was read still plays.
    if (result.Truncated())
    {
        std::wcout << L"  The file ends early. Playing what could be read." << std::endl;
    }

    PrintSequence(result);

    if (!MidiApi::EnsureServiceAvailable())
    {
        std::wcout << L"Could not demand-start the MIDI service." << std::endl;
        return 1;
    }

    // The synthesizer has exactly one endpoint, so it can be named without
    // enumerating. The id is empty when the synthesizer is switched off.
    auto const playingOnSynth = DestinationEndpointId.empty();

    auto const endpointId = playingOnSynth ? MidiSynthManager::EndpointDeviceId() : DestinationEndpointId;

    if (endpointId.empty())
    {
        std::wcout << L"The General MIDI synthesizer is not available. It may be switched off in MIDI Settings." << std::endl;
        return 1;
    }

    auto session = MidiSession::Create(L"MIDI File Player Sample");

    auto connection = session.CreateEndpointConnection(endpointId);

    if (connection == nullptr || !connection.Open())
    {
        std::wcout << L"Could not open a connection to " << endpointId.c_str() << std::endl;
        return 1;
    }

    auto const synthStatus = MidiSynthManager::GetStatus();
    auto const controlVolume = playingOnSynth && synthStatus != nullptr;

    SetConsoleCtrlHandler(OnConsoleControl, TRUE);

    double const originalVolume = controlVolume ? synthStatus.VolumeDecibels() : 0.0;
    double volume = originalVolume;

    if (controlVolume)
    {
        volume = SetSynthVolume(StartingVolumeDecibels);

        std::wcout << std::endl << L"Synthesizer volume set to " << volume
            << L" dB. It was " << originalVolume << L" dB." << std::endl;
    }

    // Declared before the player, so it outlives every event the player raises.
    std::atomic<bool> ended{ false };

    // The player borrows the connection and never closes it.
    MidiSequencePlayer player{ connection, MidiGroup{ static_cast<uint8_t>(0) } };

    // Raised on the player's own thread when the sequence reaches its end. Keep
    // the handler short, and do not call back into the player from it.
    player.PlaybackEnded([&ended](auto&&, auto&&) { ended = true; });

    // Preparing converts the whole sequence once, so this is the slow call and
    // Play is not.
    player.SetSequenceAsync(result.Sequence()).get();

    std::wcout << std::endl << L"Space pauses and resumes. ";

    if (controlVolume)
    {
        std::wcout << L"+ and - change the volume. ";
    }

    std::wcout << L"Esc stops." << std::endl << std::endl;

    player.Play();

    while (!ended)
    {
        if (_kbhit())
        {
            auto const key = _getch();

            if (key == EscapeKey)
            {
                g_stopRequested = true;
            }
            else if (key == ' ')
            {
                // Pausing silences what is sounding. Playing again sends each
                // channel's sound and controllers first, so it resumes correctly.
                if (player.State() == MidiSequencePlayerState::Playing)
                {
                    player.Pause();
                }
                else
                {
                    player.Play();
                }
            }
            else if (controlVolume && (key == '+' || key == '='))
            {
                volume = SetSynthVolume(volume + VolumeStepDecibels);
            }
            else if (controlVolume && key == '-')
            {
                volume = SetSynthVolume(volume - VolumeStepDecibels);
            }
        }

        if (g_stopRequested)
        {
            // Stopping silences what is sounding. It does not raise PlaybackEnded.
            player.Stop();
            break;
        }

        // There is no position event, on purpose. Read the position as often as
        // your display needs it: an event for every frame would cost more.
        auto const position = player.Position();

        std::wcout << L"\r  bar " << std::setw(3) << position.Bar << L" beat " << position.Beat
            << L"   " << MinutesAndSeconds(position.Microseconds) << L" / " << MinutesAndSeconds(position.DurationMicroseconds)
            << L"   " << std::setw(5) << position.BeatsPerMinute << L" BPM";

        if (controlVolume)
        {
            std::wcout << L"   volume " << std::showpos << volume << std::noshowpos << L" dB";
        }

        std::wcout << (position.State == MidiSequencePlayerState::Paused ? L"   paused" : L"         ") << std::flush;

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::wcout << std::endl << std::endl;

    // Closing the player leaves the borrowed connection open.
    player.Close();

    if (controlVolume)
    {
        volume = SetSynthVolume(originalVolume);

        std::wcout << L"Synthesizer volume put back to " << volume << L" dB." << std::endl;
    }

    session.Close();

    return 0;
}
