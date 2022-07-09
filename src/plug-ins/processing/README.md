# Processing plug-ins

These are plug-ins which process and translate messages, doing a lot of what the [MIDI Mapper](https://docs.microsoft.com/windows/win32/multimedia/the-midi-mapper) used to be (and sometimes still is) used for in Windows. Implementations may include:

Priority

* Change channel number / remap channels
* Change MIDI 2.0 group number / remap groups / functions
* Read note value, and change the channel or group based on a configured virtual split point/range
* CC controller number remapping
* Note remapping, for a drum machine, for example

Additional

* Note transposition
* Note quantizing to a configured key
* Velocity curve remapping
* Aftertouch curve remapping
* Convert UMP-packaged MIDI 1.0 to Native MIDI 2.0 messages and vv.
* Change MIDI 1.0 Note On with velocity zero to a MIDI 1.0 or MIDI 2.0 Note off message
* Patch map (program change remapping)
