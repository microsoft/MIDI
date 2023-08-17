// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"

#include "catch_amalgamated.hpp"

#include "..\api-core\ump_helpers.h"

#include <iostream>
#include <algorithm>

using namespace Windows::Devices::Midi2::Internal;


TEST_CASE("Offline.Ump.Helpers UMP Helpers")
{
    SECTION("Test Internal GetUmpLengthInMidiWordsFromMessageType")
    {

        std::uint8_t result;

        // just spot-check a few

        result = GetUmpLengthInMidiWordsFromMessageType(1);
        REQUIRE(result == 1);

        result = GetUmpLengthInMidiWordsFromMessageType(2);
        REQUIRE(result == 1);

        result = GetUmpLengthInMidiWordsFromMessageType(10);
        REQUIRE(result == 2);

        result = GetUmpLengthInMidiWordsFromMessageType(15);
        REQUIRE(result == 4);
    }


    SECTION("Test Internal GetUmpMessageTypeFromFirstWord")
    {
        std::uint8_t result;

        std::uint32_t word;

        word = 0xF0000000;
        result = GetUmpMessageTypeFromFirstWord(word);
        REQUIRE(result == 0xF);

        word = 0xA0000000;
        result = GetUmpMessageTypeFromFirstWord(word);
        REQUIRE(result == 0xA);

        word = 0x10000000;
        result = GetUmpMessageTypeFromFirstWord(word);
        REQUIRE(result == 0x1);

        word = 0x00000000;
        result = GetUmpMessageTypeFromFirstWord(word);
        REQUIRE(result == 0x0);

        word = 0xF8675309;
        result = GetUmpMessageTypeFromFirstWord(word);
        REQUIRE(result == 0xF);

        word = 0x08675309;
        result = GetUmpMessageTypeFromFirstWord(word);
        REQUIRE(result == 0x0);
    }


    SECTION("Test Internal SetUmpMessageType")
    {
        std::uint32_t word;

        // test setting otherwise empty word with lower value
        word = 0xF0000000;
        SetUmpMessageType(word, 0x0A);
        REQUIRE(word == 0xA0000000);

        // test setting empty word
        word = 0x00000000;
        SetUmpMessageType(word, 0x01);
        REQUIRE(word == 0x10000000);

        // test set to lower when all bits already set
        word = 0xF0000000;
        SetUmpMessageType(word, 0x02);
        REQUIRE(word == 0x20000000);

        // test set to F when already set lower
        word = 0x98765432;
        SetUmpMessageType(word, 0x0F);
        REQUIRE(word == 0xF8765432);

        // test set to 0 when already 0
        word = 0x08765432;
        SetUmpMessageType(word, 0x00);
        REQUIRE(word == 0x08765432);

        // test set to 0 when already set > 0
        word = 0xA8675309;
        SetUmpMessageType(word, 0x00);
        REQUIRE(word == 0x08675309);
    }






}
