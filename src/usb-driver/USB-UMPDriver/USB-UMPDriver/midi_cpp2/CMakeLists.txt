cmake_minimum_required(VERSION 3.12)

project(libmidi2 CXX)
set(CMAKE_CXX_STANDARD 17)

add_library(midi2
        src/utils.cpp
        src/bytestreamToUMP.cpp
        src/umpToBytestream.cpp
        src/umpMessageCreate.cpp
        src/midiCIMessageCreate.cpp
        src/midiCIProcessor.cpp
        src/umpProcessor.cpp
        src/mcoded7.cpp
        )


