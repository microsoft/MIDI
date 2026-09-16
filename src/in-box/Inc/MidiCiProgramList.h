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

        // One of the Appendix A categories, or null to omit.
        char const* Category{ nullptr };
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

        // A name comes out of a sound set file, so it may contain anything. An unescaped quote
        // produces malformed JSON, and a byte with the high bit set cannot travel inside a system
        // exclusive message at all.
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
                for (; *text != '\0'; text++)
                {
                    const auto value = static_cast<uint8_t>(*text);

                    if (value == '"' || value == '\\')
                    {
                        if (!AppendCharacter(buffer, capacity, length, '\\')) { return false; }
                        if (!AppendCharacter(buffer, capacity, length, static_cast<char>(value))) { return false; }
                        continue;
                    }

                    if (value < 0x20 || value > 0x7E)
                    {
                        // Control characters and anything outside seven bit ASCII become an escape,
                        // which keeps the payload legal both as JSON and as system exclusive data.
                        static constexpr char digits[] = "0123456789ABCDEF";

                        if (!AppendText(buffer, capacity, length, "\\u00")) { return false; }
                        if (!AppendCharacter(buffer, capacity, length, digits[(value >> 4) & 0x0F])) { return false; }
                        if (!AppendCharacter(buffer, capacity, length, digits[value & 0x0F])) { return false; }
                        continue;
                    }

                    if (!AppendCharacter(buffer, capacity, length, static_cast<char>(value)))
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

            if (entry.Category != nullptr)
            {
                ok = ok && Details::AppendText(buffer, limit, length, ",\"category\":[");
                ok = ok && Details::AppendJsonString(buffer, limit, length, entry.Category);
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

        ok = ok && Details::AppendCharacter(buffer, limit, length, '}');

        return ok ? length : 0;
    }

    // Advertises which resources this device will answer for.
    inline size_t BuildResourceListJson(
        _In_reads_(count) char const* const* const resourceNames,
        _In_ size_t const count,
        _Out_writes_opt_(capacity) char* const buffer,
        _In_ size_t const capacity
    ) noexcept
    {
        if (resourceNames == nullptr && count > 0)
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
            ok = ok && Details::AppendJsonString(buffer, limit, length, resourceNames[i]);
            ok = ok && Details::AppendCharacter(buffer, limit, length, '}');
        }

        ok = ok && Details::AppendCharacter(buffer, limit, length, ']');

        return ok ? length : 0;
    }
}
