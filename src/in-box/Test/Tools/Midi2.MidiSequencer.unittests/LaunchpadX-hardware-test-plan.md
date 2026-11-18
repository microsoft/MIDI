# Launchpad X in MIDI Sequencer: hardware test plan

The unit tests run the Launchpad X code with a fake device. This plan covers what only a real Novation Launchpad X can show: the port the Sequencer picks, what the device lights, and what a synth hears.

## What you need

- A Novation Launchpad X on current firmware, on USB.
- Windows MIDI Services turned on. `midi service status` should say it's running. The Sequencer doesn't work in Legacy API mode, so that mode is out of scope.
- A synth or MIDI monitor to hear or see what the Sequencer sends. A software synth on a loopback endpoint works. Note whether it stacks voices for a repeated note, since step 14 depends on that.
- A sequence with at least 9 tracks and 9 scenes, some clips, one armed track, and a key change partway through (Key and chords row).

## Axes

- **API path:** the Sequencer uses only the native UMP path through the Windows MIDI Services SDK. WinMM and WinRT MIDI 1.0 aren't used, so they aren't tested. Step 20 checks that another app loses the device while the Sequencer has it.
- **Driver:** run the whole plan once with the in-box USB MIDI 2.0 driver (the device shows as one endpoint with LPX DAW and LPX MIDI as groups) and once with the older USB MIDI 1.0 class driver (each port may show as its own endpoint). Record which driver each run used and the endpoint names `midi endpoint list` shows.
- **API mode:** Windows MIDI Services mode only (`midi api-mode get`). Record it.

## Steps

Record pass or fail for each step, with the driver used.

1. **Off by default.** Start the Sequencer with the Launchpad X plugged in. The device stays in its own Session layout, and the status bar has no Launchpad X button.
2. **Turn it on.** File > Launchpad X…, turn on "Use a Launchpad X". The status bar says "Launchpad X is on:" and the device name, and reopening the dialog says "Connected to" it. The device switches to Programmer mode, and all 81 LEDs light to match the launcher. The status bar shows "Launchpad X: Clip mode" (or Note mode, if that's how it was left).
3. **Right port.** In a MIDI monitor on the device, the Sequencer only talks to the LPX MIDI port, never LPX DAW.
4. **Clip pads.** Press a pad on a clip. The clip launches on the next launch boundary, its pad flashes amber while queued, then pulses green while it plays. Press it again and it stops.
5. **Record into a slot.** On an armed track, press an empty pad. It flashes red and records. Press it again to finish, and the new clip lights in the track's color.
6. **Scenes.** Press the top right-hand button. Scene 1 launches. Press the bottom one. Scene 8 launches.
7. **Moving the view.** Press Down and Right. The pads move by one scene and one track, and the launcher scrolls with them. Up and Left light only when there's somewhere to go. At the last track and scene, Down and Right go dark.
8. **Stop and record.** Custom stops playback. Capture MIDI starts recording, and pressing it again stops it.
9. **Note mode from the device.** Press Note. The pads relight as a keyboard in the song's key, with roots brightest. The status bar changes to "Launchpad X: Note mode".
10. **Playing.** Press pads on the selected track. The synth plays each note right away, with velocity. Pressure on a held pad sends polyphonic aftertouch. Letting go sends a note-off for every note.
11. **Octave and track.** Up and Down move by an octave, and stop at the top and bottom. Left and Right pick the previous and next track, and the notes go to that track's synth.
12. **Following the key.** Play across the key change. The pad colors change at the key change. A pad held across it gets its note-off.
13. **Picking a key.** In the dialog, turn off "Follow the song's key" and pick D Dorian. The pads relight for D Dorian straight away. Turn on "Only notes in the key". Every lit pad plays a D Dorian note. Close and reopen the Sequencer: the choices are kept.
14. **Shared notes.** With "Only notes in the key" off, hold pad 16, then press pad 21. They play the same note, and the note starts over. Let go of both. No note is left sounding, including on a synth that stacks voices.
15. **Mode button.** Click the status bar button. The device switches mode and relights. Any held notes stop.
16. **Recording pads.** Arm the selected track, start recording, and play pads. The notes land in the take at the times played, and play back the same.
17. **No double notes.** Set the armed track's input to the Launchpad X itself. Pads play once, not twice.
18. **Unplug and plug in.** Unplug the device while holding pads. The synth gets a note-off for each held note. The status bar button goes away. Plug it back in. It reconnects on its own, and all 81 LEDs repaint.
19. **Turn it off.** Turn off "Use a Launchpad X". The status bar says "Launchpad X is off.", and the device goes back to its own Session layout.
20. **Another app.** With the feature on, open another app that uses the Launchpad X. Note what it sees. Turn the feature off, and check the other app can use the device again.
21. **Closing.** With the feature on and pads held, close the Sequencer. Held notes stop, and the device leaves Programmer mode.

## Not covered

- Novation Components, firmware updates, and the device's own Custom modes.
- A second Launchpad X at the same time. The Sequencer keeps the one picked last, so this needs a separate check if anyone relies on it.
