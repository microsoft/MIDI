# Processing plug-ins

These are plug-ins which process and translate messages, doing a lot of what the old MIDI Mapper used to be used for in Windows. Implementations may include:

Top Priority

* Change channel number / remap channels
* Change MIDI 2.0 group number / remap groups / functions
* Read note value, and change the channel or group based on a configured virtual split point/range

Additional

* Note transposition
* Note remapping, for a drum machine, for example
* Note quantizing to a configured key
* Velocity curve remapping
* Aftertouch curve remapping
* Convert UMP-packaged MIDI 1.0 to Native MIDI 2.0 messages and vv.
* Change MIDI 1.0 Note On with velocity zero to a MIDI 1.0 or MIDI 2.0 Note off message
* Patch map (program change remapping)
