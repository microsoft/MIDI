// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "console_tools_shared.h"

// in addition to the return codes in console_tools_shared.h
#define RETURN_SERVICE_NOT_RESPONDING               5
#define RETURN_REPORT_SECTION_TIMED_OUT             6

namespace mididiag
{
    // The value of a record line, such as: number=1 direction=source name="Port 1"
    // A value with a space, an equals sign or a double quote is quoted, and a double quote
    // inside it is written twice. Names a device supplies are cleaned of control characters
    // first, so a name cannot start a line of its own.
    class KeyValueText
    {
    public:
        KeyValueText& Add(_In_ std::wstring_view const key, _In_ std::wstring_view const value);
        KeyValueText& AddIfNotEmpty(_In_ std::wstring_view const key, _In_ std::wstring_view const value);

        // separate names, so a string literal can never land on the bool overload
        KeyValueText& AddBool(_In_ std::wstring_view const key, _In_ bool const value);
        KeyValueText& AddNumber(_In_ std::wstring_view const key, _In_ uint64_t const value);
        KeyValueText& AddSignedNumber(_In_ std::wstring_view const key, _In_ int64_t const value);
        KeyValueText& AddHex(_In_ std::wstring_view const key, _In_ uint32_t const value, _In_ uint32_t const digits);

        std::wstring const& Text() const noexcept { return m_text; }
        bool Empty() const noexcept { return m_text.empty(); }

    private:
        std::wstring m_text{};
    };

    struct Midi1PortSummary
    {
        std::wstring Flow{};
        uint32_t Number{ 0 };
        std::wstring Name{};
    };

    struct ReportContext
    {
        bool Elevated{ false };
        bool IncludeWinRTMidi1{ false };

        // Legacy API mode turns the service off, so nothing may start it
        bool LegacyApiMode{ false };

        bool ServiceInstalled{ false };
        bool ServiceDisabled{ false };
        bool ServiceRunningBeforeReport{ false };
        uint32_t ServiceProcessIdBeforeReport{ 0 };

        // filled by the endpoint section and used to name what each session has open. The keys
        // are device ids in lower case.
        std::map<std::wstring, std::wstring> EndpointNames{};
        std::map<std::wstring, Midi1PortSummary> Midi1Ports{};

        // from the network section, for the finding about hosts the firewall blocks
        bool FirewallStateKnown{ false };
        bool AnyConnectedNetworkBlocksMidiService{ false };
    };

    ReportContext& Context() noexcept;

    void WriteSection(_In_ std::wstring_view const sectionName);

    // for the marker sections at the very end, which are not timed
    void WriteClosingSection(_In_ std::wstring_view const sectionName);

    void WriteRecordBreak();
    void WriteLine(_In_ std::wstring_view const text);

    void WriteField(_In_ std::wstring_view const label, _In_ std::wstring_view const value);
    void WriteStyledField(_In_ std::wstring_view const label, _In_ std::wstring_view const value, _In_ fmt::text_style const& valueStyle);
    void WriteField(_In_ std::wstring_view const label, _In_ KeyValueText const& values);
    void WriteBoolField(_In_ std::wstring_view const label, _In_ bool const value);
    void WriteNumberField(_In_ std::wstring_view const label, _In_ uint64_t const value);
    void WriteError(_In_ std::wstring_view const message);

    // Profile folders carry the person's name, so they are written as %USERPROFILE% or <user>.
    std::wstring RemoveUserNames(_In_ std::wstring_view const text);

    // Only the last part of an IP address is kept. Host names are left as they are.
    std::wstring MaskIpAddress(_In_ std::wstring_view const address);

    // "YYYY-MM-DD HH:MM:SS" in the PC's own time zone, or empty for a time that was never set
    std::wstring FormatLocalTime(_In_ FILETIME const& utcTime);
    std::wstring FormatLocalTime(_In_ foundation::DateTime const& time);

    // days.hours:minutes:seconds
    std::wstring FormatDuration(_In_ std::chrono::seconds const duration);

    std::wstring FormatHResult(_In_ HRESULT const hr);

    template <typename... TArgs>
    std::wstring FormatResourceString(_In_ UINT const resourceId, TArgs&&... args)
    {
        auto values = std::make_tuple(std::forward<TArgs>(args)...);

        return std::apply([resourceId](auto&... unpacked)
            {
                return std::vformat(internal::ResourceGetWString(resourceId), std::make_wformat_args(unpacked...));
            }, values);
    }

    void AddFinding(_In_ std::wstring_view const id, _In_ std::wstring const& text);
    void WriteFindingsSection();
    void WriteSectionTimingSection();

    // A call into the service can block forever when the service is stuck, which is the very
    // problem this report is most often run for. The watchdog ends the report cleanly instead:
    // each section gets a time limit, and the report says which one ran out.
    enum class ReportPhase
    {
        Local,
        Service
    };

    void StartWatchdog();
    void StopWatchdog();
    void SetReportPhase(_In_ ReportPhase const phase, _In_ std::chrono::milliseconds const sectionTimeout);

    // for one section that needs a different limit from the rest of its phase
    void SetCurrentSectionTimeout(_In_ std::chrono::milliseconds const timeout);
}
