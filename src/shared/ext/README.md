# Shared External Libraries

# midi_cpp2

**NOTE: this will be removed from this repo and made into a Github module or similar reference to the MIDI Association repo once that repo is open to the public**

This is a set of code from The MIDI Association, usable under their license terms which are compatible with the Windows MIDI Services repo license terms. Using this code helps Windows MIDI Services keep compatible with best practices for message and protocol conversion. Currently, we're working directly with source, but we may move to headers + lib in the future to be able to keep up with changes in the source repo.

* [MIDI CPP2 Repo](https://github.com/midi-mma/midi_cpp2)

Please see source for copyright information.

We're friends with Andrew and members of the MIDI Association, and want to make MIDI better for everyone, so if you find any bugs here, please be sure to also notify Andrew (starfishmod) through the MIDI Association repo, if at all possible.

## Building

On Windows, open the folder from explorer (shift-right-click, open with Visual Studio, or open Visual Studio and select "Open a local folder") to load the cmake project. Build from within Visual Studio.

This, and building, requires you have the CMake Tools for Windows workload installed with Visual Studio.

* [CMake projects in Visual Studio](https://learn.microsoft.com/cpp/build/cmake-projects-in-visual-studio)
