// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The library's patches, as tiles or as a list, the tile that makes a new one, the filter with
// its counts, and the bar along the bottom.

#include "pch.h"
#include "LibraryWindow.xaml.h"

#include "App.xaml.h"
#include "MidiServiceStatus.h"
#include "PatchCanvas.h"
#include "PatchLayout.h"
#include "RoundedShape.h"
#include "StringResources.h"

using namespace winrt::Microsoft::UI::Xaml;

namespace patchbay = ::midipatchbay;
namespace resources = ::midipatchbay::resources;

namespace winrt::midipatchbay::implementation
{
    namespace
    {
        constexpr double TileWidth = 296.0;
        constexpr double TileHeight = 212.0;
        constexpr double TileCornerRadius = 8.0;
        constexpr double MapHeight = 118.0;

        // Room kept clear round a tile's map, for the rate in its corner and the buttons along
        // its foot.
        constexpr double MapInsetX = 24.0;
        constexpr double MapInsetY = 26.0;

        constexpr double RowHeight = 64.0;
        constexpr double RowCornerRadius = 6.0;
        constexpr double RowMapWidth = 96.0;
        constexpr double RowMapHeight = 52.0;

        // The space the item container leaves to the right of each tile or row.
        constexpr double ItemGap = 14.0;

        // A map is a sketch of the patch, so its nodes are never drawn larger than this, however
        // few there are.
        constexpr double MapMaximumScale = 0.2;

        // The service is asked about every few ticks of the refresh timer rather than on every one.
        constexpr uint32_t ServiceCheckEveryTicks = 6;

        constexpr int64_t SecondsPerMinute = 60;
        constexpr int64_t SecondsPerHour = 60 * SecondsPerMinute;
        constexpr int64_t SecondsPerDay = 24 * SecondsPerHour;

        // Between 1601 and 1970, in the 100 ns ticks a FILETIME counts.
        constexpr int64_t UnixEpochTicks = 116444736000000000LL;

        // The tag of the tile that makes a new patch. Patch keys are GUIDs, so none can be this.
        constexpr wchar_t NewTileKey[] = L"new";

        media::Brush Brush(_In_ std::wstring_view key) noexcept
        {
            return patchbay::ThemeBrushes::Current().Get(key);
        }

        // Case-insensitive, the way a person expects a search box to work.
        bool ContainsText(_In_ std::wstring_view text, _In_ std::wstring_view search) noexcept
        {
            if (search.empty())
            {
                return true;
            }

            if (text.empty())
            {
                return false;
            }

            return ::FindNLSStringEx(
                LOCALE_NAME_USER_DEFAULT,
                FIND_FROMSTART | LINGUISTIC_IGNORECASE,
                text.data(), static_cast<int>(text.size()),
                search.data(), static_cast<int>(search.size()),
                nullptr, nullptr, nullptr, 0) >= 0;
        }

        // DoubleCollection has no initializer list constructor in this projection.
        media::DoubleCollection MakeDashArray(_In_ double on, _In_ double off) noexcept
        {
            media::DoubleCollection collection{};

            collection.Append(on);
            collection.Append(off);

            return collection;
        }

        std::wstring FormatDate(_In_ int64_t secondsSince1970, _In_ wchar_t const* picture) noexcept
        {
            try
            {
                auto const ticks = secondsSince1970 * 10'000'000LL + UnixEpochTicks;

                FILETIME file{};
                file.dwLowDateTime = static_cast<DWORD>(ticks & 0xFFFFFFFF);
                file.dwHighDateTime = static_cast<DWORD>(static_cast<uint64_t>(ticks) >> 32);

                FILETIME local{};
                SYSTEMTIME system{};

                if (!::FileTimeToLocalFileTime(&file, &local) || !::FileTimeToSystemTime(&local, &system))
                {
                    return {};
                }

                wchar_t buffer[96]{};

                if (::GetDateFormatEx(LOCALE_NAME_USER_DEFAULT, 0, &system, picture, buffer, ARRAYSIZE(buffer), nullptr) == 0)
                {
                    return {};
                }

                return buffer;
            }
            catch (...)
            {
                return {};
            }
        }

        // "2 hours ago", "Yesterday", "Sunday", "Sep 30", the same as MIDI Glass. Nobody should
        // have to read a timestamp to know which patch they changed this morning.
        std::wstring RelativeDate(_In_ int64_t secondsSince1970) noexcept
        {
            try
            {
                if (secondsSince1970 <= 0)
                {
                    return {};
                }

                auto const elapsed = static_cast<int64_t>(::_time64(nullptr)) - secondsSince1970;

                if (elapsed < SecondsPerMinute)
                {
                    return std::wstring{ resources::GetString(L"DateJustNow") };
                }

                if (elapsed < SecondsPerHour)
                {
                    auto const minutes = static_cast<int32_t>(elapsed / SecondsPerMinute);

                    return std::wstring{ minutes == 1
                        ? resources::GetString(L"DateMinuteAgo")
                        : resources::FormatString(L"DateMinutesAgo", minutes) };
                }

                if (elapsed < SecondsPerDay)
                {
                    auto const hours = static_cast<int32_t>(elapsed / SecondsPerHour);

                    return std::wstring{ hours == 1
                        ? resources::GetString(L"DateHourAgo")
                        : resources::FormatString(L"DateHoursAgo", hours) };
                }

                if (elapsed < SecondsPerDay * 2)
                {
                    return std::wstring{ resources::GetString(L"DateYesterday") };
                }

                // Inside the last week the weekday is what people remember it by.
                if (elapsed < SecondsPerDay * 7)
                {
                    if (auto const weekday = FormatDate(secondsSince1970, L"dddd"); !weekday.empty())
                    {
                        return weekday;
                    }
                }

                return FormatDate(secondsSince1970, L"MMM d");
            }
            catch (...)
            {
                return {};
            }
        }

        enum class ChipTone
        {
            Success,
            Caution,
            Critical,
            Neutral,
        };

        struct PatchStatus
        {
            ChipTone Tone{ ChipTone::Neutral };
            winrt::hstring Text{};
        };

        // Whether its devices are here, or what is stopping it, in the same words MIDI Glass uses.
        PatchStatus DescribeStatus(_In_ patchbay::PatchDocument const& patch) noexcept
        {
            try
            {
                auto& library = patchbay::PatchLibrary::Current();

                if (library.Problem(patch.SessionKey).has_value())
                {
                    return { ChipTone::Critical, resources::GetString(L"TileStateProblem") };
                }

                if (auto const& analysis = library.Analysis(patch.SessionKey); analysis.HasCertainLoop())
                {
                    return { ChipTone::Critical, resources::FormatString(L"TileLoopFormat",
                        static_cast<int32_t>(analysis.LoopMutedConnectionIds.size())) };
                }

                auto const missing = std::count_if(patch.Endpoints.begin(), patch.Endpoints.end(),
                    [](patchbay::PatchEndpoint const& endpoint) { return !patchbay::ResolveEndpoint(endpoint).has_value(); });

                if (missing > 0)
                {
                    return { ChipTone::Caution, missing == 1
                        ? resources::GetString(L"TileOneDeviceMissing")
                        : resources::FormatString(L"TileDevicesMissingFormat", static_cast<int32_t>(missing)) };
                }

                if (patch.Endpoints.empty())
                {
                    return { ChipTone::Neutral, resources::GetString(L"TileNoDevices") };
                }

                return { ChipTone::Success, patch.Endpoints.size() == 1
                    ? resources::GetString(L"TileOneDeviceReady")
                    : resources::FormatString(L"TileDevicesReadyFormat", static_cast<int32_t>(patch.Endpoints.size())) };
            }
            catch (...)
            {
            }

            return {};
        }

        // A pill drawn as a shape rather than a Border, because a Border rounds its corner radius
        // to whole pixels along with its bounds and the arc comes out stepped. The icon says what
        // the color says, so the color is never the only thing saying it.
        xaml::UIElement MakeChip(_In_ ChipTone tone, _In_ winrt::hstring const& text) noexcept
        {
            controls::Grid chip{};

            try
            {
                std::wstring_view foreground{ L"TextFillColorSecondaryBrush" };
                std::wstring_view background{};
                std::wstring_view stroke{ L"ControlStrokeColorDefaultBrush" };
                wchar_t const* glyph{ nullptr };

                switch (tone)
                {
                case ChipTone::Success:
                    foreground = L"SystemFillColorSuccessBrush";
                    background = L"SystemFillColorSuccessBackgroundBrush";
                    stroke = foreground;
                    glyph = L"\uE73E";
                    break;

                case ChipTone::Caution:
                    foreground = L"SystemFillColorCautionBrush";
                    background = L"SystemFillColorCautionBackgroundBrush";
                    stroke = foreground;
                    glyph = L"\uE7BA";
                    break;

                case ChipTone::Critical:
                    foreground = L"SystemFillColorCriticalBrush";
                    background = L"SystemFillColorCriticalBackgroundBrush";
                    stroke = foreground;
                    glyph = L"\uE7BA";
                    break;

                default:
                    break;
                }

                chip.Height(22);
                chip.HorizontalAlignment(xaml::HorizontalAlignment::Left);
                chip.VerticalAlignment(xaml::VerticalAlignment::Center);

                shapes::Rectangle shape{};
                shape.RadiusX(11);
                shape.RadiusY(11);
                shape.StrokeThickness(1);
                shape.UseLayoutRounding(false);
                shape.Stroke(Brush(stroke));

                if (!background.empty())
                {
                    shape.Fill(Brush(background));
                }

                chip.Children().Append(shape);

                // A Grid rather than a StackPanel, so a long status ends in an ellipsis instead of
                // being cut off mid-letter.
                controls::Grid content{};
                content.ColumnSpacing(6);
                content.Padding(xaml::ThicknessHelper::FromLengths(9, 0, 9, 0));

                for (auto const width : { xaml::GridLength{ 0, xaml::GridUnitType::Auto },
                                          xaml::GridLength{ 1, xaml::GridUnitType::Star } })
                {
                    controls::ColumnDefinition column{};
                    column.Width(width);
                    content.ColumnDefinitions().Append(column);
                }

                if (glyph != nullptr)
                {
                    controls::FontIcon icon{};
                    icon.Glyph(glyph);
                    icon.FontSize(11);
                    icon.VerticalAlignment(xaml::VerticalAlignment::Center);
                    icon.Foreground(Brush(foreground));
                    content.Children().Append(icon);
                }

                controls::TextBlock label{};
                label.Text(text);
                label.FontSize(11);
                label.VerticalAlignment(xaml::VerticalAlignment::Center);
                label.TextTrimming(xaml::TextTrimming::CharacterEllipsis);
                label.Foreground(Brush(foreground));
                controls::Grid::SetColumn(label, 1);
                content.Children().Append(label);

                chip.Children().Append(content);
            }
            catch (...)
            {
            }

            return chip;
        }

        // Where a patch is never saved, so it goes when Patchbay closes. Dashed, because nothing
        // is kept yet.
        xaml::UIElement MakeNotSavedChip() noexcept
        {
            controls::Grid chip{};

            try
            {
                chip.Height(22);
                chip.VerticalAlignment(xaml::VerticalAlignment::Center);

                shapes::Rectangle shape{};
                shape.RadiusX(11);
                shape.RadiusY(11);
                shape.StrokeThickness(1);
                shape.StrokeDashArray(MakeDashArray(3.0, 2.0));
                shape.UseLayoutRounding(false);
                shape.Stroke(Brush(L"ControlStrongStrokeColorDefaultBrush"));
                chip.Children().Append(shape);

                controls::TextBlock label{};
                label.Text(resources::GetString(L"ChipUnsaved"));
                label.FontSize(11);
                label.Margin(xaml::ThicknessHelper::FromLengths(9, 0, 9, 0));
                label.VerticalAlignment(xaml::VerticalAlignment::Center);
                label.Foreground(Brush(L"TextFillColorSecondaryBrush"));
                chip.Children().Append(label);
            }
            catch (...)
            {
            }

            return chip;
        }

        // A button laid over a tile's map, solid so the map doesn't show through it.
        controls::Button MakeMapButton() noexcept
        {
            controls::Button button{};

            button.Height(28);
            button.MinWidth(0);
            button.BorderThickness(xaml::ThicknessHelper::FromUniformLength(1));
            button.BorderBrush(Brush(L"ControlStrongStrokeColorDefaultBrush"));
            button.Background(Brush(L"ControlSolidFillColorDefaultBrush"));

            return button;
        }

        // A tile or row is built inside its list item, so whether a focused element belongs to
        // one is found by walking up from it.
        bool IsInside(_In_ xaml::DependencyObject element, _In_ xaml::DependencyObject const& container) noexcept
        {
            try
            {
                for (int depth = 0; element != nullptr && depth < 32; depth++)
                {
                    if (element == container)
                    {
                        return true;
                    }

                    element = media::VisualTreeHelper::GetParent(element);
                }
            }
            catch (...)
            {
            }

            return false;
        }
    }

    // ------------------------------------------------------------------ setup

    void LibraryWindow::InitializeLibraryControls() noexcept
    {
        try
        {
            m_fillingSortSelector = true;
            auto const filled = wil::scope_exit([this]() { m_fillingSortSelector = false; });

            SortSelector().Items().Clear();
            SortSelector().Items().Append(winrt::box_value(resources::GetString(L"SortByLastChanged")));
            SortSelector().Items().Append(winrt::box_value(resources::GetString(L"SortByName")));
            SortSelector().SelectedIndex(
                patchbay::AppSettings::Current().SortOrder() == patchbay::PatchSortOrder::Name ? 1 : 0);

            ApplyViewMode();

            auto weak = get_weak();

            // Keyboard focus on a tile shows its buttons, the same as the pointer does.
            PatchGrid().GotFocus([weak](auto&&, xaml::RoutedEventArgs const& args)
                {
                    if (auto strong = weak.get())
                    {
                        auto const source = args.OriginalSource().try_as<xaml::DependencyObject>();

                        for (auto const& tile : strong->m_tiles)
                        {
                            if (tile.HoverBar != nullptr && tile.Root != nullptr)
                            {
                                tile.HoverBar.Opacity(IsInside(source, tile.Root) || IsInside(tile.Root, source) ? 1.0 : 0.0);
                            }
                        }
                    }
                });

            PatchGrid().LostFocus([weak](auto&&, auto&&)
                {
                    if (auto strong = weak.get())
                    {
                        for (auto const& tile : strong->m_tiles)
                        {
                            if (tile.HoverBar != nullptr)
                            {
                                tile.HoverBar.Opacity(0.0);
                            }
                        }
                    }
                });

            m_serviceRunning = midiapp::IsMidiServiceRunning();
            UpdateServiceChip();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to set up the library.")
    }

    void LibraryWindow::ApplyViewMode() noexcept
    {
        try
        {
            auto const list = patchbay::AppSettings::Current().LibraryShowsList();

            GridViewToggle().IsChecked(!list);
            ListViewToggle().IsChecked(list);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to show the library view choice.")
    }

    // ------------------------------------------------------------------ changes

    _Use_decl_annotations_
    void LibraryWindow::OnLibraryChanged(patchbay::LibraryChange change, std::wstring const& key) noexcept
    {
        UNREFERENCED_PARAMETER(key);

        try
        {
            if (m_closing)
            {
                return;
            }

            switch (change)
            {
            case patchbay::LibraryChange::Routing:
                // Only a filter by routing changes which tiles show.
                if (m_filter == LibraryFilter::Routing || m_filter == LibraryFilter::Stopped)
                {
                    RebuildTiles();
                }
                else
                {
                    RefreshTileStates();
                }

                UpdateTray();
                UpdateStatusStrip();
                break;

            case patchbay::LibraryChange::Activity:
                break;

            default:
                RebuildTiles();
                UpdateTray();
                UpdateStatusStrip();
                break;
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to catch up with a change to the patches.")
    }

    _Use_decl_annotations_
    bool LibraryWindow::NeedsAttention(patchbay::PatchDocument const& patch) noexcept
    {
        auto& library = patchbay::PatchLibrary::Current();

        return library.HasMissingEndpoint(patch.SessionKey) ||
            library.Problem(patch.SessionKey).has_value() ||
            library.Analysis(patch.SessionKey).HasCertainLoop();
    }

    void LibraryWindow::UpdateFilterCounts() noexcept
    {
        try
        {
            auto& library = patchbay::PatchLibrary::Current();
            auto const patches = library.Patches();

            int32_t routing{ 0 };
            int32_t attention{ 0 };

            for (auto const* patch : patches)
            {
                routing += library.IsRouting(patch->SessionKey) ? 1 : 0;
                attention += NeedsAttention(*patch) ? 1 : 0;
            }

            auto const stopped = static_cast<int32_t>(patches.size()) - routing;

            auto const label = [](primitives::ToggleButton const& segment, controls::TextBlock const& text, winrt::hstring const& value)
                {
                    text.Text(value);
                    xaml::Automation::AutomationProperties::SetName(segment, value);
                };

            label(FilterRoutingItem(), FilterRoutingText(), resources::FormatString(L"FilterRoutingFormat", routing));
            label(FilterStoppedItem(), FilterStoppedText(), resources::FormatString(L"FilterStoppedFormat", stopped));
            label(FilterAttentionItem(), FilterAttentionText(), resources::FormatString(L"FilterAttentionFormat", attention));

            PatchesCount().Text(winrt::to_hstring(patches.size()));
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to count the patches.")
    }

    // ------------------------------------------------------------------ the patches

    void LibraryWindow::RebuildTiles() noexcept
    {
        try
        {
            if (!m_loaded || m_closing)
            {
                return;
            }

            auto& library = patchbay::PatchLibrary::Current();
            auto const patches = library.Patches();
            auto const search = std::wstring{ SearchBox().Text() };

            UpdateFilterCounts();

            std::vector<patchbay::PatchDocument const*> shown{};

            for (auto const* patch : patches)
            {
                auto const routing = library.IsRouting(patch->SessionKey);

                if ((m_filter == LibraryFilter::Routing && !routing) ||
                    (m_filter == LibraryFilter::Stopped && routing) ||
                    (m_filter == LibraryFilter::Attention && !NeedsAttention(*patch)))
                {
                    continue;
                }

                if (!search.empty())
                {
                    auto matches = ContainsText(patch->Name, search) || ContainsText(patch->Description, search);

                    for (auto const& endpoint : patch->Endpoints)
                    {
                        matches = matches || ContainsText(endpoint.DisplayName, search);
                    }

                    if (!matches)
                    {
                        continue;
                    }
                }

                shown.push_back(patch);
            }

            if (patchbay::AppSettings::Current().SortOrder() == patchbay::PatchSortOrder::Name)
            {
                std::sort(shown.begin(), shown.end(),
                    [](patchbay::PatchDocument const* a, patchbay::PatchDocument const* b)
                    {
                        return ::CompareStringOrdinal(a->Name.c_str(), -1, b->Name.c_str(), -1, TRUE) == CSTR_LESS_THAN;
                    });
            }
            else
            {
                // A patch that was never saved has no time yet, so it sorts first, where the
                // customer just made it.
                std::stable_sort(shown.begin(), shown.end(),
                    [](patchbay::PatchDocument const* a, patchbay::PatchDocument const* b)
                    {
                        if (a->FilePath.empty() != b->FilePath.empty())
                        {
                            return a->FilePath.empty();
                        }

                        return a->ModifiedTimestamp > b->ModifiedTimestamp;
                    });
            }

            m_updatingTiles = true;
            auto const reset = wil::scope_exit([this]() { m_updatingTiles = false; });

            m_tiles.clear();
            PatchGrid().Items().Clear();

            auto const list = patchbay::AppSettings::Current().LibraryShowsList();

            for (auto const* patch : shown)
            {
                if (auto const element = list ? BuildListRow(*patch) : BuildTile(*patch))
                {
                    PatchGrid().Items().Append(element);
                }
            }

            // The way to a new patch sits with the patches, where the eye already is. Not under a
            // filter or a search, which would make it look like one of the results.
            if (!list && !patches.empty() && m_filter == LibraryFilter::All && search.empty())
            {
                if (auto const tile = BuildNewTile())
                {
                    PatchGrid().Items().Append(tile);
                }
            }

            if (list)
            {
                ApplyRowWidths();
            }

            // Rates are kept only for what is on screen.
            std::erase_if(m_rateSamples, [this](auto const& entry)
                {
                    return std::none_of(m_tiles.begin(), m_tiles.end(), [&entry](TileParts const& tile) { return tile.Key == entry.first; });
                });

            EmptyPanel().Visibility(patches.empty() ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
            NoMatchesText().Visibility(!patches.empty() && shown.empty()
                ? xaml::Visibility::Visible
                : xaml::Visibility::Collapsed);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to show the patches.")
    }

    void LibraryWindow::RefreshTileStates() noexcept
    {
        try
        {
            auto& library = patchbay::PatchLibrary::Current();

            m_updatingTiles = true;
            auto const reset = wil::scope_exit([this]() { m_updatingTiles = false; });

            auto const success = Brush(L"SystemFillColorSuccessBrush");
            auto const stroke = Brush(L"CardStrokeColorDefaultBrush");

            for (auto const& tile : m_tiles)
            {
                if (library.Find(tile.Key) == nullptr)
                {
                    continue;
                }

                auto const routing = library.IsRouting(tile.Key);

                if (tile.Routing != nullptr)
                {
                    tile.Routing.IsOn(routing);
                }

                if (tile.Outline != nullptr)
                {
                    tile.Outline.Stroke(routing ? success : stroke);
                }

                if (tile.RatePill != nullptr)
                {
                    tile.RatePill.Visibility(routing ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
                }
            }

            UpdateFilterCounts();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to refresh the patch tiles.")
    }

    void LibraryWindow::UpdateTileRates() noexcept
    {
        try
        {
            auto& library = patchbay::PatchLibrary::Current();
            auto const now = std::chrono::steady_clock::now();

            for (auto const& tile : m_tiles)
            {
                if (tile.RateText == nullptr)
                {
                    continue;
                }

                auto const delivered = library.Delivered(tile.Key);
                auto& sample = m_rateSamples[tile.Key];

                if (sample.At.time_since_epoch().count() != 0 && delivered >= sample.Delivered)
                {
                    auto const elapsed = std::chrono::duration<double>(now - sample.At).count();

                    if (elapsed > 0.05)
                    {
                        auto const rate = static_cast<uint64_t>((delivered - sample.Delivered) / elapsed);

                        tile.RateText.Text(resources::FormatString(L"TileRateFormat", rate));
                    }
                }

                sample.Delivered = delivered;
                sample.At = now;
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to show the patch rates.")
    }

    _Use_decl_annotations_
    void LibraryWindow::ShowHoverBar(std::wstring const& key, bool show) noexcept
    {
        try
        {
            for (auto const& tile : m_tiles)
            {
                if (tile.Key == key && tile.HoverBar != nullptr)
                {
                    tile.HoverBar.Opacity(show ? 1.0 : 0.0);
                }
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to show a tile's buttons.")
    }

    _Use_decl_annotations_
    xaml::UIElement LibraryWindow::BuildTile(patchbay::PatchDocument const& patch) noexcept
    {
        try
        {
            auto& library = patchbay::PatchLibrary::Current();
            auto const key = patch.SessionKey;
            auto const name = winrt::hstring{ patch.Name };
            auto const routing = library.IsRouting(key);
            auto const status = DescribeStatus(patch);
            auto const date = RelativeDate(patch.ModifiedTimestamp > 0 ? patch.ModifiedTimestamp : patch.CreatedTimestamp);
            auto weak = get_weak();

            TileParts parts{};
            parts.Key = key;

            controls::Grid tile{};

            tile.Width(TileWidth);
            tile.Height(TileHeight);
            tile.Tag(winrt::box_value(winrt::hstring{ key }));

            for (auto const height : { xaml::GridLength{ MapHeight, xaml::GridUnitType::Pixel },
                                       xaml::GridLength{ 1, xaml::GridUnitType::Star } })
            {
                controls::RowDefinition row{};
                row.Height(height);
                tile.RowDefinitions().Append(row);
            }

            // ---- the surface, with no edge of its own, so the map reaches the very edge
            auto const surface = patchbay::MakeRoundedShape(TileCornerRadius, Brush(L"CardBackgroundFillColorDefaultBrush"));
            controls::Grid::SetRowSpan(surface, 2);
            tile.Children().Append(surface);

            // ---- the map, with the tile's rounded top corners: rounded all round, a corner
            // taller than the map, and cut off square at its foot
            auto const mapArea = patchbay::MakeRoundedShape(TileCornerRadius, Brush(L"CanvasSurfaceBrush"));
            mapArea.Height(MapHeight + TileCornerRadius);
            mapArea.VerticalAlignment(xaml::VerticalAlignment::Top);

            media::RectangleGeometry mapFoot{};
            mapFoot.Rect(foundation::Rect{ 0, 0, static_cast<float>(TileWidth), static_cast<float>(MapHeight) });
            mapArea.Clip(mapFoot);
            tile.Children().Append(mapArea);

            controls::Grid map{};

            auto const sketch = BuildMiniMap(patch, TileWidth - 2 * MapInsetX, MapHeight - 2 * MapInsetY);

            if (auto const element = sketch.try_as<xaml::FrameworkElement>())
            {
                element.HorizontalAlignment(xaml::HorizontalAlignment::Center);
                element.VerticalAlignment(xaml::VerticalAlignment::Center);
            }

            map.Children().Append(sketch);

            // How much it is sending, while it routes.
            controls::Grid rate{};
            rate.Height(20);
            rate.Margin(xaml::ThicknessHelper::FromUniformLength(8));
            rate.HorizontalAlignment(xaml::HorizontalAlignment::Left);
            rate.VerticalAlignment(xaml::VerticalAlignment::Top);
            rate.Visibility(routing ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);

            shapes::Rectangle rateShape{};
            rateShape.RadiusX(10);
            rateShape.RadiusY(10);
            rateShape.StrokeThickness(1);
            rateShape.UseLayoutRounding(false);
            rateShape.Fill(Brush(L"SystemFillColorSuccessBackgroundBrush"));
            rateShape.Stroke(Brush(L"SystemFillColorSuccessBrush"));
            rate.Children().Append(rateShape);

            controls::StackPanel rateContent{};
            rateContent.Orientation(controls::Orientation::Horizontal);
            rateContent.Spacing(6);
            rateContent.Margin(xaml::ThicknessHelper::FromLengths(8, 0, 9, 0));

            shapes::Ellipse light{};
            light.Width(6);
            light.Height(6);
            light.VerticalAlignment(xaml::VerticalAlignment::Center);
            light.Fill(Brush(L"SystemFillColorSuccessBrush"));
            rateContent.Children().Append(light);

            controls::TextBlock rateText{};
            rateText.Text(resources::FormatString(L"TileRateFormat", 0));
            rateText.FontSize(10.5);
            rateText.VerticalAlignment(xaml::VerticalAlignment::Center);
            rateText.Foreground(Brush(L"SystemFillColorSuccessBrush"));
            rateContent.Children().Append(rateText);

            rate.Children().Append(rateContent);
            map.Children().Append(rate);

            // Edit and More, shown while the pointer or the keyboard is on the tile.
            controls::Grid bar{};
            bar.Height(40);
            bar.VerticalAlignment(xaml::VerticalAlignment::Bottom);
            bar.Padding(xaml::ThicknessHelper::FromLengths(8, 0, 8, 7));
            bar.Opacity(0.0);

            xaml::ScalarTransition fade{};
            fade.Duration(std::chrono::milliseconds{ 100 });
            bar.OpacityTransition(fade);

            for (auto const width : { xaml::GridLength{ 0, xaml::GridUnitType::Auto },
                                      xaml::GridLength{ 1, xaml::GridUnitType::Star },
                                      xaml::GridLength{ 0, xaml::GridUnitType::Auto } })
            {
                controls::ColumnDefinition column{};
                column.Width(width);
                bar.ColumnDefinitions().Append(column);
            }

            auto edit = MakeMapButton();
            edit.Padding(xaml::ThicknessHelper::FromLengths(10, 0, 10, 0));
            edit.VerticalAlignment(xaml::VerticalAlignment::Bottom);

            controls::StackPanel editContent{};
            editContent.Orientation(controls::Orientation::Horizontal);
            editContent.Spacing(6);

            controls::FontIcon editIcon{};
            editIcon.Glyph(L"\uE70F");
            editIcon.FontSize(12);
            editContent.Children().Append(editIcon);

            controls::TextBlock editText{};
            editText.Text(resources::GetString(L"TileEdit"));
            editText.FontSize(12);
            editContent.Children().Append(editText);

            edit.Content(editContent);
            xaml::Automation::AutomationProperties::SetName(edit, resources::FormatString(L"TileEditAccessibleFormat", patch.Name));

            edit.Click([weak, key](auto&&, auto&&)
                {
                    if (auto strong = weak.get())
                    {
                        strong->OpenPatch(key);
                    }
                });

            bar.Children().Append(edit);

            auto more = MakeMapButton();
            more.Width(32);
            more.Padding(xaml::ThicknessHelper::FromUniformLength(0));
            more.VerticalAlignment(xaml::VerticalAlignment::Bottom);

            controls::FontIcon moreIcon{};
            moreIcon.Glyph(L"\uE712");
            moreIcon.FontSize(12);
            more.Content(moreIcon);

            xaml::Automation::AutomationProperties::SetName(more, resources::FormatString(L"TileMoreAccessibleFormat", patch.Name));
            controls::ToolTipService::SetToolTip(more, winrt::box_value(resources::FormatString(L"TileMoreAccessibleFormat", patch.Name)));

            more.Click([weak, key](foundation::IInspectable const& sender, auto&&)
                {
                    auto strong = weak.get();
                    auto const anchor = sender.try_as<xaml::FrameworkElement>();

                    if (strong != nullptr && anchor != nullptr)
                    {
                        strong->BuildTileMenu(key).ShowAt(anchor);
                    }
                });

            controls::Grid::SetColumn(more, 2);
            bar.Children().Append(more);

            map.Children().Append(bar);

            shapes::Rectangle rule{};
            rule.Height(1);
            rule.VerticalAlignment(xaml::VerticalAlignment::Bottom);
            rule.Fill(Brush(L"DividerStrokeColorDefaultBrush"));
            map.Children().Append(rule);

            tile.Children().Append(map);

            // ---- name, description and status
            controls::Grid body{};
            body.Padding(xaml::ThicknessHelper::FromLengths(12, 7, 12, 10));

            for (auto const height : { xaml::GridLength{ 0, xaml::GridUnitType::Auto },
                                       xaml::GridLength{ 0, xaml::GridUnitType::Auto },
                                       xaml::GridLength{ 1, xaml::GridUnitType::Star },
                                       xaml::GridLength{ 0, xaml::GridUnitType::Auto } })
            {
                controls::RowDefinition row{};
                row.Height(height);
                body.RowDefinitions().Append(row);
            }

            controls::Grid header{};
            header.ColumnSpacing(8);

            for (auto const width : { xaml::GridLength{ 1, xaml::GridUnitType::Star },
                                      xaml::GridLength{ 0, xaml::GridUnitType::Auto } })
            {
                controls::ColumnDefinition column{};
                column.Width(width);
                header.ColumnDefinitions().Append(column);
            }

            controls::TextBlock title{};
            title.Text(name);
            title.FontSize(14);
            title.FontWeight(winrt::Microsoft::UI::Text::FontWeights::SemiBold());
            title.TextTrimming(xaml::TextTrimming::CharacterEllipsis);
            title.VerticalAlignment(xaml::VerticalAlignment::Center);
            header.Children().Append(title);

            controls::ToggleSwitch routingSwitch{};
            routingSwitch.IsOn(routing);
            routingSwitch.MinWidth(0);
            routingSwitch.OnContent(winrt::box_value(winrt::hstring{}));
            routingSwitch.OffContent(winrt::box_value(winrt::hstring{}));
            routingSwitch.VerticalAlignment(xaml::VerticalAlignment::Center);
            routingSwitch.Margin(xaml::ThicknessHelper::FromLengths(0, -4, -2, -4));

            xaml::Automation::AutomationProperties::SetName(routingSwitch,
                resources::FormatString(L"TileRoutingFormat", patch.Name));

            routingSwitch.Toggled([weak, key](foundation::IInspectable const& sender, auto&&)
                {
                    auto strong = weak.get();
                    auto const toggle = sender.try_as<controls::ToggleSwitch>();

                    if (strong == nullptr || toggle == nullptr || strong->m_updatingTiles)
                    {
                        return;
                    }

                    patchbay::PatchLibrary::Current().SetRouting(key, toggle.IsOn());
                });

            controls::Grid::SetColumn(routingSwitch, 1);
            header.Children().Append(routingSwitch);

            body.Children().Append(header);

            controls::TextBlock description{};
            description.Text(winrt::hstring{ patch.Description.empty()
                ? std::wstring{ resources::GetString(L"TileNoDescription") }
                : patch.Description });
            description.FontSize(12);
            description.Margin(xaml::ThicknessHelper::FromLengths(0, 1, 0, 0));
            description.TextTrimming(xaml::TextTrimming::CharacterEllipsis);
            description.Foreground(Brush(L"TextFillColorTertiaryBrush"));
            controls::Grid::SetRow(description, 1);
            body.Children().Append(description);

            controls::Grid footer{};
            footer.ColumnSpacing(6);

            for (auto const width : { xaml::GridLength{ 0, xaml::GridUnitType::Auto },
                                      xaml::GridLength{ 0, xaml::GridUnitType::Auto },
                                      xaml::GridLength{ 0, xaml::GridUnitType::Auto },
                                      xaml::GridLength{ 1, xaml::GridUnitType::Star },
                                      xaml::GridLength{ 0, xaml::GridUnitType::Auto } })
            {
                controls::ColumnDefinition column{};
                column.Width(width);
                footer.ColumnDefinitions().Append(column);
            }

            footer.Children().Append(MakeChip(status.Tone, status.Text));

            if (patch.FilePath.empty())
            {
                auto notSaved = MakeNotSavedChip();
                controls::Grid::SetColumn(notSaved.as<xaml::FrameworkElement>(), 1);
                footer.Children().Append(notSaved);
            }
            else if (patch.ActivateAtStartup)
            {
                controls::FontIcon power{};
                power.Glyph(L"\uE7E8");
                power.FontSize(12);
                power.VerticalAlignment(xaml::VerticalAlignment::Center);
                power.Foreground(Brush(L"TextFillColorTertiaryBrush"));
                controls::ToolTipService::SetToolTip(power, winrt::box_value(resources::GetString(L"TileStartsAutomatically")));
                xaml::Automation::AutomationProperties::SetName(power, resources::GetString(L"TileStartsAutomatically"));
                controls::Grid::SetColumn(power, 2);
                footer.Children().Append(power);
            }

            controls::TextBlock when{};
            when.Text(winrt::hstring{ date });
            when.FontSize(11);
            when.VerticalAlignment(xaml::VerticalAlignment::Center);
            when.Foreground(Brush(L"TextFillColorTertiaryBrush"));
            controls::Grid::SetColumn(when, 4);
            footer.Children().Append(when);

            controls::Grid::SetRow(footer, 3);
            body.Children().Append(footer);

            controls::Grid::SetRow(body, 1);
            tile.Children().Append(body);

            // ---- the edge, drawn over everything, and green while it routes
            auto const outline = patchbay::MakeRoundedShape(TileCornerRadius, nullptr,
                Brush(routing ? L"SystemFillColorSuccessBrush" : L"CardStrokeColorDefaultBrush"));
            outline.IsHitTestVisible(false);
            controls::Grid::SetRowSpan(outline, 2);
            tile.Children().Append(outline);

            tile.PointerEntered([weak, key](auto&&, auto&&)
                {
                    if (auto strong = weak.get())
                    {
                        strong->ShowHoverBar(key, true);
                    }
                });

            tile.PointerExited([weak, key](auto&&, auto&&)
                {
                    if (auto strong = weak.get())
                    {
                        strong->ShowHoverBar(key, false);
                    }
                });

            tile.ContextFlyout(BuildTileMenu(key));

            if (!patch.Description.empty())
            {
                controls::ToolTipService::SetToolTip(tile, winrt::box_value(winrt::hstring{ patch.Description }));
            }

            xaml::Automation::AutomationProperties::SetName(tile,
                resources::FormatString(L"TileAccessibleNameFormat",
                    patch.Name,
                    resources::GetString(routing ? L"ChipRouting" : L"ChipNotRouting"),
                    status.Text,
                    date));

            parts.Routing = routingSwitch;
            parts.Outline = outline;
            parts.RatePill = rate;
            parts.RateText = rateText;
            parts.HoverBar = bar;
            parts.Root = tile;

            m_tiles.push_back(std::move(parts));

            return tile;
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to build a patch tile.")

        return nullptr;
    }

    _Use_decl_annotations_
    xaml::UIElement LibraryWindow::BuildListRow(patchbay::PatchDocument const& patch) noexcept
    {
        try
        {
            auto& library = patchbay::PatchLibrary::Current();
            auto const key = patch.SessionKey;
            auto const name = winrt::hstring{ patch.Name };
            auto const routing = library.IsRouting(key);
            auto const status = DescribeStatus(patch);
            auto const date = RelativeDate(patch.ModifiedTimestamp > 0 ? patch.ModifiedTimestamp : patch.CreatedTimestamp);
            auto weak = get_weak();

            TileParts parts{};
            parts.Key = key;

            controls::Grid row{};
            row.Height(RowHeight);
            row.Tag(winrt::box_value(winrt::hstring{ key }));

            row.Children().Append(patchbay::MakeRoundedShape(RowCornerRadius, Brush(L"CardBackgroundFillColorDefaultBrush")));

            controls::Grid content{};
            content.Padding(xaml::ThicknessHelper::FromLengths(6, 0, 10, 0));
            content.ColumnSpacing(12);

            for (auto const width : { xaml::GridLength{ 0, xaml::GridUnitType::Auto },
                                      xaml::GridLength{ 1, xaml::GridUnitType::Star },
                                      xaml::GridLength{ 0, xaml::GridUnitType::Auto },
                                      xaml::GridLength{ 0, xaml::GridUnitType::Auto },
                                      xaml::GridLength{ 0, xaml::GridUnitType::Auto },
                                      xaml::GridLength{ 0, xaml::GridUnitType::Auto } })
            {
                controls::ColumnDefinition column{};
                column.Width(width);
                content.ColumnDefinitions().Append(column);
            }

            // ---- the map, small
            auto const sketch = BuildMiniMap(patch, RowMapWidth - 10, RowMapHeight - 10);

            if (auto const element = sketch.try_as<xaml::FrameworkElement>())
            {
                element.HorizontalAlignment(xaml::HorizontalAlignment::Center);
                element.VerticalAlignment(xaml::VerticalAlignment::Center);
            }

            auto const map = patchbay::MakeRoundedPanel(4, Brush(L"CanvasSurfaceBrush"), nullptr, xaml::Thickness{}, sketch).Panel;
            map.Width(RowMapWidth);
            map.Height(RowMapHeight);
            map.VerticalAlignment(xaml::VerticalAlignment::Center);
            content.Children().Append(map);

            // ---- name, description and size
            controls::StackPanel text{};
            text.VerticalAlignment(xaml::VerticalAlignment::Center);

            controls::TextBlock title{};
            title.Text(name);
            title.FontSize(13);
            title.FontWeight(winrt::Microsoft::UI::Text::FontWeights::SemiBold());
            title.TextTrimming(xaml::TextTrimming::CharacterEllipsis);
            text.Children().Append(title);

            controls::TextBlock description{};
            description.Text(winrt::hstring{ patch.Description.empty()
                ? std::wstring{ resources::GetString(L"TileNoDescription") }
                : patch.Description });
            description.FontSize(11);
            description.TextTrimming(xaml::TextTrimming::CharacterEllipsis);
            description.Foreground(Brush(L"TextFillColorSecondaryBrush"));
            text.Children().Append(description);

            controls::TextBlock size{};
            size.Text(resources::FormatString(L"TileSizeFormat", patch.StepCount(), patch.Connections.size()));
            size.FontSize(11);
            size.TextTrimming(xaml::TextTrimming::CharacterEllipsis);
            size.Foreground(Brush(L"TextFillColorTertiaryBrush"));
            text.Children().Append(size);

            controls::Grid::SetColumn(text, 1);
            content.Children().Append(text);

            // ---- status, then not saved or starts automatically
            controls::StackPanel chips{};
            chips.Orientation(controls::Orientation::Horizontal);
            chips.Spacing(6);
            chips.VerticalAlignment(xaml::VerticalAlignment::Center);
            chips.Children().Append(MakeChip(status.Tone, status.Text));

            if (patch.FilePath.empty())
            {
                chips.Children().Append(MakeNotSavedChip());
            }
            else
            {
                // There on every saved row, and seen only on the ones that start by themselves,
                // so the columns after it line up.
                controls::FontIcon power{};
                power.Glyph(L"\uE7E8");
                power.FontSize(12);
                power.VerticalAlignment(xaml::VerticalAlignment::Center);
                power.Foreground(Brush(L"TextFillColorTertiaryBrush"));

                if (patch.ActivateAtStartup)
                {
                    controls::ToolTipService::SetToolTip(power, winrt::box_value(resources::GetString(L"TileStartsAutomatically")));
                    xaml::Automation::AutomationProperties::SetName(power, resources::GetString(L"TileStartsAutomatically"));
                }
                else
                {
                    power.Opacity(0.0);
                    xaml::Automation::AutomationProperties::SetAccessibilityView(power, xaml::Automation::Peers::AccessibilityView::Raw);
                }

                chips.Children().Append(power);
            }

            controls::Grid::SetColumn(chips, 2);
            content.Children().Append(chips);

            controls::TextBlock when{};
            when.Text(winrt::hstring{ date });
            when.FontSize(11);
            when.MinWidth(84);
            when.TextAlignment(xaml::TextAlignment::Right);
            when.VerticalAlignment(xaml::VerticalAlignment::Center);
            when.Foreground(Brush(L"TextFillColorTertiaryBrush"));
            controls::Grid::SetColumn(when, 3);
            content.Children().Append(when);

            controls::ToggleSwitch routingSwitch{};
            routingSwitch.IsOn(routing);
            routingSwitch.MinWidth(0);
            routingSwitch.OnContent(winrt::box_value(winrt::hstring{}));
            routingSwitch.OffContent(winrt::box_value(winrt::hstring{}));
            routingSwitch.VerticalAlignment(xaml::VerticalAlignment::Center);

            xaml::Automation::AutomationProperties::SetName(routingSwitch,
                resources::FormatString(L"TileRoutingFormat", patch.Name));

            routingSwitch.Toggled([weak, key](foundation::IInspectable const& sender, auto&&)
                {
                    auto strong = weak.get();
                    auto const toggle = sender.try_as<controls::ToggleSwitch>();

                    if (strong == nullptr || toggle == nullptr || strong->m_updatingTiles)
                    {
                        return;
                    }

                    patchbay::PatchLibrary::Current().SetRouting(key, toggle.IsOn());
                });

            controls::Grid::SetColumn(routingSwitch, 4);
            content.Children().Append(routingSwitch);

            controls::Button more{};
            more.Width(32);
            more.Height(28);
            more.Padding(xaml::ThicknessHelper::FromUniformLength(0));
            more.VerticalAlignment(xaml::VerticalAlignment::Center);

            controls::FontIcon moreIcon{};
            moreIcon.Glyph(L"\uE712");
            moreIcon.FontSize(12);
            more.Content(moreIcon);

            xaml::Automation::AutomationProperties::SetName(more, resources::FormatString(L"TileMoreAccessibleFormat", patch.Name));
            controls::ToolTipService::SetToolTip(more, winrt::box_value(resources::FormatString(L"TileMoreAccessibleFormat", patch.Name)));

            more.Click([weak, key](foundation::IInspectable const& sender, auto&&)
                {
                    auto strong = weak.get();
                    auto const anchor = sender.try_as<xaml::FrameworkElement>();

                    if (strong != nullptr && anchor != nullptr)
                    {
                        strong->BuildTileMenu(key).ShowAt(anchor);
                    }
                });

            controls::Grid::SetColumn(more, 5);
            content.Children().Append(more);

            row.Children().Append(content);

            auto const outline = patchbay::MakeRoundedShape(RowCornerRadius, nullptr,
                Brush(routing ? L"SystemFillColorSuccessBrush" : L"CardStrokeColorDefaultBrush"));
            outline.IsHitTestVisible(false);
            row.Children().Append(outline);

            row.ContextFlyout(BuildTileMenu(key));

            xaml::Automation::AutomationProperties::SetName(row,
                resources::FormatString(L"TileAccessibleNameFormat",
                    patch.Name,
                    resources::GetString(routing ? L"ChipRouting" : L"ChipNotRouting"),
                    status.Text,
                    date));

            parts.Routing = routingSwitch;
            parts.Outline = outline;
            parts.Root = row;

            m_tiles.push_back(std::move(parts));

            return row;
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to build a patch row.")

        return nullptr;
    }

    xaml::UIElement LibraryWindow::BuildNewTile() noexcept
    {
        try
        {
            controls::Grid tile{};

            tile.Width(TileWidth);
            tile.Height(TileHeight);
            tile.Tag(winrt::box_value(winrt::hstring{ NewTileKey }));

            // Dashed, because nothing is here yet.
            auto const outline = patchbay::MakeRoundedShape(TileCornerRadius, nullptr, Brush(L"ControlStrongStrokeColorDefaultBrush"));
            outline.StrokeDashArray(MakeDashArray(4.0, 3.0));
            outline.StrokeDashCap(media::PenLineCap::Flat);
            tile.Children().Append(outline);

            controls::StackPanel content{};
            content.Spacing(9);
            content.HorizontalAlignment(xaml::HorizontalAlignment::Center);
            content.VerticalAlignment(xaml::VerticalAlignment::Center);

            controls::FontIcon plus{};
            plus.Glyph(L"\uE710");
            plus.FontSize(24);
            plus.Foreground(Brush(L"AccentTextFillColorPrimaryBrush"));
            content.Children().Append(plus);

            controls::TextBlock heading{};
            heading.Text(resources::GetString(L"NewTileHeading"));
            heading.FontSize(13);
            heading.FontWeight(winrt::Microsoft::UI::Text::FontWeights::SemiBold());
            heading.HorizontalAlignment(xaml::HorizontalAlignment::Center);
            content.Children().Append(heading);

            controls::TextBlock body{};
            body.Text(resources::GetString(L"NewTileBody"));
            body.FontSize(11);
            body.Margin(xaml::ThicknessHelper::FromLengths(24, 0, 24, 0));
            body.TextAlignment(xaml::TextAlignment::Center);
            body.TextWrapping(xaml::TextWrapping::Wrap);
            body.Foreground(Brush(L"TextFillColorTertiaryBrush"));
            content.Children().Append(body);

            tile.Children().Append(content);

            xaml::Automation::AutomationProperties::SetName(tile, resources::GetString(L"NewTileAccessibleName"));

            return tile;
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to build the new patch tile.")

        return nullptr;
    }

    _Use_decl_annotations_
    void LibraryWindow::ShowNewPatchMenu(xaml::FrameworkElement const& anchor) noexcept
    {
        try
        {
            controls::MenuFlyout menu{};
            auto weak = get_weak();

            controls::MenuFlyoutItem empty{};
            empty.Text(resources::GetString(L"NewTileMenuEmpty"));

            controls::FontIcon emptyIcon{};
            emptyIcon.Glyph(L"\uE710");
            empty.Icon(emptyIcon);

            empty.Click([weak](auto&&, auto&&)
                {
                    if (auto strong = weak.get())
                    {
                        strong->OnNewPatchClick(nullptr, nullptr);
                    }
                });

            menu.Items().Append(empty);

            controls::MenuFlyoutItem quick{};
            quick.Text(resources::GetString(L"NewTileMenuQuick"));

            controls::FontIcon quickIcon{};
            quickIcon.Glyph(L"\uE71B");
            quick.Icon(quickIcon);

            quick.Click([weak](auto&&, auto&&)
                {
                    if (auto strong = weak.get())
                    {
                        strong->ShowQuickPatchDialogAsync();
                    }
                });

            menu.Items().Append(quick);

            menu.ShowAt(anchor);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to show the new patch choices.")
    }

    void LibraryWindow::ApplyRowWidths() noexcept
    {
        try
        {
            auto const grid = PatchGrid();
            auto const padding = grid.Padding();

            // A little short of the room, so a rounding pixel never wraps a row onto two lines.
            auto const width = grid.ActualWidth() - padding.Left - padding.Right - ItemGap - 2.0;

            if (width <= 0)
            {
                return;
            }

            for (auto const& tile : m_tiles)
            {
                if (tile.Root != nullptr)
                {
                    tile.Root.Width(width);
                }
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to size the patch rows.")
    }

    _Use_decl_annotations_
    xaml::UIElement LibraryWindow::BuildMiniMap(patchbay::PatchDocument const& patch, double width, double height) noexcept
    {
        controls::Canvas canvas{};

        try
        {
            canvas.Width(width);
            canvas.Height(height);
            canvas.IsHitTestVisible(false);

            struct Box
            {
                std::wstring Id{};
                double X{ 0 };
                double Y{ 0 };
                double Width{ 0 };
                double Height{ 0 };
                std::optional<patchbay::BlockCategory> Category{};
                bool Missing{ false };
            };

            std::vector<Box> boxes{};

            for (auto const& endpoint : patch.Endpoints)
            {
                auto const size = patchbay::EstimatedNodeSize(patch, endpoint.Id);
                boxes.push_back(Box{ endpoint.Id, endpoint.CanvasX, endpoint.CanvasY, size.Width, size.Height,
                    std::nullopt, !patchbay::ResolveEndpoint(endpoint).has_value() });
            }

            // Notes on the canvas route nothing, so the sketch leaves them out.
            for (auto const& block : patch.Blocks)
            {
                if (patchbay::IsAnnotation(block.Kind))
                {
                    continue;
                }

                auto const size = patchbay::EstimatedNodeSize(patch, block.Id);
                boxes.push_back(Box{ block.Id, block.CanvasX, block.CanvasY, size.Width, size.Height,
                    patchbay::CategoryOf(block.Kind), false });
            }

            if (boxes.empty())
            {
                return canvas;
            }

            auto left = std::numeric_limits<double>::max();
            auto top = std::numeric_limits<double>::max();
            auto right = std::numeric_limits<double>::lowest();
            auto bottom = std::numeric_limits<double>::lowest();

            for (auto const& box : boxes)
            {
                left = (std::min)(left, box.X);
                top = (std::min)(top, box.Y);
                right = (std::max)(right, box.X + box.Width);
                bottom = (std::max)(bottom, box.Y + box.Height);
            }

            auto const scale = (std::min)({
                width / (std::max)(right - left, 1.0),
                height / (std::max)(bottom - top, 1.0),
                MapMaximumScale });

            auto const offsetX = (width - (right - left) * scale) / 2 - left * scale;
            auto const offsetY = (height - (bottom - top) * scale) / 2 - top * scale;

            auto const find = [&boxes](std::wstring const& id) -> Box const*
                {
                    auto const found = std::find_if(boxes.begin(), boxes.end(), [&id](Box const& b) { return b.Id == id; });
                    return found == boxes.end() ? nullptr : &(*found);
                };

            auto const lineBrush = Brush(L"TextFillColorTertiaryBrush");

            // Out on the right of the source, in on the left of the destination, like the canvas.
            for (auto const& link : patch.Connections)
            {
                auto const* source = find(link.SourceId);
                auto const* destination = find(link.DestinationId);

                if (source == nullptr || destination == nullptr)
                {
                    continue;
                }

                shapes::Line line{};

                line.X1((source->X + source->Width) * scale + offsetX);
                line.Y1((source->Y + source->Height / 2) * scale + offsetY);
                line.X2(destination->X * scale + offsetX);
                line.Y2((destination->Y + destination->Height / 2) * scale + offsetY);
                line.StrokeThickness(1);
                line.Stroke(lineBrush);
                line.Opacity(link.Muted ? 0.3 : 0.8);

                canvas.Children().Append(line);
            }

            for (auto const& box : boxes)
            {
                shapes::Rectangle node{};

                node.Width((std::max)(box.Width * scale, 3.0));
                node.Height((std::max)(box.Height * scale, 3.0));
                node.RadiusX(2);
                node.RadiusY(2);
                node.UseLayoutRounding(false);

                if (box.Category.has_value())
                {
                    node.Fill(patchbay::PatchCanvas::CategoryBrush(box.Category.value(), 0.85));
                }
                else if (box.Missing)
                {
                    // Dashed and red, like a device that isn't here on the canvas.
                    node.Fill(Brush(L"SystemFillColorCriticalBackgroundBrush"));
                    node.Stroke(Brush(L"SystemFillColorCriticalBrush"));
                    node.StrokeThickness(1);
                    node.StrokeDashArray(MakeDashArray(3.0, 2.0));
                }
                else
                {
                    node.Fill(Brush(L"ControlAltFillColorSecondaryBrush"));
                    node.Stroke(Brush(L"TextFillColorTertiaryBrush"));
                    node.StrokeThickness(1);
                }

                controls::Canvas::SetLeft(node, box.X * scale + offsetX);
                controls::Canvas::SetTop(node, box.Y * scale + offsetY);

                canvas.Children().Append(node);
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to draw a patch map.")

        return canvas;
    }

    _Use_decl_annotations_
    controls::MenuFlyout LibraryWindow::BuildTileMenu(std::wstring const& key) noexcept
    {
        controls::MenuFlyout menu{};

        try
        {
            auto weak = get_weak();

            auto const addItem = [&menu, weak](winrt::hstring const& text, std::function<void(LibraryWindow&)> action)
                {
                    controls::MenuFlyoutItem item{};

                    item.Text(text);
                    item.Click([weak, action](auto&&, auto&&)
                        {
                            if (auto strong = weak.get())
                            {
                                action(*strong);
                            }
                        });

                    menu.Items().Append(item);
                };

            addItem(resources::GetString(L"TileMenuOpen"), [key](LibraryWindow& window) { window.OpenPatch(key); });

            // Worked out when the menu opens, so it reads right whatever happened since the tile
            // was built.
            controls::ToggleMenuFlyoutItem routingItem{};
            routingItem.Text(resources::GetString(L"MenuRouteThisPatch"));
            routingItem.Click([key](foundation::IInspectable const& sender, auto&&)
                {
                    if (auto const item = sender.try_as<controls::ToggleMenuFlyoutItem>())
                    {
                        patchbay::PatchLibrary::Current().SetRouting(key, item.IsChecked());
                    }
                });

            menu.Items().Append(routingItem);

            // Weak, because the item is in the menu and the menu keeps this handler.
            menu.Opening([key, item = winrt::make_weak(routingItem)](auto&&, auto&&)
                {
                    if (auto const routing = item.get())
                    {
                        routing.IsChecked(patchbay::PatchLibrary::Current().IsRouting(key));
                    }
                });

            menu.Items().Append(controls::MenuFlyoutSeparator{});

            addItem(resources::GetString(L"TileMenuDuplicate"), [key](LibraryWindow& window) { window.DuplicatePatch(key); });

            if (auto const* patch = patchbay::PatchLibrary::Current().Find(key); patch != nullptr && !patch->FilePath.empty())
            {
                auto const path = patch->FilePath;
                addItem(resources::GetString(L"TileMenuShowInFolder"), [path](LibraryWindow& window) { window.ShowInFolder(path); });
            }

            menu.Items().Append(controls::MenuFlyoutSeparator{});

            addItem(resources::GetString(L"MenuDeletePatch"), [key](LibraryWindow& window) { window.DeletePatchAsync(key); });
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to build a patch menu.")

        return menu;
    }

    // ------------------------------------------------------------------ the toolbar and filter

    _Use_decl_annotations_
    void LibraryWindow::OnSearchTextChanged(
        controls::AutoSuggestBox const& sender,
        controls::AutoSuggestBoxTextChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        RebuildTiles();
    }

    _Use_decl_annotations_
    void LibraryWindow::OnSortSelectionChanged(foundation::IInspectable const& sender, controls::SelectionChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        try
        {
            if (m_fillingSortSelector || SortSelector().SelectedIndex() < 0)
            {
                return;
            }

            auto const order = SortSelector().SelectedIndex() == 1
                ? patchbay::PatchSortOrder::Name
                : patchbay::PatchSortOrder::Newest;

            if (order == patchbay::AppSettings::Current().SortOrder())
            {
                return;
            }

            patchbay::AppSettings::Current().SortOrder(order);
            RebuildTiles();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to change the sort order.")
    }

    _Use_decl_annotations_
    void LibraryWindow::OnGridViewToggled(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        patchbay::AppSettings::Current().LibraryShowsList(false);
        ApplyViewMode();
        RebuildTiles();
    }

    _Use_decl_annotations_
    void LibraryWindow::OnListViewToggled(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        patchbay::AppSettings::Current().LibraryShowsList(true);
        ApplyViewMode();
        RebuildTiles();
    }

    _Use_decl_annotations_
    void LibraryWindow::OnFilterClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(args);

        try
        {
            auto filter = LibraryFilter::All;

            if (sender == FilterRoutingItem())
            {
                filter = LibraryFilter::Routing;
            }
            else if (sender == FilterStoppedItem())
            {
                filter = LibraryFilter::Stopped;
            }
            else if (sender == FilterAttentionItem())
            {
                filter = LibraryFilter::Attention;
            }

            // One part is always chosen, even the one clicked again.
            FilterAllItem().IsChecked(filter == LibraryFilter::All);
            FilterRoutingItem().IsChecked(filter == LibraryFilter::Routing);
            FilterStoppedItem().IsChecked(filter == LibraryFilter::Stopped);
            FilterAttentionItem().IsChecked(filter == LibraryFilter::Attention);

            if (filter != m_filter)
            {
                m_filter = filter;
                RebuildTiles();
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to filter the patches.")
    }

    _Use_decl_annotations_
    void LibraryWindow::OnPatchTileClick(foundation::IInspectable const& sender, controls::ItemClickEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);

        try
        {
            auto const element = args.ClickedItem().try_as<xaml::FrameworkElement>();

            if (element == nullptr)
            {
                return;
            }

            auto const key = std::wstring{ winrt::unbox_value_or<winrt::hstring>(element.Tag(), L"") };

            if (key == NewTileKey)
            {
                ShowNewPatchMenu(element);
            }
            else if (!key.empty())
            {
                OpenPatch(key);
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to open the patch.")
    }

    _Use_decl_annotations_
    void LibraryWindow::OnPatchGridSizeChanged(foundation::IInspectable const& sender, xaml::SizeChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        if (patchbay::AppSettings::Current().LibraryShowsList())
        {
            ApplyRowWidths();
        }
    }

    // ------------------------------------------------------------------ the bar at the bottom

    void LibraryWindow::UpdateStatusStrip() noexcept
    {
        try
        {
            auto& library = patchbay::PatchLibrary::Current();

            auto const count = library.Patches().size();
            auto const routing = static_cast<int32_t>(library.RoutingCount());

            // The folder is named the way the customer would say it, not as a path.
            FolderStatusText().Text(count == 1
                ? resources::GetString(L"StatusFolderOne")
                : resources::FormatString(L"StatusFolderFormat", count));

            auto const delivered = library.TotalDelivered();
            auto const now = std::chrono::steady_clock::now();

            uint64_t rate{ 0 };
            bool measured{ m_lastRateSample.time_since_epoch().count() == 0 };

            if (!measured)
            {
                auto const elapsed = std::chrono::duration<double>(now - m_lastRateSample).count();

                if (elapsed > 0.05 && delivered >= m_lastDelivered)
                {
                    rate = static_cast<uint64_t>((delivered - m_lastDelivered) / elapsed);
                    measured = true;
                }
            }

            if (measured)
            {
                ActivityStatusText().Text(rate == 1
                    ? resources::FormatString(L"StatusActivityOneFormat", routing)
                    : resources::FormatString(L"StatusActivityFormat", routing, rate));
            }

            m_lastRateSample = now;
            m_lastDelivered = delivered;
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to update the status strip.")
    }

    void LibraryWindow::CheckServiceState() noexcept
    {
        try
        {
            // Cheap, but there is no need to ask twice a second.
            if (m_serviceCheckTicks++ % ServiceCheckEveryTicks != 0)
            {
                return;
            }

            auto const running = midiapp::IsMidiServiceRunning();

            if (running != m_serviceRunning)
            {
                m_serviceRunning = running;
                UpdateServiceChip();
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to check the MIDI service.")
    }

    void LibraryWindow::UpdateServiceChip() noexcept
    {
        try
        {
            ServiceChipText().Text(resources::GetString(m_serviceRunning ? L"StatusServiceRunning" : L"StatusServiceStopped"));
            ServiceChipIcon().Glyph(m_serviceRunning ? L"\uE73E" : L"\uE7BA");

            auto const foreground = Brush(m_serviceRunning ? L"SystemFillColorSuccessBrush" : L"SystemFillColorCriticalBrush");

            ServiceChipShape().Stroke(foreground);
            ServiceChipShape().Fill(Brush(m_serviceRunning
                ? L"SystemFillColorSuccessBackgroundBrush"
                : L"SystemFillColorCriticalBackgroundBrush"));

            ServiceChipIcon().Foreground(foreground);
            ServiceChipText().Foreground(foreground);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to show the MIDI service state.")
    }
}
