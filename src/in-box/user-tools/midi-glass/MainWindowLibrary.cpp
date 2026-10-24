// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The library half of the main window: reading the layouts folder, grouping into favorites and
// recent, the search and the sort, and everything the card menu does to a file.

#include "pch.h"
#include "MainWindow.xaml.h"
#include "App.xaml.h"

#include "AppSettings.h"
#include "AppProvenance.h"
#include "SharingDialogs.h"
#include "StringResources.h"
#include "LayoutStore.h"
#include "ThemeStore.h"
#include "ThumbnailLayout.h"
#include "ThumbnailRenderer.h"
#include "SurfaceThumbnail.h"
#include "EndpointCatalog.h"
#include "MidiServiceStatus.h"

#include <wil/cppwinrt_helpers.h>

#include <shlobj_core.h>
#include <filesystem>

namespace resources = ::midiglass::resources;

namespace winrt::midiglass::implementation
{
    namespace
    {
        constexpr int64_t TicksPerSecond = 10'000'000;
        constexpr int64_t TicksPerMinute = TicksPerSecond * 60;
        constexpr int64_t TicksPerHour = TicksPerMinute * 60;
        constexpr int64_t TicksPerDay = TicksPerHour * 24;

        int64_t NowTicks() noexcept
        {
            FILETIME now{};
            ::GetSystemTimeAsFileTime(&now);

            return static_cast<int64_t>(
                (static_cast<uint64_t>(now.dwHighDateTime) << 32) | now.dwLowDateTime);
        }

        int64_t LastWriteTicks(_In_ std::wstring const& path) noexcept
        {
            try
            {
                WIN32_FILE_ATTRIBUTE_DATA attributes{};

                if (!::GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &attributes))
                {
                    return 0;
                }

                return static_cast<int64_t>(
                    (static_cast<uint64_t>(attributes.ftLastWriteTime.dwHighDateTime) << 32) |
                    attributes.ftLastWriteTime.dwLowDateTime);
            }
            catch (...)
            {
                return 0;
            }
        }

        std::wstring FormatDate(_In_ int64_t ticks, _In_ wchar_t const* picture) noexcept
        {
            try
            {
                FILETIME file{};
                file.dwLowDateTime = static_cast<DWORD>(ticks & 0xFFFFFFFF);
                file.dwHighDateTime = static_cast<DWORD>(static_cast<uint64_t>(ticks) >> 32);

                FILETIME local{};
                SYSTEMTIME system{};

                if (!::FileTimeToLocalFileTime(&file, &local) ||
                    !::FileTimeToSystemTime(&local, &system))
                {
                    return {};
                }

                wchar_t buffer[96]{};

                if (::GetDateFormatEx(
                    LOCALE_NAME_USER_DEFAULT, 0, &system, picture, buffer, ARRAYSIZE(buffer), nullptr) == 0)
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

        // "2 hours ago", "Yesterday", "Sunday", "Sep 18". A customer should never have to read a
        // timestamp to know which layout they had open this morning.
        std::wstring RelativeDate(_In_ int64_t ticks) noexcept
        {
            if (ticks <= 0)
            {
                return std::wstring{ resources::GetString(L"DateNeverOpened") };
            }

            auto const elapsed = NowTicks() - ticks;

            if (elapsed < TicksPerMinute)
            {
                return std::wstring{ resources::GetString(L"DateJustNow") };
            }

            if (elapsed < TicksPerHour)
            {
                auto const minutes = static_cast<int32_t>(elapsed / TicksPerMinute);

                return std::wstring{ resources::FormatString(
                    minutes == 1 ? L"DateMinuteAgo" : L"DateMinutesAgo", minutes) };
            }

            if (elapsed < TicksPerDay)
            {
                auto const hours = static_cast<int32_t>(elapsed / TicksPerHour);

                return std::wstring{ resources::FormatString(
                    hours == 1 ? L"DateHourAgo" : L"DateHoursAgo", hours) };
            }

            if (elapsed < TicksPerDay * 2)
            {
                return std::wstring{ resources::GetString(L"DateYesterday") };
            }

            // Inside the last week the weekday is what people remember it by.
            if (elapsed < TicksPerDay * 7)
            {
                auto const weekday = FormatDate(ticks, L"dddd");

                if (!weekday.empty())
                {
                    return weekday;
                }
            }

            auto const date = FormatDate(ticks, L"MMM d");

            return date.empty() ? std::wstring{} : date;
        }

        bool CardIsStale(
            _In_ std::wstring const& layoutPath,
            _In_ std::wstring const& cardPath) noexcept
        {
            try
            {
                std::error_code error{};

                if (!std::filesystem::exists(cardPath, error) || error)
                {
                    return true;
                }

                auto const layoutTime = std::filesystem::last_write_time(layoutPath, error);

                if (error)
                {
                    return true;
                }

                auto const cardTime = std::filesystem::last_write_time(cardPath, error);

                return error || cardTime < layoutTime;
            }
            catch (...)
            {
                return true;
            }
        }

        // Cards drawn an older way are never looked up again, so they are cleared out once a
        // session rather than left to pile up. It is a cache: nothing in it is ever the only
        // copy of anything.
        void ForgetOldCards() noexcept
        {
            static std::atomic<bool> done{ false };

            if (done.exchange(true))
            {
                return;
            }

            try
            {
                auto const folder = glass::ThumbnailCacheFolder();

                if (folder.empty())
                {
                    return;
                }

                auto const current = std::format(L"-v{}", glass::ThumbnailCacheVersion);

                std::error_code error{};

                for (auto const& entry : std::filesystem::directory_iterator(folder, error))
                {
                    if (error || !entry.is_regular_file(error))
                    {
                        continue;
                    }

                    auto const& path = entry.path();
                    auto const extension = path.extension().wstring();
                    auto const stem = path.stem().wstring();

                    auto const oldCard = extension == L".png" &&
                        (stem.size() < current.size() ||
                         stem.compare(stem.size() - current.size(), current.size(), current) != 0);

                    // Left behind by a write that never finished.
                    auto const partial = extension == L".partial";

                    if (oldCard || partial)
                    {
                        std::error_code ignored{};
                        std::filesystem::remove(path, ignored);
                    }
                }
            }
            catch (...)
            {
            }
        }

        std::wstring FallbackName(_In_ std::wstring const& filePath) noexcept
        {
            try
            {
                return glass::LayoutNameFromFileName(std::filesystem::path{ filePath }.filename().wstring());
            }
            catch (...)
            {
                return filePath;
            }
        }

        media::ImageSource LoadCard(_In_ std::wstring const& cardPath) noexcept
        {
            try
            {
                std::error_code error{};

                if (cardPath.empty() || !std::filesystem::exists(cardPath, error) || error)
                {
                    return nullptr;
                }

                media::Imaging::BitmapImage bitmap{};

                // A card is redrawn under the same file name, so the decoder's cache would
                // otherwise keep showing the picture from before the layout changed.
                bitmap.CreateOptions(media::Imaging::BitmapCreateOptions::IgnoreImageCache);
                bitmap.DecodePixelWidth(glass::LargeThumbnailWidth);
                bitmap.UriSource(foundation::Uri{ L"file:///" + winrt::hstring{ cardPath } });

                return bitmap;
            }
            catch (...)
            {
                return nullptr;
            }
        }

        std::wstring LowerCopy(_In_ std::wstring value) noexcept
        {
            std::transform(value.begin(), value.end(), value.begin(),
                [](wchar_t c) { return static_cast<wchar_t>(::towlower(c)); });

            return value;
        }

        // How many of this layout's devices are here right now. The card says this plainly,
        // because a missing device never blocks opening a layout.
        void DescribeDevices(
            _In_ glass::LayoutDocument const& document,
            _Out_ ::midiglass::LayoutCardStatus& status,
            _Out_ std::wstring& text) noexcept
        {
            auto const& catalog = midiapp::EndpointCatalog::Current();

            int32_t ready{ 0 };
            int32_t missing{ 0 };

            for (auto const& device : document.Devices)
            {
                if (catalog.Resolve(device.Match, device.MatchMode, device.Name).has_value())
                {
                    ready++;
                }
                else
                {
                    missing++;
                }
            }

            if (document.Devices.empty())
            {
                status = ::midiglass::LayoutCardStatus::NeedsAttention;
                text = std::wstring{ resources::GetString(L"CardStatusNoDevices") };
            }
            else if (missing > 0)
            {
                status = ::midiglass::LayoutCardStatus::DeviceMissing;
                text = std::wstring{ resources::FormatString(
                    missing == 1 ? L"CardStatusOneMissing" : L"CardStatusSomeMissing", missing) };
            }
            else
            {
                status = ::midiglass::LayoutCardStatus::Ready;
                text = std::wstring{ resources::FormatString(
                    ready == 1 ? L"CardStatusOneReady" : L"CardStatusAllReady", ready) };
            }
        }

        ::midiglass::LayoutCardData MakeNewTile() noexcept
        {
            ::midiglass::LayoutCardData tile{};
            tile.IsNewTile = true;

            return tile;
        }

        // Kept beside the markup they have to agree with: a card is this wide and the grid puts
        // this much air between two of them.
        constexpr double CardWidth = 232.0;
        constexpr double CardGap = 14.0;
    }

    // ================================================================== reading

    void MainWindow::RefreshLibrary()
    {
        // Dropping it would lose whatever asked, such as the device list arriving.
        if (m_refreshing)
        {
            m_refreshAgain = true;
            return;
        }

        m_refreshing = true;

        auto weak = get_weak();

        // Reading every layout and drawing every missing card is disk work, so it never happens
        // on the UI thread.
        std::thread([weak, queue = m_dispatcher]()
            {
                winrt::init_apartment(winrt::apartment_type::multi_threaded);

                std::vector<::midiglass::LayoutCardData> cards{};
                std::vector<StaleCard> stale{};

                ForgetOldCards();

                try
                {
                    auto const& settings = ::midiglass::AppSettings::Current();

                    for (auto const& path : glass::ListLayoutFiles())
                    {
                        auto const read = glass::ReadLayoutFile(path);

                        ::midiglass::LayoutCardData card{};

                        card.FilePath = path;
                        card.LastUsedTicks = settings.LayoutLastUsed(path);
                        card.LastChangedTicks = LastWriteTicks(path);

                        // Never opened on this PC still deserves a date, so the card falls back
                        // to when the file itself last changed.
                        card.RelativeDate = RelativeDate(
                            card.LastUsedTicks != 0 ? card.LastUsedTicks : card.LastChangedTicks);

                        if (!read.Succeeded)
                        {
                            card.DisplayName = FallbackName(path);
                            card.Status = ::midiglass::LayoutCardStatus::NeedsAttention;
                            card.StatusText = std::wstring{ resources::GetString(L"CardStatusUnreadable") };

                            cards.push_back(std::move(card));
                            continue;
                        }

                        auto const& document = read.Document;

                        card.DisplayName = document.Name.empty() ? FallbackName(path) : document.Name;
                        card.Description = document.Description;
                        card.FileSaysFavorite = document.IsFavorite;

                        card.DetailText = std::wstring{ resources::FormatString(
                            L"LibraryDetailFormat",
                            static_cast<int32_t>(document.ControlCount()),
                            document.PageWidth,
                            document.PageHeight,
                            document.ThemeName.empty()
                                ? std::wstring{ L"Studio Dark" }
                                : document.ThemeName) };

                        DescribeDevices(document, card.Status, card.StatusText);

                        // Said on the card, so it is read out with everything else on it.
                        card.SignerName = ::midiglass::sharing::SignerOf(path);

                        if (!card.SignerName.empty())
                        {
                            card.DetailText = std::wstring{ resources::FormatString(
                                L"LibraryDetailSignedFormat", card.DetailText, card.SignerName) };
                        }

                        // It opens and runs, but nothing here writes it, so that comes first.
                        if (document.IsFromNewerVersion)
                        {
                            card.IsFromNewerVersion = true;
                            card.Status = ::midiglass::LayoutCardStatus::NeedsAttention;
                            card.StatusText = std::wstring{ resources::GetString(L"CardStatusNewerVersion") };
                        }

                        auto const cardPath = glass::ThumbnailPathForLayout(path, glass::LargeThumbnailWidth);

                        // Drawn afterwards, on the window's own thread, by the same surface the
                        // layout opens in. Only the reading happens here.
                        if (!cardPath.empty() && CardIsStale(path, cardPath))
                        {
                            stale.push_back(StaleCard{ path, cardPath, document });
                        }

                        cards.push_back(std::move(card));
                    }
                }
                catch (...)
                {
                }

                // The window is resolved on the UI thread only, so it is never released here.
                try
                {
                    if (queue != nullptr)
                    {
                        queue.TryEnqueue([weak, cards = std::move(cards), stale = std::move(stale)]()
                            {
                                if (auto inner = weak.get())
                                {
                                    inner->ApplyCards(cards);
                                    inner->m_refreshing = false;
                                    inner->QueueStaleCards(stale);

                                    if (std::exchange(inner->m_refreshAgain, false))
                                    {
                                        inner->RefreshLibrary();
                                    }
                                }
                            });
                    }
                }
                catch (...)
                {
                }

                winrt::uninit_apartment();
            }).detach();
    }

    _Use_decl_annotations_
    void MainWindow::QueueStaleCards(std::vector<StaleCard> stale)
    {
        // A layout already waiting is not queued twice. A refresh can land while the last one's
        // cards are still being drawn, and it finds the same stale cards they are about to fix.
        for (auto& card : stale)
        {
            auto const queued = std::any_of(m_cardQueue.begin(), m_cardQueue.end(),
                [&card](StaleCard const& waiting) { return waiting.CardPath == card.CardPath; });

            if (!queued)
            {
                m_cardQueue.push_back(std::move(card));
            }
        }

        if (!m_drawingCards && !m_cardQueue.empty())
        {
            DrawQueuedCardsAsync();
        }
    }

    winrt::fire_and_forget MainWindow::DrawQueuedCardsAsync()
    {
        auto strong = get_strong();

        m_drawingCards = true;

        auto drewAny = false;

        try
        {
            while (!m_cardQueue.empty())
            {
                auto next = std::move(m_cardQueue.front());
                m_cardQueue.erase(m_cardQueue.begin());

                auto const theme = glass::ResolveDocumentTheme(next.Document);

                // What the layout really looks like, drawn by the surface it opens in.
                auto const captured = co_await glass::RenderSurfaceThumbnailAsync(
                    ThumbnailStage(),
                    next.Document,
                    theme,
                    glass::LargeThumbnailWidth,
                    glass::LargeThumbnailHeight,
                    next.CardPath);

                // A capture that fails or comes back empty still gets a card. The plain drawing
                // needs no window at all, so the card is never left blank.
                if (!captured)
                {
                    co_await winrt::resume_background();

                    glass::RenderThumbnailToFile(
                        glass::PlanThumbnail(
                            next.Document,
                            theme,
                            glass::LargeThumbnailWidth,
                            glass::LargeThumbnailHeight),
                        next.CardPath);

                    co_await wil::resume_foreground(m_dispatcher);
                }

                drewAny = true;
            }
        }
        catch (...)
        {
            // The window went away mid-card. Whatever was not drawn is stale, and will be found
            // again the next time the library is read.
            m_cardQueue.clear();
        }

        m_drawingCards = false;

        if (drewAny)
        {
            try
            {
                RebuildSections();
            }
            catch (...)
            {
            }
        }
    }

    _Use_decl_annotations_
    void MainWindow::ApplyCards(std::vector<::midiglass::LayoutCardData> const& cards)
    {
        auto& settings = ::midiglass::AppSettings::Current();

        // Older builds kept the star in the layout file. It moves into this PC's settings once.
        if (!settings.FavoritesMoved())
        {
            std::vector<std::wstring> starred{};

            for (auto const& card : cards)
            {
                if (card.FileSaysFavorite)
                {
                    starred.push_back(card.FilePath);
                }
            }

            settings.MoveFavorites(starred);
        }

        auto resolved = cards;

        for (auto& card : resolved)
        {
            card.IsFavorite = settings.IsFavoriteLayout(card.FilePath);
        }

        std::wstring signature{};

        for (auto const& card : resolved)
        {
            signature += card.FilePath;
            signature += L'\x1';
            signature += card.DisplayName;
            signature += L'\x1';
            signature += card.Description;
            signature += L'\x1';
            signature += card.StatusText;
            signature += L'\x1';
            signature += card.RelativeDate;
            signature += L'\x1';
            signature += card.IsFavorite ? L'1' : L'0';
            signature += L'\n';
        }

        if (signature == m_cardSignature)
        {
            return;
        }

        m_cardSignature = std::move(signature);
        m_allCards = std::move(resolved);

        RebuildSections();
        UpdateStatusBar();
    }

    // ============================================================ search and sort

    void MainWindow::RebuildSections()
    {
        try
        {
            auto const& settings = ::midiglass::AppSettings::Current();
            auto const sort = settings.LibrarySortOrder();

            auto matching = m_allCards;

            if (!m_searchText.empty())
            {
                auto const needle = LowerCopy(m_searchText);

                matching.erase(
                    std::remove_if(matching.begin(), matching.end(),
                        [&needle](::midiglass::LayoutCardData const& card)
                        {
                            return LowerCopy(card.DisplayName).find(needle) == std::wstring::npos &&
                                LowerCopy(card.Description).find(needle) == std::wstring::npos;
                        }),
                    matching.end());
            }

            std::stable_sort(matching.begin(), matching.end(),
                [sort](::midiglass::LayoutCardData const& left, ::midiglass::LayoutCardData const& right)
                {
                    switch (sort)
                    {
                    case ::midiglass::LibrarySort::Name:
                        return ::CompareStringOrdinal(
                            left.DisplayName.c_str(), -1, right.DisplayName.c_str(), -1, TRUE) == CSTR_LESS_THAN;

                    case ::midiglass::LibrarySort::LastChanged:
                        return left.LastChangedTicks > right.LastChangedTicks;

                    case ::midiglass::LibrarySort::LastUsed:
                    default:
                    {
                        auto const leftKey = left.LastUsedTicks != 0 ? left.LastUsedTicks : left.LastChangedTicks;
                        auto const rightKey = right.LastUsedTicks != 0 ? right.LastUsedTicks : right.LastChangedTicks;

                        return leftKey > rightKey;
                    }
                    }
                });

            m_favorites.Clear();
            m_recent.Clear();

            int32_t favoriteCount{ 0 };
            int32_t recentCount{ 0 };

            for (auto const& data : matching)
            {
                auto card = winrt::make_self<LayoutCard>();

                card->Update(data);
                card->Thumbnail(LoadCard(
                    glass::ThumbnailPathForLayout(data.FilePath, glass::LargeThumbnailWidth)));

                if (data.IsFavorite)
                {
                    m_favorites.Append(*card);
                    favoriteCount++;
                }
                else
                {
                    m_recent.Append(*card);
                    recentCount++;
                }
            }

            // The tile that starts a new layout sits at the end of the grid, where the eye
            // arrives after the last card rather than before the first. A list has no room for a
            // dashed card, and the toolbar button is right there, so it is cards only.
            if (!settings.LibraryShowsList())
            {
                auto newTile = winrt::make_self<LayoutCard>();
                newTile->Update(MakeNewTile());

                if (recentCount > 0 || favoriteCount == 0)
                {
                    m_recent.Append(*newTile);
                }
                else
                {
                    m_favorites.Append(*newTile);
                }
            }

            FavoritesCount().Text(winrt::to_hstring(favoriteCount));
            RecentCount().Text(winrt::to_hstring(recentCount));

            FavoritesSection().Visibility(
                favoriteCount > 0 ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);

            auto const nothingAtAll = m_allCards.empty();

            RecentSection().Visibility(nothingAtAll ? xaml::Visibility::Collapsed : xaml::Visibility::Visible);
            EmptyLibraryPanel().Visibility(nothingAtAll ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);

            RecentHeading().Text(m_searchText.empty()
                ? resources::GetString(L"RecentHeadingText")
                : resources::GetString(L"SearchResultsHeading"));
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to show the layout library.")
    }

    void MainWindow::ApplyViewMode()
    {
        try
        {
            auto const showsList = ::midiglass::AppSettings::Current().LibraryShowsList();

            auto const selector = RootGrid().Resources()
                .Lookup(box_value(L"CardTemplateSelector"))
                .as<midiglass::LayoutCardTemplateSelector>();

            auto const templateKey = showsList ? L"ListRowTemplate" : L"GridCardTemplate";

            selector.CardTemplate(
                RootGrid().Resources().Lookup(box_value(templateKey)).as<xaml::DataTemplate>());

            m_updatingChrome = true;
            GridViewToggle().IsChecked(!showsList);
            ListViewToggle().IsChecked(showsList);
            m_updatingChrome = false;

            // One item per row is a list. Keeping the same control for both means the selection,
            // the keyboard order and the context menu behave identically in either view.
            for (auto const& grid : { FavoritesGrid(), RecentGrid() })
            {
                grid.ItemTemplateSelector(nullptr);
                grid.ItemTemplateSelector(selector);
            }

            ApplyItemWidths();

            RebuildSections();
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to change the library view.")
    }

    void MainWindow::ApplyItemWidths()
    {
        try
        {
            auto const showsList = ::midiglass::AppSettings::Current().LibraryShowsList();

            for (auto const& grid : { FavoritesGrid(), RecentGrid() })
            {
                auto const panel = grid.ItemsPanelRoot().try_as<controls::ItemsWrapGrid>();

                if (panel == nullptr)
                {
                    continue;
                }

                panel.MaximumRowsOrColumns(showsList ? 1 : -1);

                // A list row spans the window. The item's own margin is inside that width, so it
                // is taken off rather than left to push the last column off the edge.
                panel.ItemWidth(showsList
                    ? std::max(320.0, grid.ActualWidth() - CardGap)
                    : CardWidth + CardGap);
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to size the library items.")
    }

    _Use_decl_annotations_
    void MainWindow::OnLibrarySizeChanged(
        foundation::IInspectable const& sender,
        xaml::SizeChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        // Also raised by a grid the first time it is shown, which is the first moment its panel
        // exists to be sized. Favorites start hidden, so that is how their first card gets its width.
        ApplyItemWidths();
    }

    _Use_decl_annotations_
    void MainWindow::OnLayoutContainerContentChanging(
        controls::ListViewBase const& sender,
        controls::ContainerContentChangingEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);

        try
        {
            if (args.InRecycleQueue())
            {
                return;
            }

            // Rows sit half as far apart as cards.
            auto const gap = ::midiglass::AppSettings::Current().LibraryShowsList() ? CardGap / 2.0 : CardGap;

            if (auto const container = args.ItemContainer())
            {
                container.Margin(xaml::ThicknessHelper::FromLengths(0.0, 0.0, CardGap, gap));
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to space the library items.")
    }

    void MainWindow::UpdateStatusBar()
    {
        try
        {
            auto const count = static_cast<int32_t>(m_allCards.size());

            // The folder is named the way the customer would say it, not as a path.
            FolderStatusText().Text(resources::FormatString(
                count == 1 ? L"StatusOneLayoutFormat" : L"StatusLayoutCountFormat", count));

            ServiceChipText().Text(resources::GetString(
                m_serviceRunning ? L"StatusServiceRunning" : L"StatusServiceStopped"));

            ServiceChipIcon().Glyph(m_serviceRunning ? L"\uE73E" : L"\uE7BA");

            auto const& appResources = xaml::Application::Current().Resources();

            auto const foreground = appResources.Lookup(box_value(m_serviceRunning
                ? L"SystemFillColorSuccessBrush"
                : L"SystemFillColorCriticalBrush")).as<media::Brush>();

            ServiceChipShape().Stroke(foreground);
            ServiceChipShape().Fill(appResources.Lookup(box_value(m_serviceRunning
                ? L"SystemFillColorSuccessBackgroundBrush"
                : L"SystemFillColorCriticalBackgroundBrush")).as<media::Brush>());

            ServiceChipIcon().Foreground(foreground);
            ServiceChipText().Foreground(foreground);

            // On a PC with a lot of devices the first look takes a while, and until it is done
            // an empty device list does not mean there is nothing there.
            auto const looking = m_serviceRunning && !midiapp::EndpointCatalog::Current().HasEnumerated();

            DeviceSearchPanel().Visibility(looking ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
            DeviceSearchRing().IsActive(looking);
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to show the status bar.")
    }

    bool MainWindow::CheckServiceState()
    {
        try
        {
            auto const running = midiapp::IsMidiServiceRunning();

            if (running == m_serviceRunning)
            {
                return false;
            }

            m_serviceRunning = running;

            UpdateStatusBar();

            // Every card's device status is wrong the moment the service state changes, so the
            // signature is dropped and they are re-resolved rather than left claiming a synth
            // is still reachable.
            m_cardSignature.clear();

            return true;
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to check the MIDI service state.")

        return false;
    }

    // =============================================================== the toolbar

    _Use_decl_annotations_
    void MainWindow::OnSearchTextChanged(
        controls::AutoSuggestBox const& sender,
        controls::AutoSuggestBoxTextChangedEventArgs const& args)
    {
        if (args.Reason() != controls::AutoSuggestionBoxTextChangeReason::UserInput)
        {
            return;
        }

        m_searchText = std::wstring{ sender.Text() };

        RebuildSections();
    }

    _Use_decl_annotations_
    void MainWindow::OnSortSelectionChanged(
        foundation::IInspectable const& sender,
        controls::SelectionChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        if (m_updatingChrome)
        {
            return;
        }

        auto const index = SortSelector().SelectedIndex();

        if (index < 0)
        {
            return;
        }

        ::midiglass::AppSettings::Current().LibrarySortOrder(
            static_cast<::midiglass::LibrarySort>(index));

        RebuildSections();
    }

    _Use_decl_annotations_
    void MainWindow::OnGridViewToggled(
        foundation::IInspectable const& sender,
        xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        if (m_updatingChrome)
        {
            return;
        }

        ::midiglass::AppSettings::Current().LibraryShowsList(false);

        ApplyViewMode();
    }

    _Use_decl_annotations_
    void MainWindow::OnListViewToggled(
        foundation::IInspectable const& sender,
        xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        if (m_updatingChrome)
        {
            return;
        }

        ::midiglass::AppSettings::Current().LibraryShowsList(true);

        ApplyViewMode();
    }

    _Use_decl_annotations_
    void MainWindow::OnOpenFileClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        try
        {
            // The Win32 common item dialog, never Windows.Storage.Pickers, which is what the rest
            // of this tool family uses in an unpackaged app.
            auto dialog = wil::CoCreateInstance<IFileOpenDialog>(CLSID_FileOpenDialog);

            // The old extension too, for a layout somebody was sent before it changed.
            auto const filterName = resources::GetString(L"OpenFileFilterLayouts");

            COMDLG_FILTERSPEC const filters[]
            {
                { filterName.c_str(), L"*.midilayout;*.midilayout.json" },
            };

            dialog->SetFileTypes(ARRAYSIZE(filters), filters);
            dialog->SetTitle(resources::GetString(L"OpenFileTitle").c_str());

            if (FAILED(dialog->Show(m_chrome.WindowHandle())))
            {
                return;
            }

            winrt::com_ptr<IShellItem> item{};

            if (FAILED(dialog->GetResult(item.put())))
            {
                return;
            }

            wil::unique_cotaskmem_string path{};

            if (FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) || path.get() == nullptr)
            {
                return;
            }

            App::OpenRuntimeWindow(std::wstring{ path.get() });
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to open a layout file.")
    }

    // ================================================================= the cards

    _Use_decl_annotations_
    void MainWindow::RunCard(midiglass::LayoutCard const& card)
    {
        if (card == nullptr || card.IsNewTile())
        {
            return;
        }

        auto const path = std::wstring{ card.FilePath() };

        ::midiglass::AppSettings::Current().RecordLayoutUse(path);

        App::OpenRuntimeWindow(path);

        // The date on the card is now wrong, and so is the order if the library is sorted by
        // last used.
        m_cardSignature.clear();

        RefreshLibrary();
    }

    _Use_decl_annotations_
    void MainWindow::OnLayoutItemClick(
        foundation::IInspectable const& sender,
        controls::ItemClickEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);

        try
        {
            auto const card = args.ClickedItem().try_as<midiglass::LayoutCard>();

            if (card == nullptr)
            {
                return;
            }

            if (card.IsNewTile())
            {
                ShowNewLayoutDialogAsync();
                return;
            }

            RunCard(card);
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to run the layout.")
    }

    _Use_decl_annotations_
    void MainWindow::OnRunLayoutClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(args);

        try
        {
            if (auto const element = sender.try_as<xaml::FrameworkElement>())
            {
                RunCard(element.DataContext().try_as<midiglass::LayoutCard>());
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to run the layout.")
    }

    _Use_decl_annotations_
    void MainWindow::EditCard(midiglass::LayoutCard const& card)
    {
        if (card == nullptr || card.IsNewTile())
        {
            return;
        }

        auto const path = std::wstring{ card.FilePath() };

        if (auto const self = winrt::get_self<LayoutCard>(card); !self->SignerName().empty())
        {
            EditSignedCardAsync(card);
            return;
        }

        ::midiglass::AppSettings::Current().RecordLayoutUse(path);

        App::OpenEditorWindow(path);

        m_cardSignature.clear();

        RefreshLibrary();
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::EditSignedCardAsync(midiglass::LayoutCard card)
    {
        auto lifetime = get_strong();

        try
        {
            auto const signer = winrt::get_self<LayoutCard>(card)->SignerName();

            controls::TextBlock text{};
            text.Text(resources::FormatString(L"EditSignedBodyFormat", std::wstring{ card.DisplayName() }, signer));
            text.TextWrapping(xaml::TextWrapping::Wrap);
            text.MaxWidth(420.0);

            controls::ContentDialog dialog{};
            dialog.XamlRoot(RootGrid().XamlRoot());
            dialog.Title(box_value(resources::GetString(L"EditSignedTitle")));
            dialog.Content(text);
            dialog.PrimaryButtonText(resources::GetString(L"EditSignedCopyAction"));
            dialog.CloseButtonText(resources::GetString(L"DialogCancel"));
            dialog.DefaultButton(controls::ContentDialogButton::Primary);

            if (co_await dialog.ShowAsync() != controls::ContentDialogResult::Primary)
            {
                co_return;
            }

            auto const copy = DuplicateLayout(std::wstring{ card.FilePath() });

            if (copy.empty())
            {
                co_return;
            }

            ::midiglass::AppSettings::Current().RecordLayoutUse(copy);

            App::OpenEditorWindow(copy);

            m_cardSignature.clear();
            RefreshLibrary();
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to copy the signed layout.")
    }

    _Use_decl_annotations_
    void MainWindow::OnEditLayoutClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(args);

        try
        {
            if (auto const element = sender.try_as<xaml::FrameworkElement>())
            {
                EditCard(element.DataContext().try_as<midiglass::LayoutCard>());
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to open the layout for editing.")
    }

    namespace
    {
        void SetHoverBar(_In_ foundation::IInspectable const& sender, _In_ bool visible) noexcept
        {
            try
            {
                auto const root = sender.try_as<xaml::FrameworkElement>();

                if (root == nullptr)
                {
                    return;
                }

                if (auto const bar = root.FindName(L"HoverBar").try_as<xaml::UIElement>())
                {
                    bar.Opacity(visible ? 1.0 : 0.0);
                }
            }
            catch (...)
            {
            }
        }

        // Focus lands on the item container, which sits above the template root, so a GotFocus
        // handler inside the template never sees it. Walking up to the container and back down
        // through its template root is what makes the keyboard reach the same bar the pointer does.
        void SetHoverBarForFocus(_In_ foundation::IInspectable const& source, _In_ bool visible) noexcept
        {
            try
            {
                auto current = source.try_as<xaml::DependencyObject>();

                for (int32_t depth = 0; current != nullptr && depth < 12; ++depth)
                {
                    if (auto const item = current.try_as<controls::GridViewItem>())
                    {
                        SetHoverBar(item.ContentTemplateRoot(), visible);
                        return;
                    }

                    current = media::VisualTreeHelper::GetParent(current);
                }
            }
            catch (...)
            {
            }
        }
    }

    _Use_decl_annotations_
    void MainWindow::OnCardPointerEntered(
        foundation::IInspectable const& sender,
        xaml::Input::PointerRoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(args);

        SetHoverBar(sender, true);
    }

    _Use_decl_annotations_
    void MainWindow::OnCardPointerExited(
        foundation::IInspectable const& sender,
        xaml::Input::PointerRoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(args);

        SetHoverBar(sender, false);
    }

    _Use_decl_annotations_
    void MainWindow::OnCardGotFocus(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(args);

        // Keyboard only, so Run and the card menu are reachable without a pointer.
        SetHoverBar(sender, true);
    }

    _Use_decl_annotations_
    void MainWindow::OnCardLostFocus(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(args);

        SetHoverBar(sender, false);
    }

    _Use_decl_annotations_
    void MainWindow::OnGridGotFocus(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);

        SetHoverBarForFocus(args.OriginalSource(), true);
    }

    _Use_decl_annotations_
    void MainWindow::OnGridLostFocus(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);

        SetHoverBarForFocus(args.OriginalSource(), false);
    }

    // ============================================================= the card menu

    _Use_decl_annotations_
    void MainWindow::OnCardMoreClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(args);

        try
        {
            auto const button = sender.try_as<controls::Button>();

            if (button == nullptr)
            {
                return;
            }

            m_menuCard = button.DataContext().try_as<midiglass::LayoutCard>();

            auto const flyout = RootGrid().Resources()
                .Lookup(box_value(L"CardMenuFlyout"))
                .as<controls::MenuFlyout>();

            flyout.ShowAt(button);
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to show the card menu.")
    }

    _Use_decl_annotations_
    void MainWindow::OnCardMenuOpening(
        foundation::IInspectable const& sender,
        foundation::IInspectable const& args)
    {
        UNREFERENCED_PARAMETER(args);

        try
        {
            auto const flyout = sender.try_as<controls::MenuFlyout>();

            if (flyout == nullptr)
            {
                return;
            }

            // A right tap opens the same menu without going through the More button, so the
            // target is where the card comes from in that case.
            if (auto const target = flyout.Target().try_as<xaml::FrameworkElement>())
            {
                if (auto const card = target.DataContext().try_as<midiglass::LayoutCard>())
                {
                    m_menuCard = card;
                }
            }

            if (m_menuCard == nullptr)
            {
                return;
            }

            // Found by tag rather than by position, so reordering the menu cannot silently
            // relabel the wrong item.
            auto const newer = winrt::get_self<LayoutCard>(m_menuCard)->IsFromNewerVersion();
            auto const isSigned = !winrt::get_self<LayoutCard>(m_menuCard)->SignerName().empty();

            for (auto const& item : flyout.Items())
            {
                auto const entry = item.try_as<controls::MenuFlyoutItem>();

                if (entry == nullptr)
                {
                    continue;
                }

                auto const tag = winrt::unbox_value_or<winrt::hstring>(entry.Tag(), L"");

                if (tag == L"favorite")
                {
                    entry.Text(m_menuCard.FavoriteMenuText());
                }

                // Each of these writes the layout, and a newer version's layout is never written
                // from here. The star is kept in settings, so it is always allowed.
                if (tag == L"duplicate" || tag == L"rename" || tag == L"describe")
                {
                    entry.IsEnabled(!newer);
                }

                // A signed layout stays exactly as it was signed. Its copy can be changed.
                if (isSigned && (tag == L"rename" || tag == L"describe"))
                {
                    entry.IsEnabled(false);
                }
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to prepare the card menu.")
    }

    _Use_decl_annotations_
    bool MainWindow::EditLayoutFile(
        std::wstring const& filePath,
        std::function<void(glass::LayoutDocument&)> const& change)
    {
        try
        {
            auto read = glass::ReadLayoutFile(filePath);

            if (!read.Succeeded)
            {
                return false;
            }

            change(read.Document);

            return glass::WriteLayoutFile(read.Document, filePath);
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to change the layout.")

        return false;
    }

    _Use_decl_annotations_
    void MainWindow::OnCardMenuRun(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        RunCard(m_menuCard);
    }

    _Use_decl_annotations_
    void MainWindow::OnCardMenuEdit(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        EditCard(m_menuCard);
    }

    _Use_decl_annotations_
    void MainWindow::OnCardMenuFavorite(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        if (m_menuCard == nullptr)
        {
            return;
        }

        auto const wanted = !m_menuCard.IsFavorite();
        auto const path = std::wstring{ m_menuCard.FilePath() };

        ::midiglass::AppSettings::Current().FavoriteLayout(path, wanted);

        for (auto& card : m_allCards)
        {
            if (card.FilePath == path)
            {
                card.IsFavorite = wanted;
            }
        }

        // The next read of the folder has to rebuild, because what it compares against is now stale.
        m_cardSignature.clear();
        RebuildSections();
    }

    _Use_decl_annotations_
    void MainWindow::OnCardMenuDuplicate(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        if (m_menuCard == nullptr)
        {
            return;
        }

        if (!DuplicateLayout(std::wstring{ m_menuCard.FilePath() }).empty())
        {
            RefreshLibrary();
        }
    }

    _Use_decl_annotations_
    std::wstring MainWindow::DuplicateLayout(std::wstring const& filePath)
    {
        try
        {
            auto read = glass::ReadLayoutFile(filePath);

            if (!read.Succeeded)
            {
                return {};
            }

            auto const folder = glass::LayoutsFolder();

            if (folder.empty())
            {
                return {};
            }

            auto const originalName = read.Document.Name;

            read.Document.Name = std::wstring{ resources::FormatString(
                L"DuplicateNameFormat", read.Document.Name) };

            // A copy is a new layout made from this one, so it gets its own identity and credits
            // the original.
            read.Document.Provenance = ::midiglass::CopyProvenance(originalName, read.Document.Provenance, false);

            auto const path = glass::MakeUnusedLayoutPath(folder, read.Document.Name);

            read.Document.FilePath = path;

            if (glass::WriteLayoutFile(read.Document, path))
            {
                return path;
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to duplicate the layout.")

        return {};
    }

    _Use_decl_annotations_
    void MainWindow::OnCardMenuShowInFolder(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        if (m_menuCard == nullptr)
        {
            return;
        }

        try
        {
            auto const path = std::wstring{ m_menuCard.FilePath() };

            // Opens the folder with the layout already selected, rather than just the folder.
            auto const list = ::ILCreateFromPathW(path.c_str());

            if (list != nullptr)
            {
                ::SHOpenFolderAndSelectItems(list, 0, nullptr, 0);
                ::ILFree(list);
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to show the layout in its folder.")
    }

    _Use_decl_annotations_
    void MainWindow::OnCardMenuRename(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        RenameCardAsync(m_menuCard);
    }

    _Use_decl_annotations_
    void MainWindow::OnCardMenuDescribe(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        DescribeCardAsync(m_menuCard);
    }

    _Use_decl_annotations_
    void MainWindow::OnCardMenuDelete(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        DeleteCardAsync(m_menuCard);
    }
}
