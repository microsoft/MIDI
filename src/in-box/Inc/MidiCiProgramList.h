// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <stdint.h>
#include <stddef.h>

// Builds the Property Exchange ProgramList resource. The caller supplies every entry and owns the
// buffer; chunking for the wire is done separately using the helpers in MidiCiMessage.h.

namespace WindowsMidiServicesCapabilityInquiry
{
    struct ProgramListEntry
    {
        // UTF-8, owned by the caller and only read during the call.
        char const* Title{ nullptr };

        // The values a client should send to select this program. M2-107-UM states that bank and
        // program are 0 based; its own example contradicts that with 1 based program numbers, but
        // the bank value in the same example is the literal wire value, so the normative text wins.
        uint8_t BankMsb{ 0 };
        uint8_t BankLsb{ 0 };
        uint8_t Program{ 0 };

        // Category names, or an empty list to omit. M2-107-UM suggests a vocabulary in Appendix A
        // but says a device is not limited to it, so nothing here is checked against that list.
        char const* const* Categories{ nullptr };
        size_t CategoryCount{ 0 };

        // A single free form tag, or null to omit. Sound sets routinely give the same name to a
        // program and its variation, and this is the field that tells them apart without inventing
        // a title the sound set never had.
        char const* Tag{ nullptr };
    };

    // One entry of a "links" array. M2-105-UM section 4.5.2 is what makes a device with more than
    // one program list usable: a channel entry lists the collections that channel can select from,
    // and the resource id is how a client asks for one of them.
    struct ResourceLink
    {
        // UTF-8, owned by the caller and only read during the call.
        char const* Resource{ nullptr };
        char const* ResourceId{ nullptr };
        char const* Title{ nullptr };
    };

    namespace Details
    {
        inline bool AppendCharacter(
            _Out_writes_opt_(capacity) char* const buffer,
            _In_ size_t const capacity,
            _Inout_ size_t& length,
            _In_ char const value
        ) noexcept
        {
            if (length >= capacity)
            {
                return false;
            }

            if (buffer != nullptr)
            {
                buffer[length] = value;
            }

            length++;
            return true;
        }

        inline bool AppendText(
            _Out_writes_opt_(capacity) char* const buffer,
            _In_ size_t const capacity,
            _Inout_ size_t& length,
            _In_z_ char const* text
        ) noexcept
        {
            while (*text != '\0')
            {
                if (!AppendCharacter(buffer, capacity, length, *text++))
                {
                    return false;
                }
            }

            return true;
        }

        inline bool AppendUnicodeEscape(
            _Out_writes_opt_(capacity) char* const buffer,
            _In_ size_t const capacity,
            _Inout_ size_t& length,
            _In_ uint16_t const value
        ) noexcept
        {
            static constexpr char digits[] = "0123456789ABCDEF";

            if (!AppendText(buffer, capacity, length, "\\u"))
            {
                return false;
            }

            for (int shift = 12; shift >= 0; shift -= 4)
            {
                if (!AppendCharacter(buffer, capacity, length, digits[(value >> shift) & 0x0F]))
                {
                    return false;
                }
            }

            return true;
        }

        // One UTF-8 sequence from a NUL terminated string. Refuses anything malformed, including an
        // overlong form, a surrogate half and a value past the last code point, none of which can
        // be written as a JSON escape. A continuation byte test also fails on the NUL, so a
        // truncated sequence at the end of the string is refused rather than read past.
        inline bool DecodeUtf8(
            _In_z_ uint8_t const* const text,
            _Out_ uint32_t& codePoint,
            _Out_ size_t& consumed
        ) noexcept
        {
            codePoint = 0;
            consumed = 0;

            const auto lead = text[0];

            size_t following = 0;
            uint32_t value = 0;

            if (lead >= 0xC2 && lead <= 0xDF) { following = 1; value = lead & 0x1Fu; }
            else if (lead >= 0xE0 && lead <= 0xEF) { following = 2; value = lead & 0x0Fu; }
            else if (lead >= 0xF0 && lead <= 0xF4) { following = 3; value = lead & 0x07u; }
            else { return false; }

            for (size_t i = 1; i <= following; i++)
            {
                if ((text[i] & 0xC0) != 0x80)
                {
                    return false;
                }

                value = (value << 6) | (text[i] & 0x3Fu);
            }

            if (following == 2 && value < 0x800) { return false; }
            if (following == 3 && value < 0x10000) { return false; }
            if (value >= 0xD800 && value <= 0xDFFF) { return false; }
            if (value > 0x10FFFF) { return false; }

            codePoint = value;
            consumed = following + 1;

            return true;
        }

        // A name comes out of a sound set file, so it may contain anything. An unescaped quote
        // produces malformed JSON, and a byte with the high bit set cannot travel inside a system
        // exclusive message at all. M2-103-UM section 7.6 settles what to do instead: everything
        // outside seven bit ASCII is converted to UTF-16 and escaped with "\u".
        inline bool AppendJsonString(
            _Out_writes_opt_(capacity) char* const buffer,
            _In_ size_t const capacity,
            _Inout_ size_t& length,
            _In_opt_z_ char const* text
        ) noexcept
        {
            if (!AppendCharacter(buffer, capacity, length, '"'))
            {
                return false;
            }

            if (text != nullptr)
            {
                auto const* cursor = reinterpret_cast<uint8_t const*>(text);

                while (*cursor != '\0')
                {
                    const auto value = *cursor;

                    if (value == '"' || value == '\\')
                    {
                        if (!AppendCharacter(buffer, capacity, length, '\\')) { return false; }
                        if (!AppendCharacter(buffer, capacity, length, static_cast<char>(value))) { return false; }

                        cursor++;
                        continue;
                    }

                    if (value >= 0x20 && value <= 0x7E)
                    {
                        if (!AppendCharacter(buffer, capacity, length, static_cast<char>(value))) { return false; }

                        cursor++;
                        continue;
                    }

                    if (value < 0x80)
                    {
                        // A control character, or delete. It is already one character, so it is
                        // escaped as itself rather than run through the UTF-8 decoder.
                        if (!AppendUnicodeEscape(buffer, capacity, length, value)) { return false; }

                        cursor++;
                        continue;
                    }

                    // Everything else is escaped. The text is UTF-8, so a multi byte sequence has
                    // to be decoded first: escaping its bytes one at a time would arrive as
                    // mojibake rather than the character the sound set actually named.
                    uint32_t codePoint = 0;
                    size_t consumed = 0;

                    if (!DecodeUtf8(cursor, codePoint, consumed))
                    {
                        // One replacement character and one byte forward, so a bad byte cannot
                        // throw the rest of the name out of step.
                        codePoint = 0xFFFD;
                        consumed = 1;
                    }

                    cursor += consumed;

                    if (codePoint > 0xFFFF)
                    {
                        const auto remainder = codePoint - 0x10000;

                        if (!AppendUnicodeEscape(buffer, capacity, length,
                            static_cast<uint16_t>(0xD800 + (remainder >> 10)))) { return false; }

                        if (!AppendUnicodeEscape(buffer, capacity, length,
                            static_cast<uint16_t>(0xDC00 + (remainder & 0x3FF)))) { return false; }

                        continue;
                    }

                    if (!AppendUnicodeEscape(buffer, capacity, length, static_cast<uint16_t>(codePoint)))
                    {
                        return false;
                    }
                }
            }

            return AppendCharacter(buffer, capacity, length, '"');
        }

        inline bool AppendNumber(
            _Out_writes_opt_(capacity) char* const buffer,
            _In_ size_t const capacity,
            _Inout_ size_t& length,
            _In_ uint8_t const value
        ) noexcept
        {
            char text[4]{};
            size_t digits = 0;

            uint8_t remaining = value;

            do
            {
                text[digits++] = static_cast<char>('0' + (remaining % 10));
                remaining = static_cast<uint8_t>(remaining / 10);
            } while (remaining != 0);

            while (digits > 0)
            {
                if (!AppendCharacter(buffer, capacity, length, text[--digits]))
                {
                    return false;
                }
            }

            return true;
        }
    }

    // Writes the JSON array. Pass a null buffer to measure the length first. Returns zero when the
    // buffer is too small, so a partial array is never mistaken for a complete one.
    inline size_t BuildProgramListJson(
        _In_reads_(entryCount) ProgramListEntry const* const entries,
        _In_ size_t const entryCount,
        _Out_writes_opt_(capacity) char* const buffer,
        _In_ size_t const capacity
    ) noexcept
    {
        if (entries == nullptr && entryCount > 0)
        {
            return 0;
        }

        const size_t limit = (buffer == nullptr) ? SIZE_MAX : capacity;

        size_t length = 0;
        bool ok = true;

        ok = ok && Details::AppendCharacter(buffer, limit, length, '[');

        for (size_t i = 0; ok && i < entryCount; i++)
        {
            const auto& entry = entries[i];

            if (i > 0)
            {
                ok = ok && Details::AppendCharacter(buffer, limit, length, ',');
            }

            ok = ok && Details::AppendText(buffer, limit, length, "{\"title\":");
            ok = ok && Details::AppendJsonString(buffer, limit, length, entry.Title);

            ok = ok && Details::AppendText(buffer, limit, length, ",\"bankPC\":[");
            ok = ok && Details::AppendNumber(buffer, limit, length, entry.BankMsb & 0x7F);
            ok = ok && Details::AppendCharacter(buffer, limit, length, ',');
            ok = ok && Details::AppendNumber(buffer, limit, length, entry.BankLsb & 0x7F);
            ok = ok && Details::AppendCharacter(buffer, limit, length, ',');
            ok = ok && Details::AppendNumber(buffer, limit, length, entry.Program & 0x7F);
            ok = ok && Details::AppendCharacter(buffer, limit, length, ']');

            if (entry.Categories != nullptr && entry.CategoryCount > 0)
            {
                ok = ok && Details::AppendText(buffer, limit, length, ",\"category\":[");

                for (size_t category = 0; ok && category < entry.CategoryCount; category++)
                {
                    if (category > 0)
                    {
                        ok = ok && Details::AppendCharacter(buffer, limit, length, ',');
                    }

                    ok = ok && Details::AppendJsonString(buffer, limit, length, entry.Categories[category]);
                }

                ok = ok && Details::AppendCharacter(buffer, limit, length, ']');
            }

            if (entry.Tag != nullptr)
            {
                ok = ok && Details::AppendText(buffer, limit, length, ",\"tags\":[");
                ok = ok && Details::AppendJsonString(buffer, limit, length, entry.Tag);
                ok = ok && Details::AppendCharacter(buffer, limit, length, ']');
            }

            ok = ok && Details::AppendCharacter(buffer, limit, length, '}');
        }

        ok = ok && Details::AppendCharacter(buffer, limit, length, ']');

        return ok ? length : 0;
    }


    struct DeviceInfoFields
    {
        // The same identity the MIDI 1.0 identity reply, the MIDI-CI discovery reply and the UMP
        // device identity notification carry, in a fifth encoding.
        uint8_t ManufacturerId[3]{};
        char const* Manufacturer{ nullptr };

        uint8_t FamilyId[2]{};
        char const* Family{ nullptr };

        uint8_t ModelId[2]{};
        char const* Model{ nullptr };

        // M2-105-UM makes both of these required, and says versionId shall match the software
        // revision in the MIDI-CI Discovery message.
        uint8_t VersionId[4]{};
        char const* Version{ nullptr };
    };

    inline size_t BuildDeviceInfoJson(
        _In_ DeviceInfoFields const& fields,
        _Out_writes_opt_(capacity) char* const buffer,
        _In_ size_t const capacity
    ) noexcept
    {
        const size_t limit = (buffer == nullptr) ? SIZE_MAX : capacity;

        size_t length = 0;
        bool ok = true;

        const auto appendByteArray = [&](char const* name, uint8_t const* values, size_t count)
        {
            ok = ok && Details::AppendCharacter(buffer, limit, length, '"');
            ok = ok && Details::AppendText(buffer, limit, length, name);
            ok = ok && Details::AppendText(buffer, limit, length, "\":[");

            for (size_t i = 0; ok && i < count; i++)
            {
                if (i > 0) { ok = ok && Details::AppendCharacter(buffer, limit, length, ','); }
                ok = ok && Details::AppendNumber(buffer, limit, length, values[i] & 0x7F);
            }

            ok = ok && Details::AppendCharacter(buffer, limit, length, ']');
        };

        const auto appendString = [&](char const* name, char const* value)
        {
            ok = ok && Details::AppendText(buffer, limit, length, ",\"");
            ok = ok && Details::AppendText(buffer, limit, length, name);
            ok = ok && Details::AppendText(buffer, limit, length, "\":");
            ok = ok && Details::AppendJsonString(buffer, limit, length, value);
        };

        ok = ok && Details::AppendCharacter(buffer, limit, length, '{');

        appendByteArray("manufacturerId", fields.ManufacturerId, 3);
        appendString("manufacturer", fields.Manufacturer);

        ok = ok && Details::AppendCharacter(buffer, limit, length, ',');
        appendByteArray("familyId", fields.FamilyId, 2);
        appendString("family", fields.Family);

        ok = ok && Details::AppendCharacter(buffer, limit, length, ',');
        appendByteArray("modelId", fields.ModelId, 2);
        appendString("model", fields.Model);

        ok = ok && Details::AppendCharacter(buffer, limit, length, ',');
        appendByteArray("versionId", fields.VersionId, 4);
        appendString("version", fields.Version);

        ok = ok && Details::AppendCharacter(buffer, limit, length, '}');

        return ok ? length : 0;
    }

    // Advertises which resources this device will answer for.
    struct ResourceListEntry
    {
        char const* Resource{ nullptr };

        // A resource published more than once, told apart by resource id, has to say so or a
        // client is entitled to ask for it without one.
        bool RequireResourceId{ false };

        bool CanSubscribe{ false };

        // Declaring this obliges every reply for the resource to carry "totalCount", per
        // M2-103-UM section 8.6.2.
        bool CanPaginate{ false };
    };

    inline size_t BuildResourceListJson(
        _In_reads_(count) ResourceListEntry const* const entries,
        _In_ size_t const count,
        _Out_writes_opt_(capacity) char* const buffer,
        _In_ size_t const capacity
    ) noexcept
    {
        if (entries == nullptr && count > 0)
        {
            return 0;
        }

        const size_t limit = (buffer == nullptr) ? SIZE_MAX : capacity;

        size_t length = 0;
        bool ok = true;

        ok = ok && Details::AppendCharacter(buffer, limit, length, '[');

        for (size_t i = 0; ok && i < count; i++)
        {
            if (i > 0) { ok = ok && Details::AppendCharacter(buffer, limit, length, ','); }

            ok = ok && Details::AppendText(buffer, limit, length, "{\"resource\":");
            ok = ok && Details::AppendJsonString(buffer, limit, length, entries[i].Resource);

            // Both default to false, so they are written only when they are true. M2-105-UM
            // section 4.7 shows a bare resource name as a complete entry.
            if (entries[i].RequireResourceId)
            {
                ok = ok && Details::AppendText(buffer, limit, length, ",\"requireResId\":true");
            }

            if (entries[i].CanSubscribe)
            {
                ok = ok && Details::AppendText(buffer, limit, length, ",\"canSubscribe\":true");
            }

            if (entries[i].CanPaginate)
            {
                ok = ok && Details::AppendText(buffer, limit, length, ",\"canPaginate\":true");
            }

            ok = ok && Details::AppendCharacter(buffer, limit, length, '}');
        }

        ok = ok && Details::AppendCharacter(buffer, limit, length, ']');

        return ok ? length : 0;
    }


    struct ChannelListEntry
    {
        char const* Title{ nullptr };

        // One based, 1 to 256, counted across every Group in the Function Block. Note that this is
        // one based while bankPC in the same object is zero based.
        uint16_t Channel{ 1 };

        char const* ProgramTitle{ nullptr };

        uint8_t BankMsb{ 0 };
        uint8_t BankLsb{ 0 };
        uint8_t Program{ 0 };

        // The program collections, and anything else, reachable from this channel.
        ResourceLink const* Links{ nullptr };
        size_t LinkCount{ 0 };
    };

    inline size_t BuildChannelListJson(
        _In_reads_(entryCount) ChannelListEntry const* const entries,
        _In_ size_t const entryCount,
        _Out_writes_opt_(capacity) char* const buffer,
        _In_ size_t const capacity
    ) noexcept
    {
        if (entries == nullptr && entryCount > 0)
        {
            return 0;
        }

        const size_t limit = (buffer == nullptr) ? SIZE_MAX : capacity;

        size_t length = 0;
        bool ok = true;

        ok = ok && Details::AppendCharacter(buffer, limit, length, '[');

        for (size_t i = 0; ok && i < entryCount; i++)
        {
            const auto& entry = entries[i];

            if (i > 0)
            {
                ok = ok && Details::AppendCharacter(buffer, limit, length, ',');
            }

            ok = ok && Details::AppendText(buffer, limit, length, "{\"title\":");
            ok = ok && Details::AppendJsonString(buffer, limit, length, entry.Title);

            ok = ok && Details::AppendText(buffer, limit, length, ",\"channel\":");

            // Channel can reach 256, which does not fit the single byte number helper.
            {
                char digits[4]{};
                size_t count = 0;
                uint16_t remaining = entry.Channel;

                do
                {
                    digits[count++] = static_cast<char>('0' + (remaining % 10));
                    remaining = static_cast<uint16_t>(remaining / 10);
                } while (remaining != 0 && count < sizeof(digits));

                while (ok && count > 0)
                {
                    ok = ok && Details::AppendCharacter(buffer, limit, length, digits[--count]);
                }
            }

            if (entry.ProgramTitle != nullptr)
            {
                ok = ok && Details::AppendText(buffer, limit, length, ",\"programTitle\":");
                ok = ok && Details::AppendJsonString(buffer, limit, length, entry.ProgramTitle);
            }

            ok = ok && Details::AppendText(buffer, limit, length, ",\"bankPC\":[");
            ok = ok && Details::AppendNumber(buffer, limit, length, entry.BankMsb & 0x7F);
            ok = ok && Details::AppendCharacter(buffer, limit, length, ',');
            ok = ok && Details::AppendNumber(buffer, limit, length, entry.BankLsb & 0x7F);
            ok = ok && Details::AppendCharacter(buffer, limit, length, ',');
            ok = ok && Details::AppendNumber(buffer, limit, length, entry.Program & 0x7F);
            ok = ok && Details::AppendCharacter(buffer, limit, length, ']');

            if (entry.Links != nullptr && entry.LinkCount > 0)
            {
                ok = ok && Details::AppendText(buffer, limit, length, ",\"links\":[");

                for (size_t link = 0; ok && link < entry.LinkCount; link++)
                {
                    if (link > 0)
                    {
                        ok = ok && Details::AppendCharacter(buffer, limit, length, ',');
                    }

                    ok = ok && Details::AppendText(buffer, limit, length, "{\"resource\":");
                    ok = ok && Details::AppendJsonString(buffer, limit, length, entry.Links[link].Resource);

                    if (entry.Links[link].ResourceId != nullptr)
                    {
                        ok = ok && Details::AppendText(buffer, limit, length, ",\"resId\":");
                        ok = ok && Details::AppendJsonString(buffer, limit, length, entry.Links[link].ResourceId);
                    }

                    if (entry.Links[link].Title != nullptr)
                    {
                        ok = ok && Details::AppendText(buffer, limit, length, ",\"title\":");
                        ok = ok && Details::AppendJsonString(buffer, limit, length, entry.Links[link].Title);
                    }

                    ok = ok && Details::AppendCharacter(buffer, limit, length, '}');
                }

                ok = ok && Details::AppendCharacter(buffer, limit, length, ']');
            }

            ok = ok && Details::AppendCharacter(buffer, limit, length, '}');
        }

        ok = ok && Details::AppendCharacter(buffer, limit, length, ']');

        return ok ? length : 0;
    }
}
