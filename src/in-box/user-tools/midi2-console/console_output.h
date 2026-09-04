// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace midi2console
{
    // This tool is UTF-8 narrow-string end to end, unlike the older console tools which use
    // _O_U16TEXT and std::wstring. FTXUI writes UTF-8 through std::cout, and the two modes
    // cannot share stdout.
    bool InitializeConsole();

    // False when stdout is redirected, so escape sequences never land in a captured file.
    bool StylingEnabled();

    std::string ToUtf8(_In_ std::wstring_view value);
    std::string ToUtf8(_In_ winrt::hstring const& value);
    std::wstring FromUtf8(_In_ std::string_view value);

    // Number of terminal cells a UTF-8 string occupies. Emoji and CJK are two cells, so byte
    // length and character count are both wrong for column padding.
    size_t DisplayWidth(_In_ std::string_view value);
    std::string PadRightToWidth(_In_ std::string_view value, _In_ size_t width);

    // Use in place of fmt::styled everywhere. An empty text_style makes fmt emit no escape
    // sequences at all, so suppression costs nothing at the call site.
    template <typename TValue>
    inline auto Styled(TValue const& value, fmt::text_style const& style)
    {
        return fmt::styled(value, StylingEnabled() ? style : fmt::text_style{});
    }

    // ---- theme -------------------------------------------------------------------------
    // Every color in the tool comes from here. Nothing should call fmt::fg directly.

    inline const auto normalTextStyle = fmt::fg(fmt::color::light_gray);
    // deepskyblue1 (xterm 39), matching the banner the shipping console uses.
    inline const auto appTitleTextStyle = fmt::fg(fmt::rgb(0x00, 0xAF, 0xFF));
    inline const auto infoTextStyle = fmt::fg(fmt::color::steel_blue);
    inline const auto errorTextStyle = fmt::fg(fmt::color::pink);
    inline const auto warningTextStyle = fmt::fg(fmt::color::light_yellow);
    inline const auto successTextStyle = fmt::fg(fmt::color::green_yellow);
    inline const auto separatorTextStyle = fmt::fg(fmt::color::gray);
    inline const auto commentTextStyle = fmt::fg(fmt::color::light_yellow);

    inline const auto tableTitleTextStyle = fmt::fg(fmt::color::deep_sky_blue);
    inline const auto tableHeaderTextStyle = fmt::fg(fmt::color::sky_blue);
    inline const auto sectionHeadingTextStyle = fmt::fg(fmt::color::deep_sky_blue);
    inline const auto fieldLabelTextStyle = fmt::fg(fmt::color::dark_golden_rod);
    inline const auto fieldValueTextStyle = fmt::fg(fmt::color::light_gray);

    inline const auto endpointNameTextStyle = fmt::fg(fmt::color::light_sky_blue);
    inline const auto endpointIdTextStyle = fmt::fg(fmt::color::olive);
    inline const auto transportCodeTextStyle = fmt::fg(fmt::color::steel_blue);
    inline const auto manufacturerTextStyle = fmt::fg(fmt::color::slate_gray);
    inline const auto midi2IndicatorTextStyle = fmt::fg(fmt::color::gray);

    inline const auto guidTextStyle = fmt::fg(fmt::color::dark_sea_green);
    inline const auto numberTextStyle = fmt::fg(fmt::color::olive);
    inline const auto timestampTextStyle = fmt::fg(fmt::color::dark_sea_green);
    inline const auto fileNameTextStyle = fmt::fg(fmt::color::sky_blue);
    inline const auto processNameTextStyle = fmt::fg(fmt::color::light_sky_blue);
    inline const auto propertyKeyTextStyle = fmt::fg(fmt::color::dark_olive_green);
    inline const auto booleanTrueTextStyle = fmt::fg(fmt::color::green_yellow);
    inline const auto booleanFalseTextStyle = fmt::fg(fmt::color::slate_gray);
    inline const auto groupTextStyle = fmt::fg(fmt::color::indian_red);
    inline const auto channelTextStyle = fmt::fg(fmt::color::medium_orchid);
    inline const auto messageTypeTextStyle = fmt::fg(fmt::color::light_sky_blue);
    inline const auto dataWordTextStyle = fmt::fg(fmt::color::deep_sky_blue);

    void WriteInfoLine(_In_ std::string_view text);
    void WriteNormalLine(_In_ std::string_view text);
    void WriteErrorLine(_In_ std::string_view text);
    void WriteWarningLine(_In_ std::string_view text);
    void WriteSuccessLine(_In_ std::string_view text);

    // ALWAYS terminate console lines with this. fmt::print writes to a console through
    // WriteConsoleW, bypassing the CRT, so a bare \n gets no carriage return and every line
    // starts where the last one ended.
    void WriteLine(_In_ std::string_view text);
    void WriteBlankLine();

    void SetConsoleTitleText(_In_ std::string_view title);

    // Erases an FTXUI frame that was just drawn above the cursor. frameHeight comes from
    // ScreenInteractive::dimy().
    //
    // This MUST use cursor-relative VT sequences, not the Win32 buffer API. Under a conpty the
    // console buffer is only as tall as the visible window - scrollback belongs to the terminal -
    // so once output reaches the bottom the cursor is pinned at the last row and the frame's rows
    // are no longer addressable by absolute coordinates.
    void ErasePopupFrame(_In_ int frameHeight);
}
