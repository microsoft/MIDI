// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "TestLayoutFiles.h"

namespace glasstests
{
    std::wstring HandAuthoredLayout()
    {
        return LR"JSON({
    "pages": [
        {
            "controls": [
                {
                    "kind": "fader",
                    "id": "fader-1",
                    "label": "Kick",
                    "x": 48, "y": 32, "width": 40, "height": 180,
                    "hueSlot": 2,
                    "keyboardOrder": 1,
                    "pickup": "catch",
                    "defaultValue": 0.25,
                    "sendsValueOnStart": true,
                    "messages": [
                        {
                            "kind": "controlChange",
                            "trigger": "changes",
                            "device": "Desk",
                            "group": 0,
                            "channel": 3,
                            "number": 7
                        }
                    ],
                    "feedback": {
                        "enabled": true,
                        "kind": "controlChange",
                        "device": "Desk",
                        "group": 0,
                        "channel": 3,
                        "number": 7
                    }
                },
                {
                    "id": "pad-1",
                    "kind": "pad",
                    "label": "Run",
                    "x": 200, "y": 32, "width": 56, "height": 56,
                    "hueSlot": 4,
                    "keyboardOrder": 2,
                    "messages": [
                        { "kind": "sequence", "trigger": "turnsOn", "sequence": "Intro" },
                        {
                            "kind": "systemExclusive",
                            "trigger": "turnsOn",
                            "device": "Synth",
                            "systemExclusive": "F07E7F0601F7"
                        }
                    ]
                }
            ],
            "id": "page-1",
            "name": "Mixer",
            "hueSlot": 1
        }
    ],
    "name": "Hand Authored",
    "description": "Written by a person, not by the app.",
    "fileVersion": 1,
    "pageWidth": 1280,
    "pageHeight": 800,
    "canvasWidth": 1600,
    "canvasHeight": 1000,
    "theme": "Studio Dark",
    "scaleMode": "fitToScreen",
    "fullScreenButtonCorner": "bottomLeft",
    "created": 1790000000,
    "modified": 1790000001,
    "tempo": { "kind": "followIncomingClock", "beatsPerMinute": 128, "device": "Desk" },
    "devices": [
        {
            "name": "Desk",
            "matchMode": "usbVendorAndProduct",
            "match": { "usbVendorId": 1234, "usbProductId": 99, "transportSuppliedEndpointName": "Big Desk" },
            "sendsBeatClock": true
        },
        {
            "name": "Synth",
            "match": { "endpointDeviceId": "\\\\?\\swd#midisrv#example#{e7cce071-3c03-423f-88d3-f1045d02552b}" }
        }
    ],
    "sequences": [
        {
            "name": "Intro",
            "steps": [
                { "kind": "repeatStart", "repeatCount": 3 },
                { "kind": "sendMessage", "message": { "kind": "note", "device": "Synth", "channel": 0, "number": 60 } },
                { "kind": "wait", "waitMilliseconds": 250 },
                { "kind": "repeatEnd" },
                { "kind": "setControlValue", "targetControl": "fader-1", "targetValue": 0.75 }
            ]
        }
    ]
})JSON";
    }

    std::wstring LayoutFromANewerVersion()
    {
        return LR"JSON({
    "fileVersion": 99,
    "name": "From The Future",
    "pageWidth": 1920,
    "pageHeight": 1080,
    "canvasWidth": 1920,
    "canvasHeight": 1080,
    "somethingThisBuildHasNeverHeardOf": {
        "nested": [ 1, 2, { "deep": true } ],
        "alpha": "kept"
    },
    "pages": [
        {
            "id": "page-1",
            "name": "One",
            "futurePageProperty": "kept too",
            "controls": [
                {
                    "id": "c1",
                    "kind": "knob",
                    "x": 10, "y": 10, "width": 56, "height": 56,
                    "holographicProjection": { "enabled": true, "depth": 4 },
                    "messages": [
                        { "kind": "controlChange", "number": 1, "quantumEntanglement": "yes" }
                    ]
                }
            ]
        }
    ]
})JSON";
    }

    std::wstring HostileLayout()
    {
        // 4000 characters, well past the 1024 cap on a stored string.
        std::wstring const enormous(4000, L'A');

        std::wstring json = LR"JSON({
    "fileVersion": 1,
    "name": ")JSON";

        json += enormous;

        json += LR"JSON(",
    "pageWidth": -5,
    "pageHeight": 999999,
    "canvasWidth": "not a number",
    "scaleMode": "teleport",
    "fullScreenButtonCorner": 42,
    "tempo": { "kind": "quantum", "beatsPerMinute": -300 },
    "devices": "this should be an array",
    "pages": [
        {
            "id": "page-1",
            "name": "Hostile",
            "hueSlot": 4000,
            "controls": [
                {
                    "id": "c1",
                    "kind": "teapot",
                    "x": 1e308, "y": -1e308,
                    "width": 0, "height": 0,
                    "hueSlot": -99,
                    "defaultValue": 17.5,
                    "pickup": true,
                    "messages": [
                        {
                            "kind": "systemExclusive",
                            "systemExclusive": "F0ZZ7F06",
                            "group": 99,
                            "channel": -4,
                            "number": -1,
                            "words": [ 1, 2, 3, 4, 5, 6, 7, 8 ]
                        },
                        { "kind": 17 },
                        "a string where an object belongs",
                        null
                    ]
                },
                12345
            ]
        },
        null
    ],
    "sequences": [ { "name": "S", "steps": [ { "kind": "wait", "waitMilliseconds": 99999999999 } ] } ]
})JSON";

        return json;
    }
}
