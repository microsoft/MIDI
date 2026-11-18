// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "LaunchpadXHardeningTests.h"

#include "LaunchpadXController.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <vector>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace midisequencer;
using namespace midisequencer::launchpad;

namespace
{
    constexpr uint8_t Group = 1;

    // Stands in for the device. Keeps count of what was sent, and checks every LED message only
    // names LEDs the device has.
    class CheckingOutput final : public ILaunchpadOutput
    {
    public:
        bool SendUmpPackets(_In_ std::span<const UmpPacket> packets) noexcept override
        {
            std::scoped_lock guard{ m_lock };

            for (auto const& packet : packets)
            {
                uint8_t group = 0;
                std::vector<uint8_t> payload{};

                if (packet.wordCount == 2 && m_accumulator.Feed(packet.words[0], packet.words[1], group, payload))
                {
                    CheckLeds(payload);
                }
            }

            return true;
        }

        bool AllLedsValid() const
        {
            std::scoped_lock guard{ m_lock };
            return m_allValid;
        }

    private:
        void CheckLeds(_In_ std::vector<uint8_t> const& payload) noexcept
        {
            if (payload.size() < 6 || payload[5] != SysExCommandLedLighting)
            {
                return;
            }

            for (size_t i = 6; i < payload.size();)
            {
                size_t length = 0;

                switch (payload[i])
                {
                case 0x00: length = 3; break;
                case 0x01: length = 4; break;
                case 0x02: length = 3; break;
                case 0x03: length = 5; break;
                default: m_allValid = false; return;
                }

                if (i + length > payload.size() || !IsLedIndex(payload[i + 1]))
                {
                    m_allValid = false;
                    return;
                }

                i += length;
            }
        }

        mutable std::mutex m_lock{};
        SysEx7Accumulator m_accumulator{};
        bool m_allValid{ true };
    };

    // The notes as the app hears them, in the order it hears them. Tracks how many times each note
    // is sounding, and whether a note was ever let go more often than it was played.
    class NoteLedger
    {
    public:
        void Hear(_In_ SurfaceAction const& action)
        {
            std::scoped_lock guard{ m_lock };

            ++m_heard;

            if (action.midiNote > 127)
            {
                Problem(L"note number out of range", action.midiNote);
                return;
            }

            if (action.kind == SurfaceActionKind::EmitNoteOn)
            {
                ++m_sounding[action.midiNote];
                ++m_played;
            }
            else if (action.kind == SurfaceActionKind::EmitNoteOff)
            {
                if (--m_sounding[action.midiNote] < 0)
                {
                    Problem(L"note let go before it was played", action.midiNote);
                }
            }
        }

        // No note left on, and none let go before it was played. Says what was wrong when not.
        bool Clean() const
        {
            std::scoped_lock guard{ m_lock };

            if (!m_problem.empty())
            {
                Log::Comment(m_problem);
                return false;
            }

            for (auto const& [note, count] : m_sounding)
            {
                if (count != 0)
                {
                    Log::Comment(String().Format(L"note %u left sounding %d times, of %zu notes played", note, count, m_played));
                    return false;
                }
            }

            return true;
        }

        size_t Played() const
        {
            std::scoped_lock guard{ m_lock };
            return m_played;
        }

    private:
        void Problem(_In_ wchar_t const* what, _In_ uint32_t note)
        {
            if (m_problem.empty())
            {
                m_problem = String().Format(L"%ls: note %u, message %zu heard", what, note, m_heard);
            }
        }

        mutable std::mutex m_lock{};
        std::map<uint8_t, int> m_sounding{};
        size_t m_played{ 0 };
        size_t m_heard{ 0 };
        std::wstring m_problem{};
    };

    uint32_t Midi1(_In_ uint8_t group, _In_ uint8_t status, _In_ uint8_t data1, _In_ uint8_t data2) noexcept
    {
        return 0x20000000u |
            (static_cast<uint32_t>(group & 0x0F) << 24) |
            (static_cast<uint32_t>(status & 0x0F) << 20) |
            (static_cast<uint32_t>(data1 & 0x7F) << 8) |
            static_cast<uint32_t>(data2 & 0x7F);
    }

    SurfaceStateSnapshot Sized(_In_ uint32_t tracks, _In_ uint32_t scenes, _In_ size_t clips)
    {
        SurfaceStateSnapshot snapshot{};
        snapshot.totalTracks = tracks;
        snapshot.totalScenes = scenes;
        snapshot.gridClips.resize(clips);

        for (size_t i = 0; i < clips; i += 3)
        {
            snapshot.gridClips[i].hasClip = true;
            snapshot.gridClips[i].playing = (i % 2) == 0;
        }

        return snapshot;
    }

    uint8_t RandomPad(_In_ std::mt19937& random)
    {
        std::uniform_int_distribution<int> cell{ 0, 7 };
        return GridPadIndex(static_cast<uint8_t>(cell(random)), static_cast<uint8_t>(cell(random)));
    }

    // Presses and lets go of pads in Note mode as fast as it can, with pressure now and then, until
    // told to stop. Says when it's done.
    void HammerPads(_In_ LaunchpadXController& controller, _In_ std::atomic<bool> const& stop, _Inout_ std::atomic<bool>& done, _In_ uint32_t seed)
    {
        std::mt19937 random{ seed };
        std::vector<uint8_t> held{};

        for (int i = 0; i < 20000 && !stop.load(); ++i)
        {
            uint32_t word = 0;

            if (held.empty() || random() % 3 == 0)
            {
                auto const pad = RandomPad(random);
                held.push_back(pad);
                word = Midi1(Group, 0x9, pad, static_cast<uint8_t>(1 + random() % 127));
            }
            else if (random() % 4 == 0)
            {
                word = Midi1(Group, 0xA, held[random() % held.size()], static_cast<uint8_t>(random() % 128));
            }
            else
            {
                auto const at = random() % held.size();
                word = Midi1(Group, 0x9, held[at], 0);
                held.erase(held.begin() + static_cast<std::ptrdiff_t>(at));
            }

            controller.OnInput(&word, 1, 1000 + static_cast<uint64_t>(i));
        }

        done = true;
    }

    // Holds the note handler up now and then, as a busy service thread would, so a message sent
    // later can try to overtake it.
    void Dawdle(_In_ SurfaceAction const& action)
    {
        if (action.kind == SurfaceActionKind::EmitNoteOn && (action.midiNote % 4) == 0)
        {
            std::this_thread::sleep_for(std::chrono::microseconds{ 20 });
        }
        else
        {
            std::this_thread::yield();
        }
    }
}

void LaunchpadXHardeningTests::RandomMessagesNeverAskForSomethingOutOfRange()
{
    auto const output = std::make_shared<CheckingOutput>();
    NoteLedger ledger{};
    SurfaceStateSnapshot snapshot = Sized(5, 11, 55);
    bool inRange = true;
    size_t actionCount = 0;

    LaunchpadXController controller{
        output,
        Group,
        [&ledger](SurfaceAction const& action, uint64_t) { ledger.Hear(action); },
        [&](std::vector<SurfaceAction> actions)
        {
            for (auto const& action : actions)
            {
                ++actionCount;

                switch (action.kind)
                {
                case SurfaceActionKind::TriggerOrStopSlot:
                    inRange = inRange && action.trackIndex < snapshot.totalTracks && action.sceneIndex < snapshot.totalScenes;
                    break;
                case SurfaceActionKind::LaunchScene:
                    inRange = inRange && action.sceneIndex < snapshot.totalScenes;
                    break;
                case SurfaceActionKind::SelectTrack:
                    inRange = inRange && action.trackIndex < snapshot.totalTracks;
                    break;
                case SurfaceActionKind::SetViewport:
                    inRange = inRange && (action.trackIndex == 0 || action.trackIndex < snapshot.totalTracks) &&
                        (action.sceneIndex == 0 || action.sceneIndex < snapshot.totalScenes);
                    break;
                default:
                    break;
                }
            }
        } };

    VERIFY_IS_TRUE(controller.Start(snapshot));

    std::mt19937 random{ 20261118u };

    for (int i = 0; i < 50000; ++i)
    {
        std::array<uint32_t, 4> words{};
        auto const pick = random() % 10;

        if (pick < 4)
        {
            // Channel voice messages the device could send, and some it couldn't, on its group.
            static constexpr uint8_t Statuses[]{ 0x8, 0x9, 0xA, 0xB, 0xC, 0xD, 0xE };
            words[0] = Midi1(Group, Statuses[random() % std::size(Statuses)], static_cast<uint8_t>(random()), static_cast<uint8_t>(random()));
        }
        else if (pick < 6)
        {
            // MIDI 2.0 messages, some with the wrong number of words.
            words[0] = 0x40000000u | (static_cast<uint32_t>(Group) << 24) | (random() & 0x00FFFFFFu);
            words[1] = random();
        }
        else if (pick < 8)
        {
            // SysEx fragments in any order, some with bad lengths or high bits.
            words[0] = 0x30000000u | (static_cast<uint32_t>(Group) << 24) | (random() & 0x00FFFFFFu);
            words[1] = random();
        }
        else
        {
            for (auto& word : words)
            {
                word = random();
            }
        }

        controller.OnInput(words.data(), static_cast<uint8_t>(1 + random() % 4), static_cast<uint64_t>(i));

        // The sequence changes under the pads now and then, smaller and bigger.
        if (random() % 500 == 0)
        {
            snapshot = Sized(random() % 20, random() % 20, random() % 400);
            controller.Update(snapshot);
        }
    }

    controller.Stop();

    Log::Comment(String().Format(L"%zu actions, %zu notes played", actionCount, ledger.Played()));
    VERIFY_IS_TRUE(actionCount > 0);
    VERIFY_IS_TRUE(inRange);
    VERIFY_IS_TRUE(ledger.Clean());
    VERIFY_IS_TRUE(output->AllLedsValid());
}

void LaunchpadXHardeningTests::BrokenSysExNeverGrowsPastItsLimitOrBlocksTheNextMessage()
{
    SysEx7Accumulator accumulator{};
    std::mt19937 random{ 7u };
    uint8_t group = 0;
    std::vector<uint8_t> payload{};

    // Random fragments on random groups: whatever comes out is within the limit, and 7-bit.
    for (int i = 0; i < 100000; ++i)
    {
        auto const word0 = 0x30000000u | (random() & 0x0FFFFFFFu);

        if (accumulator.Feed(word0, random(), group, payload))
        {
            VERIFY_IS_TRUE(payload.size() <= SysEx7Accumulator::MaxPayloadBytes);

            for (auto const byte : payload)
            {
                VERIFY_IS_TRUE(byte < 0x80);
            }
        }
    }

    // A message that starts and never ends stops growing at the limit...
    auto const start = PacketizeSysEx7(Group, std::vector<uint8_t>(12, 0x11));
    VERIFY_IS_FALSE(accumulator.Feed(start.front().words[0], start.front().words[1], group, payload));

    auto const continued = 0x30000000u | (static_cast<uint32_t>(Group) << 24) | (0x2u << 20) | (6u << 16) | 0x2222u;

    for (size_t i = 0; i < SysEx7Accumulator::MaxPayloadBytes; ++i)
    {
        VERIFY_IS_FALSE(accumulator.Feed(continued, 0x22222222u, group, payload));
    }

    // ...and a whole message after it still comes through as sent.
    std::vector<uint8_t> const message{ 0x7E, 0x00, 0x06, 0x02, 0x00, 0x20, 0x29, 0x13, 0x01, 0x00, 0x00, 0x00, 0x04, 0x05, 0x01 };
    auto const packets = PacketizeSysEx7(Group, message);
    bool done = false;

    for (auto const& packet : packets)
    {
        done = accumulator.Feed(packet.words[0], packet.words[1], group, payload);
    }

    VERIFY_IS_TRUE(done);
    VERIFY_ARE_EQUAL(Group, group);
    VERIFY_IS_TRUE(payload == message);
}

void LaunchpadXHardeningTests::EmptyAndHugeSequencesStillPaintEveryLed()
{
    struct Case
    {
        uint32_t Tracks;
        uint32_t Scenes;
        size_t Clips;
        uint32_t SelectedTrack;
        uint32_t SelectedScene;
    };

    // Totals that don't match the clips given, and numbers near the top of uint32_t.
    Case const cases[]{
        { 0, 0, 0, 0, 0 },
        { 0, 0, 64, 5, 5 },
        { 1, 1, 0, UINT32_MAX, UINT32_MAX },
        { 8, 8, 3, 7, 7 },
        { UINT32_MAX, UINT32_MAX, 16, UINT32_MAX - 1, 0 },
        { UINT32_MAX, 2, 0, UINT32_MAX, 1 },
        { 3, UINT32_MAX, 9, 2, UINT32_MAX },
    };

    std::vector<uint8_t> const presses{ 11, 18, 44, 81, 88, 19, 89, ButtonUp, ButtonDown, ButtonLeft, ButtonRight, ButtonSession, ButtonNote, ButtonCustom, ButtonRecord };

    for (auto const& c : cases)
    {
        for (auto const mode : { ControllerMode::Clip, ControllerMode::Note })
        {
            auto snapshot = Sized(c.Tracks, c.Scenes, c.Clips);
            snapshot.selectedTrackIndex = c.SelectedTrack;
            snapshot.selectedSceneIndex = c.SelectedScene;

            LaunchpadXSurface surface{};
            std::vector<SurfaceAction> actions{};
            surface.SetMode(mode, actions);

            for (auto const offset : { 0u, 7u, UINT32_MAX - 7u, UINT32_MAX })
            {
                surface.SetViewport(offset, offset);

                auto const leds = surface.BuildLedFeedback(snapshot, true);
                VERIFY_ARE_EQUAL(size_t{ 81 }, leds.size());

                for (auto const& led : leds)
                {
                    VERIFY_IS_TRUE(IsLedIndex(led.index));
                }

                for (auto const index : presses)
                {
                    auto const kind = IsGridPadIndex(index) ? InputEventKind::PadPress : InputEventKind::ButtonPress;
                    actions.clear();
                    surface.HandleInput(InputEvent{ kind, index, 100, false, {} }, snapshot, actions);

                    for (auto const& action : actions)
                    {
                        if (action.kind == SurfaceActionKind::TriggerOrStopSlot)
                        {
                            VERIFY_IS_TRUE(action.trackIndex < snapshot.totalTracks && action.sceneIndex < snapshot.totalScenes);
                        }
                        else if (action.kind == SurfaceActionKind::LaunchScene)
                        {
                            VERIFY_IS_TRUE(action.sceneIndex < snapshot.totalScenes);
                        }
                        else if (action.kind == SurfaceActionKind::SelectTrack)
                        {
                            VERIFY_IS_TRUE(action.trackIndex < snapshot.totalTracks);
                        }
                    }

                    // Let go of whatever Note mode pressed, so the next press starts clean.
                    actions.clear();
                    surface.HandleInput(InputEvent{ kind == InputEventKind::PadPress ? InputEventKind::PadRelease : InputEventKind::ButtonRelease, index, 0, false, {} }, snapshot, actions);
                }
            }
        }
    }
}

void LaunchpadXHardeningTests::ModeChangesRacingPadsNeverLeaveANoteOn()
{
    auto const output = std::make_shared<CheckingOutput>();
    NoteLedger ledger{};

    LaunchpadXController controller{
        output,
        Group,
        [&ledger](SurfaceAction const& action, uint64_t)
        {
            Dawdle(action);
            ledger.Hear(action);
        },
        [](std::vector<SurfaceAction>) {} };

    controller.SetMode(ControllerMode::Note);
    VERIFY_IS_TRUE(controller.Start(Sized(4, 4, 16)));

    // The device's thread plays pads while the app's thread moves the key and switches mode, as
    // it does when the playhead reaches a key change or someone presses the mode button.
    std::atomic<bool> stop{ false };
    std::atomic<bool> done{ false };
    std::thread pads{ [&]() { HammerPads(controller, stop, done, 11u); } };

    std::mt19937 random{ 12u };

    // For as long as the pads are being played, however long the thread takes to start.
    while (!done)
    {
        if (random() % 5 == 0)
        {
            controller.SetMode(ControllerMode::Clip);
            controller.SetMode(ControllerMode::Note);
        }
        else
        {
            NoteLayoutConfig config{};
            config.rootNote = static_cast<uint8_t>(random() % 12);
            config.inScaleOnly = (random() % 2) == 0;
            controller.SetNoteConfig(config);
        }

        std::this_thread::yield();
    }

    pads.join();
    controller.Stop();

    Log::Comment(String().Format(L"%zu notes played", ledger.Played()));
    VERIFY_IS_TRUE(ledger.Played() > 0);
    VERIFY_IS_TRUE(ledger.Clean());
}

void LaunchpadXHardeningTests::StopRacingPadsNeverLeavesANoteOn()
{
    // Unplugging, or turning the Launchpad X off, while pads are still coming in.
    for (uint32_t round = 0; round < 40; ++round)
    {
        auto const output = std::make_shared<CheckingOutput>();
        NoteLedger ledger{};

        LaunchpadXController controller{
            output,
            Group,
            [&ledger](SurfaceAction const& action, uint64_t)
            {
                Dawdle(action);
                ledger.Hear(action);
            },
            [](std::vector<SurfaceAction>) {} };

        controller.SetMode(ControllerMode::Note);
        VERIFY_IS_TRUE(controller.Start(Sized(2, 2, 4)));

        std::atomic<bool> stop{ false };
        std::atomic<bool> done{ false };
        std::thread pads{ [&]() { HammerPads(controller, stop, done, 100u + round); } };

        std::this_thread::sleep_for(std::chrono::microseconds{ 200 + 50 * round });
        controller.Stop();

        stop = true;
        pads.join();

        VERIFY_IS_TRUE(ledger.Clean());
    }
}
