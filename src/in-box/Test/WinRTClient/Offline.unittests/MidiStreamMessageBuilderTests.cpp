// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "stdafx.h"


// A device identity is carried by four different messages with three different encodings, and the
// builder and the parser used to be independent hand-written layouts with nothing holding them
// together. These tests are what holds them together.

void MidiStreamMessageBuilderTests::TestDeviceIdentityNotificationRoundTrip()
{
    // The values Windows MIDI Services itself declares.
    const uint8_t sysExId1{ 0x00 };
    const uint8_t sysExId2{ 0x00 };
    const uint8_t sysExId3{ 0x41 };

    const uint8_t familyLsb{ 11 };
    const uint8_t familyMsb{ 0 };

    const uint8_t modelLsb{ 1 };
    const uint8_t modelMsb{ 0 };

    const uint8_t revision1{ 1 };
    const uint8_t revision2{ 0 };
    const uint8_t revision3{ 0 };
    const uint8_t revision4{ 0 };

    auto message = MidiStreamMessageBuilder::BuildDeviceIdentityNotificationMessage(
        0,
        sysExId1, sysExId2, sysExId3,
        familyLsb, familyMsb,
        modelLsb, modelMsb,
        revision1, revision2, revision3, revision4);

    VERIFY_IS_NOT_NULL(message);

    // The round trip below shares one implementation, so on its own it would also pass if the
    // builder and the parser were wrong in the same way. These are the words the UMP specification
    // calls for, worked out by hand.
    auto words = message.as<MidiMessage128>();

    VERIFY_ARE_EQUAL(words.Word0(), (uint32_t)0xF0020000);   // stream message, form 0, status 0x02
    VERIFY_ARE_EQUAL(words.Word1(), (uint32_t)0x00000041);   // reserved byte, then 00 00 41
    VERIFY_ARE_EQUAL(words.Word2(), (uint32_t)0x0B000100);   // family 11, model 1
    VERIFY_ARE_EQUAL(words.Word3(), (uint32_t)0x01000000);   // revision 1.0.0.0

    auto identity = MidiStreamMessageBuilder::ParseDeviceIdentityNotificationMessage(message);

    VERIFY_IS_NOT_NULL(identity);

    auto sysExId = identity.SystemExclusiveId();

    VERIFY_ARE_EQUAL(sysExId.size(), (uint32_t)3);
    VERIFY_ARE_EQUAL(sysExId[0], sysExId1);
    VERIFY_ARE_EQUAL(sysExId[1], sysExId2);
    VERIFY_ARE_EQUAL(sysExId[2], sysExId3);

    VERIFY_ARE_EQUAL(identity.DeviceFamilyLsb(), familyLsb);
    VERIFY_ARE_EQUAL(identity.DeviceFamilyMsb(), familyMsb);

    VERIFY_ARE_EQUAL(identity.DeviceFamilyModelNumberLsb(), modelLsb);
    VERIFY_ARE_EQUAL(identity.DeviceFamilyModelNumberMsb(), modelMsb);

    auto revision = identity.SoftwareRevisionLevel();

    VERIFY_ARE_EQUAL(revision.size(), (uint32_t)4);
    VERIFY_ARE_EQUAL(revision[0], revision1);
    VERIFY_ARE_EQUAL(revision[1], revision2);
    VERIFY_ARE_EQUAL(revision[2], revision3);
    VERIFY_ARE_EQUAL(revision[3], revision4);
}

void MidiStreamMessageBuilderTests::TestDeviceIdentityNotificationMasksHighBits()
{
    // Every field is seven bits on the wire. A caller passing a full byte must not corrupt the
    // neighboring field.
    auto message = MidiStreamMessageBuilder::BuildDeviceIdentityNotificationMessage(
        0,
        0xFF, 0xFF, 0xFF,
        0xFF, 0xFF,
        0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF);

    VERIFY_IS_NOT_NULL(message);

    auto identity = MidiStreamMessageBuilder::ParseDeviceIdentityNotificationMessage(message);

    VERIFY_IS_NOT_NULL(identity);

    auto sysExId = identity.SystemExclusiveId();

    VERIFY_ARE_EQUAL(sysExId[0], (uint8_t)0x7F);
    VERIFY_ARE_EQUAL(sysExId[1], (uint8_t)0x7F);
    VERIFY_ARE_EQUAL(sysExId[2], (uint8_t)0x7F);

    VERIFY_ARE_EQUAL(identity.DeviceFamilyLsb(), (uint8_t)0x7F);
    VERIFY_ARE_EQUAL(identity.DeviceFamilyMsb(), (uint8_t)0x7F);

    VERIFY_ARE_EQUAL(identity.DeviceFamilyModelNumberLsb(), (uint8_t)0x7F);
    VERIFY_ARE_EQUAL(identity.DeviceFamilyModelNumberMsb(), (uint8_t)0x7F);

    auto revision = identity.SoftwareRevisionLevel();

    VERIFY_ARE_EQUAL(revision[0], (uint8_t)0x7F);
    VERIFY_ARE_EQUAL(revision[1], (uint8_t)0x7F);
    VERIFY_ARE_EQUAL(revision[2], (uint8_t)0x7F);
    VERIFY_ARE_EQUAL(revision[3], (uint8_t)0x7F);
}

void MidiStreamMessageBuilderTests::TestDeviceIdentityNotificationRejectsOtherMessages()
{
    // Same message type and form, different status. Returning an identity here would hand the
    // caller eleven fields of unrelated data.
    auto discovery = MidiStreamMessageBuilder::BuildEndpointDiscoveryMessage(
        0, 1, 1, MidiEndpointDiscoveryRequests::RequestEndpointInfo);

    VERIFY_IS_NOT_NULL(discovery);

    auto identity = MidiStreamMessageBuilder::ParseDeviceIdentityNotificationMessage(discovery);

    VERIFY_IS_NULL(identity);
}
