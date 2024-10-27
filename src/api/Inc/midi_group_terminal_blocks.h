// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#ifndef MIDI_GROUP_TERMINAL_BLOCKS_H
#define MIDI_GROUP_TERMINAL_BLOCKS_H

#include <Windows.h>
#include <stdint.h>
#include <memory>
#include <vector>
#include <ks.h>         // for KSMULTIPLE_ITEM

namespace WindowsMidiServicesInternal
{
    // GTB structures Copied from driver code. This should be a shared file instead.

    // Group Terminal Block structure defines
#pragma pack(push)
#pragma pack(1)

    /* Based on current definitions in USB Class Definition for MIDI Devices Release 2.0

    For Group Terminal Block Type (sect. A.6)
    BIDIRECTIONAL                       0x00
    INPUT_ONLY                          0x01
    OUTPUT_ONLY                         0x02

    For Group Terminal Default MIDI Protocol (sect. A.7)
    USE_MIDI_CI                         0x00
    MIDI_1_0_UP_TO_64_BITS              0x01
    MIDI_1_0_UP_TO_64_BITS_AND_JRTS     0x02
    MIDI_1_0_UP_TO_128_BITS             0x03
    MIDI_1_0_UP_TO_128_BITS_AND_JRTS    0x04
    MIDI_2_0                            0x11
    MIDI_2_0_AND_JRTS                   0x12

    Group Terminal Number
    GROUP_1                             0x00
    GROUP_2                             0x01
    GROUP_3                             0x02
    ...
    GROUP_16                            0x0F
    */

#define MIDI_GROUP_TERMINAL_BLOCK_BIDIRECTIONAL 0x00
#define MIDI_GROUP_TERMINAL_BLOCK_INPUT         0x01
#define MIDI_GROUP_TERMINAL_BLOCK_OUTPUT        0x02



    typedef struct
    {
        WORD                            Size;
        BYTE                            Number;             // Group Terminal Block ID
        BYTE                            Direction;          // Group Terminal Block Type
        BYTE                            FirstGroupIndex;    // The first group in this block
        BYTE                            GroupCount;         // The number of groups spanned
        BYTE                            Protocol;           // The MIDI Protocol
        WORD                            MaxInputBandwidth;  ///< Maximum Input Bandwidth Capability in 4KB/second
        WORD                            MaxOutputBandwidth; ///< Maximum Output Bandwidth Capability in 4KB/second
    } UMP_GROUP_TERMINAL_BLOCK_HEADER;

    typedef struct
    {
        UMP_GROUP_TERMINAL_BLOCK_HEADER GrpTrmBlock;
        WCHAR                           Name[1];             // NULL Terminated string, blank indicates none available
        // from USB Device
    } UMP_GROUP_TERMINAL_BLOCK_DEFINITION, * PUMP_GROUP_TERMINAL_BLOCK_DEFINITION;
#pragma pack(pop)


    struct GroupTerminalBlockInternal
    {
        uint8_t Number{ 0 };
        uint8_t Direction{ MIDI_GROUP_TERMINAL_BLOCK_BIDIRECTIONAL };
        uint8_t FirstGroupIndex{ 0 };
        uint8_t GroupCount{ 1 };
        uint8_t Protocol{ 0 };
        uint16_t MaxInputBandwidth{ 0 };
        uint16_t MaxOutputBandwidth{ 0 };

        std::wstring Name{ };
    };



    // used by the API
    inline std::vector<GroupTerminalBlockInternal> ReadGroupTerminalBlocksFromPropertyData(
        _In_reads_bytes_(dataSize) uint8_t* ksMultipleItemsDataPointer,
        _In_ uint32_t const dataSize
    ) noexcept
    {
        std::vector<GroupTerminalBlockInternal> blocks{ };

        try
        {
            if (ksMultipleItemsDataPointer != nullptr)
            {
                uint32_t offset = 0;

                // get the KS_MULTIPLE_ITEMS info. First is a ULONG of the total size, next is a ULONG of the count of items
                // I should use the struct directly, but I don't want to pull in all that KS stuff here.

                // we don't actually need anything from the KS_MULTIPLE_ITEMS header, so we skip it
                offset += sizeof(KSMULTIPLE_ITEM);

                // read all entries
                while (offset < dataSize)
                {
                    auto pheader = (UMP_GROUP_TERMINAL_BLOCK_HEADER*)(ksMultipleItemsDataPointer + offset);
                    //auto block = winrt::make_self<MidiGroupTerminalBlock>();

                    GroupTerminalBlockInternal gtb{};

                    gtb.Number = pheader->Number;
                    gtb.Direction = pheader->Direction;
                    gtb.FirstGroupIndex = pheader->FirstGroupIndex;
                    gtb.GroupCount = pheader->GroupCount;
                    gtb.Protocol = pheader->Protocol;
                    gtb.MaxInputBandwidth = pheader->MaxInputBandwidth;
                    gtb.MaxOutputBandwidth = pheader->MaxOutputBandwidth;

                    int charOffset = sizeof(UMP_GROUP_TERMINAL_BLOCK_HEADER);

                    // TODO this could be much more efficient just pointing to a string instead of char by char
                    while (charOffset + 1 < pheader->Size)
                    {
                        wchar_t ch = (wchar_t)(*(ksMultipleItemsDataPointer + offset + charOffset));

                        gtb.Name += ch;

                        charOffset += sizeof(wchar_t);
                    }

                    blocks.push_back(std::move(gtb));

                    // move to the next struct, if there is one
                    offset += pheader->Size;
                }
            }
        }
        catch (...)
        {
            
        }

        return blocks;

    }

    // used by the service when creating new GTBs to be read by the API
    inline bool WriteGroupTerminalBlocksToPropertyDataPointer(
        _In_ std::vector<GroupTerminalBlockInternal>& blocks,
        _Inout_ std::vector<std::byte>& propertyData
    )
    {
        if (blocks.size() == 0) return false;

        // create a vector of the data passed in, but in GTB format
        // include the strings
        // figure out header size
        // total up all the memory

        size_t totalMemory{ 0 };
        std::vector<UMP_GROUP_TERMINAL_BLOCK_DEFINITION> blockdefs{ };

        for (auto const& block : blocks)
        {
            UMP_GROUP_TERMINAL_BLOCK_DEFINITION def{};
            def.GrpTrmBlock.Number = block.Number;
            def.GrpTrmBlock.Direction = block.Direction;
            def.GrpTrmBlock.FirstGroupIndex = block.FirstGroupIndex;
            def.GrpTrmBlock.GroupCount = block.GroupCount;
            def.GrpTrmBlock.MaxInputBandwidth = block.MaxInputBandwidth;
            def.GrpTrmBlock.MaxOutputBandwidth = block.MaxOutputBandwidth;
            def.GrpTrmBlock.Protocol = block.Protocol;

            // this needs to include the name including null terminator.
            // if no name, it's 1 character. The additional sizeof(wchar_t) is to account for nul
            def.GrpTrmBlock.Size = (WORD)(sizeof(UMP_GROUP_TERMINAL_BLOCK_HEADER) + (block.Name.length() * sizeof(wchar_t)) + sizeof(wchar_t));

            totalMemory += def.GrpTrmBlock.Size;
            blockdefs.push_back(def);
        }

        KSMULTIPLE_ITEM ksi{ };

        // add in size of the KS multiple items header
        totalMemory += sizeof(KSMULTIPLE_ITEM);

        ksi.Count = (ULONG)blocks.size();      // count is the count of buffers in the data
        ksi.Size = (ULONG)totalMemory;         // the size is size of the header + size of all items

        // allocate the buffer 
        propertyData.resize(totalMemory, (std::byte)0);

        size_t offset{ 0 };
        size_t blockDefSizeWithoutString{ sizeof(UMP_GROUP_TERMINAL_BLOCK_DEFINITION) - sizeof(wchar_t) };

        // KS Multiple Item header
        memcpy(propertyData.data() , &ksi, sizeof(KSMULTIPLE_ITEM));
        offset += sizeof(KSMULTIPLE_ITEM);

        // copy in all the block data
        for (uint32_t i = 0; i < blockdefs.size(); i++)
        {
            memcpy(&propertyData[offset], &blockdefs[i], blockDefSizeWithoutString);

            // we've copied the non-variable data
            offset += blockDefSizeWithoutString;

            // convert our wchar_t string to an array of bytes
            auto stringBytes = reinterpret_cast<const std::byte*>(blocks[i].Name.data());

            // Copy the wide string to bytes. There's likely a nicer way to do this
            // with std::copy, but I wasn't finding it obvious
            for (uint32_t j = 0; j < blocks[i].Name.length() * sizeof(wchar_t); j++)
            {
                propertyData[offset++] = stringBytes[j];
            }

            // we don't need to add the nul because we already zeroed memory
            offset += sizeof(wchar_t);
        }


        return true;
    }

}

#endif