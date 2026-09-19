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
    // Where a sound set is allowed to come from. The parser is hardened and fuzzed, but it still
    // runs inside the service, so the shipping default is to accept only what Windows installed.
    enum class SoundSetOrigin
    {
        // Only files under the Windows system directory. Verified on the open file handle rather
        // than on the caller's string, so symlinks, junctions, short names, relative traversal and
        // a swap between the check and the read cannot get around it.
        SystemOnly,

        // Any readable path. Kept working for offline tools and for user supplied sound sets later.
        AnyPath,
    };

    // What a settings app needs to show a sound set in a list.
    struct DlsSoundSetInfo
    {
        std::wstring Name;
        std::wstring Engineer;
        std::wstring Comments;

        DlsVersion Version{};

        // What the collection header claims, which is cheap to read and does not require parsing
        // the instrument list.
        uint32_t InstrumentCount{ 0 };

        uint64_t FileBytes{ 0 };
    };

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
            _In_ SoundSetOrigin origin,
            _Out_ DlsCollection& collection);

        // Reads only the small header chunks, seeking past the instrument list and wave pool, so a
        // settings app can list what is installed without pulling megabytes of samples per file.
        static DlsParseStatus ProbeFile(
            _In_ const std::wstring& path,
            _In_ const DlsParseLimits& limits,
            _In_ SoundSetOrigin origin,
            _Out_ DlsSoundSetInfo& info);

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
