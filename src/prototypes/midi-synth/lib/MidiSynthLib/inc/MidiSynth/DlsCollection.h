// Loads and owns a DLS collection. The file bytes are held in memory for the lifetime of the
// object because DlsWave::SampleData points into them.

#pragma once

#include "DlsTypes.h"

#include <sal.h>

#include <cstddef>
#include <string>
#include <vector>

namespace MidiSynth
{
    class DlsCollection
    {
    public:
        DlsCollection() = default;

        // Copying would leave DlsWave::SampleData pointing at the original buffer. Moving is safe:
        // a moved vector keeps its heap allocation, so the spans stay valid.
        DlsCollection(const DlsCollection&) = delete;
        DlsCollection& operator=(const DlsCollection&) = delete;
        DlsCollection(DlsCollection&&) noexcept = default;
        DlsCollection& operator=(DlsCollection&&) noexcept = default;

        static DlsParseStatus LoadFromFile(
            _In_ const std::wstring& path,
            _In_ const DlsParseLimits& limits,
            _Out_ DlsCollection& collection);

        static DlsParseStatus LoadFromMemory(
            _Inout_ std::vector<std::byte>&& fileBytes,
            _In_ const DlsParseLimits& limits,
            _Out_ DlsCollection& collection);

        const std::vector<DlsInstrument>& Instruments() const noexcept { return m_instruments; }
        const std::vector<DlsWave>& Waves() const noexcept { return m_waves; }

        DlsVersion Version() const noexcept { return m_version; }
        const std::wstring& Name() const noexcept { return m_name; }
        size_t FileByteCount() const noexcept { return m_fileBytes.size(); }

        // The count the collection header claims. May disagree with Instruments().size()
        // on a malformed file; the parser reports what it actually found.
        uint32_t DeclaredInstrumentCount() const noexcept { return m_declaredInstrumentCount; }

        _Ret_maybenull_ const DlsInstrument* FindInstrument(
            _In_ uint32_t bankMsb,
            _In_ uint32_t bankLsb,
            _In_ uint32_t program,
            _In_ bool isDrumKit) const noexcept;

    private:
        DlsParseStatus Parse(_In_ const DlsParseLimits& limits);

        std::vector<std::byte> m_fileBytes;
        std::vector<DlsInstrument> m_instruments;
        std::vector<DlsWave> m_waves;
        std::wstring m_name;
        DlsVersion m_version{};
        uint32_t m_declaredInstrumentCount{ 0 };
    };
}
