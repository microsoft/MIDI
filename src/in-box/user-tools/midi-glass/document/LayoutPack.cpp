// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Deliberately free of pch.h and XAML, like the rest of the document layer.

#include "LayoutPack.h"
#include "LayoutPackage.h"
#include "LayoutSerializer.h"
#include "LayoutStore.h"
#include "ThemeStore.h"

#include <windows.h>

#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <set>

namespace glass
{
    namespace
    {
        using midiapp::ContentPackInput;
        using midiapp::ContentPackKind;
        using midiapp::OpenedContentPack;
        using midiapp::StoredZipEntry;

        std::vector<uint8_t> ReadAllBytes(_In_ std::filesystem::path const& path) noexcept
        {
            try
            {
                std::ifstream file{ path, std::ios::binary };

                if (!file)
                {
                    return {};
                }

                return std::vector<uint8_t>{
                    std::istreambuf_iterator<char>{ file }, std::istreambuf_iterator<char>{} };
            }
            catch (...)
            {
                return {};
            }
        }

        // Beside the target and moved into place, so a failure leaves what was there.
        bool WriteAllBytes(
            _In_ std::filesystem::path const& path,
            _In_ std::vector<uint8_t> const& bytes) noexcept
        {
            try
            {
                auto temporary = path;
                temporary += L".writing";

                {
                    std::ofstream file{ temporary, std::ios::binary | std::ios::trunc };

                    if (!file)
                    {
                        return false;
                    }

                    if (!bytes.empty())
                    {
                        file.write(
                            reinterpret_cast<char const*>(bytes.data()),
                            static_cast<std::streamsize>(bytes.size()));
                    }

                    if (!file.good())
                    {
                        file.close();
                        std::error_code ignored{};
                        std::filesystem::remove(temporary, ignored);
                        return false;
                    }
                }

                std::error_code ec{};
                std::filesystem::rename(temporary, path, ec);

                if (ec)
                {
                    std::filesystem::remove(temporary, ec);
                    return false;
                }

                return true;
            }
            catch (...)
            {
                return false;
            }
        }

        bool HoldsBytes(
            _In_ std::filesystem::path const& path,
            _In_ std::vector<uint8_t> const& bytes) noexcept
        {
            try
            {
                std::error_code ec{};

                if (std::filesystem::file_size(path, ec) != bytes.size() || ec)
                {
                    return false;
                }

                return ReadAllBytes(path) == bytes;
            }
            catch (...)
            {
                return false;
            }
        }

        std::wstring TextOf(_In_ std::vector<uint8_t> const& bytes) noexcept
        {
            try
            {
                size_t start{ 0 };

                if (bytes.size() >= 3 && bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF)
                {
                    start = 3;
                }

                if (bytes.size() <= start)
                {
                    return {};
                }

                auto const data = reinterpret_cast<char const*>(bytes.data() + start);
                auto const length = static_cast<int>(bytes.size() - start);

                auto const needed = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, data, length, nullptr, 0);

                if (needed <= 0)
                {
                    return {};
                }

                std::wstring text(static_cast<size_t>(needed), L'\0');
                ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, data, length, text.data(), needed);

                return text;
            }
            catch (...)
            {
                return {};
            }
        }

        std::wstring Lower(_In_ std::wstring value)
        {
            std::transform(value.begin(), value.end(), value.begin(),
                [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });

            return value;
        }

        std::wstring InThemeFolder(_In_ std::wstring const& name)
        {
            return std::wstring{ PackThemeFolder } + L"/" + name;
        }

        bool IsInThemeFolder(_In_ std::wstring const& path, _Out_ std::wstring& name)
        {
            std::wstring const prefix = std::wstring{ PackThemeFolder } + L"/";

            if (path.size() > prefix.size() && path.compare(0, prefix.size(), prefix) == 0)
            {
                name = path.substr(prefix.size());
                return true;
            }

            name.clear();
            return false;
        }

        bool IsThemeFileName(_In_ std::wstring const& name) noexcept
        {
            return HasFileExtension(name, ThemeFileExtension) || HasFileExtension(name, LegacyThemeFileExtension);
        }

        std::wstring ThemeStemOf(_In_ std::wstring const& fileName)
        {
            for (std::wstring_view const extension : { LegacyThemeFileExtension, ThemeFileExtension })
            {
                if (HasFileExtension(fileName, extension))
                {
                    return fileName.substr(0, fileName.size() - extension.size());
                }
            }

            return fileName;
        }

        // A theme name typed by a person becomes a file name, so anything unsafe in one goes.
        std::wstring ThemeFileNameFor(_In_ std::wstring const& name)
        {
            std::wstring safe{};

            for (auto const ch : name)
            {
                safe += (ch == L'\\' || ch == L'/' || ch == L':' || ch == L'*' || ch == L'?' ||
                    ch == L'"' || ch == L'<' || ch == L'>' || ch == L'|' || ch < L' ')
                    ? L'_'
                    : ch;
            }

            while (!safe.empty() && (safe.back() == L' ' || safe.back() == L'.'))
            {
                safe.pop_back();
            }

            return (safe.empty() ? std::wstring{ L"Theme" } : safe) + ThemeFileExtension;
        }

        std::filesystem::path UnusedPath(
            _In_ std::filesystem::path const& folder,
            _In_ std::wstring const& fileName)
        {
            auto target = folder / fileName;

            std::error_code ignored{};

            auto const stem = std::filesystem::path{ fileName }.stem().wstring();
            auto const extension = std::filesystem::path{ fileName }.extension().wstring();

            for (int32_t attempt = 2; std::filesystem::exists(target, ignored) && attempt < 1000; ++attempt)
            {
                target = folder / (stem + L" " + std::to_wstring(attempt) + extension);
            }

            return target;
        }

        // The same bytes already there are used as they are. A different file with the name is
        // left alone and the picture takes another name, unless it is one this install replaces.
        // The name it ended up with, or empty when it couldn't be written.
        std::wstring PlaceFile(
            _In_ std::filesystem::path const& folder,
            _In_ std::wstring const& name,
            _In_ std::vector<uint8_t> const& bytes,
            _In_ std::set<std::wstring> const& mayReplace,
            _Inout_ std::vector<std::wstring>& files)
        {
            auto target = folder / name;

            std::error_code ignored{};

            if (std::filesystem::exists(target, ignored))
            {
                if (HoldsBytes(target, bytes))
                {
                    files.push_back(target.wstring());
                    return name;
                }

                if (mayReplace.count(Lower(name)) == 0)
                {
                    target = UnusedPath(folder, name);
                }
            }

            if (!WriteAllBytes(target, bytes))
            {
                return {};
            }

            files.push_back(target.wstring());

            return target.filename().wstring();
        }

        std::vector<std::wstring> PicturesOf(_In_ LayoutDocument const& document)
        {
            std::set<std::wstring> names{};

            if (!document.BackgroundImage.empty())
            {
                names.insert(document.BackgroundImage);
            }

            for (auto const& page : document.Pages)
            {
                for (auto const& control : page.Controls)
                {
                    if (!control.Image.FileName.empty())
                    {
                        names.insert(control.Image.FileName);
                    }
                }
            }

            return { names.begin(), names.end() };
        }

        std::wstring CustomThemeFileNamed(_In_ std::wstring const& name)
        {
            if (name.empty() || FindBuiltInTheme(name) != nullptr)
            {
                return {};
            }

            for (auto const& file : ListThemeFiles())
            {
                auto const read = ReadThemeFile(file);

                if (read.Succeeded && read.Value.Name == name)
                {
                    return file;
                }
            }

            return {};
        }

        // The deck picture a theme names, into the pack's theme folder.
        void AddDeckPicture(
            _In_ Theme const& theme,
            _In_ bool inThemeFolder,
            _Inout_ std::vector<ContentPackInput>& inputs)
        {
            auto const picture = DeckImagePath(theme.Deck);

            if (picture.empty() || !midiapp::IsAllowedContentPackPath(theme.Deck.ImageFileName))
            {
                return;
            }

            auto const path = inThemeFolder ? InThemeFolder(theme.Deck.ImageFileName) : theme.Deck.ImageFileName;

            for (auto const& input : inputs)
            {
                if (::CompareStringOrdinal(input.Path.c_str(), -1, path.c_str(), -1, TRUE) == CSTR_EQUAL)
                {
                    return;
                }
            }

            ContentPackInput input{};
            input.Path = path;
            input.Bytes = ReadAllBytes(picture);

            if (!input.Bytes.empty())
            {
                inputs.push_back(std::move(input));
            }
        }

        StoredZipEntry const* PrimaryOf(_In_ OpenedContentPack const& pack) noexcept
        {
            return pack.FindFile(pack.Manifest.Primary);
        }
    }

    _Use_decl_annotations_
    PackBuildResult BuildLayoutPack(std::wstring const& layoutFilePath) noexcept
    {
        PackBuildResult result{};

        try
        {
            auto const read = ReadLayoutFile(layoutFilePath);

            if (!read.Succeeded)
            {
                result.FailureKey = L"PackageFailedUnreadable";
                return result;
            }

            // A pack has to be small enough for the reader on the other PC to take.
            auto const survey = SurveyLayoutPackage(layoutFilePath);

            if (survey.TotalBytes > midiapp::MaximumContentPackBytes)
            {
                result.FailureKey = L"PackFailedTooBig";
                return result;
            }

            std::vector<ContentPackInput> inputs{};

            auto const fileName = std::filesystem::path{ layoutFilePath }.filename().wstring();
            auto primary = LayoutNameFromFileName(fileName) + LayoutFileExtension;

            if (!midiapp::IsAllowedContentPackPath(primary))
            {
                primary = std::wstring{ L"Layout" } + LayoutFileExtension;
            }

            ContentPackInput layout{};
            layout.Path = primary;
            layout.Bytes = ReadAllBytes(layoutFilePath);

            if (layout.Bytes.empty())
            {
                result.FailureKey = L"PackageFailedUnreadable";
                return result;
            }

            inputs.push_back(std::move(layout));

            auto const folder = std::filesystem::path{ layoutFilePath }.parent_path();

            std::error_code ignored{};

            for (auto const& name : PicturesOf(read.Document))
            {
                // A picture that is missing, or has a name a pack can't hold, is left out rather
                // than refused, as it is for a backup.
                if (SanitizeFileName(name).empty() || !midiapp::IsAllowedContentPackPath(name) ||
                    !std::filesystem::is_regular_file(folder / name, ignored))
                {
                    continue;
                }

                ContentPackInput picture{};
                picture.Path = name;
                picture.Bytes = ReadAllBytes(folder / name);

                if (!picture.Bytes.empty())
                {
                    inputs.push_back(std::move(picture));
                }
            }

            auto const& document = read.Document;

            if (document.HasOwnTheme)
            {
                AddDeckPicture(document.OwnTheme, true, inputs);
            }
            else if (auto const themeFile = CustomThemeFileNamed(CurrentThemeName(document.ThemeName)); !themeFile.empty())
            {
                auto const theme = ReadThemeFile(themeFile);
                auto const name = ThemeStemOf(std::filesystem::path{ themeFile }.filename().wstring()) + ThemeFileExtension;

                if (theme.Succeeded && midiapp::IsAllowedContentPackPath(name))
                {
                    ContentPackInput input{};
                    input.Path = InThemeFolder(name);
                    input.Bytes = ReadAllBytes(themeFile);

                    if (!input.Bytes.empty())
                    {
                        inputs.push_back(std::move(input));
                        AddDeckPicture(theme.Value, true, inputs);
                    }
                }
            }

            result.Bytes = midiapp::BuildContentPack(ContentPackKind::GlassLayout, primary, inputs);

            if (result.Bytes.empty())
            {
                result.FailureKey = L"PackFailedTooBig";
                return result;
            }

            result.FileCount = static_cast<uint32_t>(inputs.size());
            result.Succeeded = true;
        }
        catch (...)
        {
            result = PackBuildResult{};
            result.FailureKey = L"PackageFailedWrite";
        }

        return result;
    }

    _Use_decl_annotations_
    PackBuildResult BuildThemePack(std::wstring const& themeFilePath) noexcept
    {
        PackBuildResult result{};

        try
        {
            auto const read = ReadThemeFile(themeFilePath);

            if (!read.Succeeded || read.Value.Name.empty())
            {
                result.FailureKey = L"PackageFailedUnreadable";
                return result;
            }

            auto primary = ThemeStemOf(std::filesystem::path{ themeFilePath }.filename().wstring()) + ThemeFileExtension;

            if (!midiapp::IsAllowedContentPackPath(primary))
            {
                primary = ThemeFileNameFor(L"Theme");
            }

            std::vector<ContentPackInput> inputs{};

            ContentPackInput theme{};
            theme.Path = primary;
            theme.Bytes = ReadAllBytes(themeFilePath);

            if (theme.Bytes.empty())
            {
                result.FailureKey = L"PackageFailedUnreadable";
                return result;
            }

            inputs.push_back(std::move(theme));

            AddDeckPicture(read.Value, false, inputs);

            result.Bytes = midiapp::BuildContentPack(ContentPackKind::GlassTheme, primary, inputs);

            if (result.Bytes.empty())
            {
                result.FailureKey = L"PackFailedTooBig";
                return result;
            }

            result.FileCount = static_cast<uint32_t>(inputs.size());
            result.Succeeded = true;
        }
        catch (...)
        {
            result = PackBuildResult{};
            result.FailureKey = L"PackageFailedWrite";
        }

        return result;
    }

    _Use_decl_annotations_
    bool SummarizePack(OpenedContentPack const& pack, PackSummary& summary) noexcept
    {
        summary = PackSummary{};

        try
        {
            if (!pack.Succeeded())
            {
                return false;
            }

            auto const primary = PrimaryOf(pack);

            if (primary == nullptr)
            {
                return false;
            }

            summary.Kind = pack.Manifest.Kind;
            summary.FileCount = static_cast<uint32_t>(pack.Manifest.Files.size());

            for (auto const& file : pack.Manifest.Files)
            {
                summary.TotalBytes += file.Size;
            }

            auto const text = TextOf(primary->Bytes);

            if (text.empty())
            {
                return false;
            }

            if (summary.Kind == ContentPackKind::GlassLayout)
            {
                if (!IsLayoutFileName(primary->Name))
                {
                    return false;
                }

                auto const read = ReadLayoutFromJson(text);

                if (!read.Succeeded)
                {
                    return false;
                }

                summary.Name = read.Document.Name.empty()
                    ? LayoutNameFromFileName(primary->Name)
                    : read.Document.Name;
                summary.Description = read.Document.Description;
                summary.Provenance = read.Document.Provenance;
                summary.IsFromNewerVersion = read.IsFromNewerVersion;

                for (auto const& file : pack.Files)
                {
                    std::wstring name{};

                    if (IsInThemeFolder(file.Name, name) && IsThemeFileName(name))
                    {
                        auto const theme = ReadThemeFromJson(TextOf(file.Bytes));

                        if (theme.Succeeded)
                        {
                            summary.ThemeName = theme.Value.Name;
                        }
                    }
                }

                auto const id = summary.Provenance.has_value() ? summary.Provenance->Id : std::wstring{};

                if (!id.empty())
                {
                    for (auto const& path : ListLayoutFiles())
                    {
                        auto const existing = ReadLayoutFile(path);

                        if (existing.Succeeded && existing.Document.Provenance.has_value() &&
                            existing.Document.Provenance->Id == id)
                        {
                            summary.ExistingPath = path;
                            summary.ExistingProvenance = existing.Document.Provenance;
                            break;
                        }
                    }
                }

                return true;
            }

            if (summary.Kind == ContentPackKind::GlassTheme)
            {
                if (!IsThemeFileName(primary->Name))
                {
                    return false;
                }

                auto const read = ReadThemeFromJson(text);

                if (!read.Succeeded || read.Value.Name.empty())
                {
                    return false;
                }

                summary.Name = read.Value.Name;
                summary.Provenance = read.Value.Provenance;
                summary.IsFromNewerVersion = read.IsFromNewerVersion;

                auto const id = summary.Provenance.has_value() ? summary.Provenance->Id : std::wstring{};

                for (auto const& path : ListThemeFiles())
                {
                    auto const existing = ReadThemeFile(path);

                    if (!existing.Succeeded)
                    {
                        continue;
                    }

                    if (!id.empty() && existing.Value.Provenance.has_value() && existing.Value.Provenance->Id == id)
                    {
                        summary.ExistingPath = path;
                        summary.ExistingProvenance = existing.Value.Provenance;
                    }
                    else if (existing.Value.Name == summary.Name)
                    {
                        summary.NameTakenByPath = path;
                    }
                }

                // The one being updated is allowed to have the name.
                if (!summary.ExistingPath.empty() && !summary.NameTakenByPath.empty() &&
                    ReadThemeFile(summary.ExistingPath).Value.Name == summary.Name)
                {
                    summary.NameTakenByPath.clear();
                }

                // A built-in theme's name can never be used.
                summary.NameIsBuiltIn = FindBuiltInTheme(summary.Name) != nullptr;

                return true;
            }

            return false;
        }
        catch (...)
        {
            summary = PackSummary{};
            return false;
        }
    }

    _Use_decl_annotations_
    PackInstallResult InstallLayoutPack(
        OpenedContentPack const& pack,
        std::wstring const& targetFolder,
        std::wstring const& replacePath) noexcept
    {
        PackInstallResult result{};

        try
        {
            auto const primary = PrimaryOf(pack);

            if (!pack.Succeeded() || primary == nullptr || pack.Manifest.Kind != ContentPackKind::GlassLayout ||
                !IsLayoutFileName(primary->Name) || targetFolder.empty())
            {
                result.FailureKey = L"ImportFailedNoLayout";
                return result;
            }

            auto parsed = ReadLayoutFromJson(TextOf(primary->Bytes));

            if (!parsed.Succeeded)
            {
                result.FailureKey = L"ImportFailedUnreadable";
                return result;
            }

            auto document = std::move(parsed.Document);
            auto rewrite = false;

            std::error_code ignored{};
            std::filesystem::create_directories(targetFolder, ignored);

            std::filesystem::path layoutPath{};
            std::set<std::wstring> mayReplace{};

            if (!replacePath.empty())
            {
                layoutPath = replacePath;

                // The pictures of the layout being updated are its own to write over.
                auto const old = ReadLayoutFile(replacePath);

                if (old.Succeeded)
                {
                    for (auto const& name : PicturesOf(old.Document))
                    {
                        mayReplace.insert(Lower(name));
                    }
                }
            }
            else
            {
                layoutPath = std::filesystem::path{ targetFolder } /
                    (LayoutNameFromFileName(primary->Name) + LayoutFileExtension);

                if (std::filesystem::exists(layoutPath, ignored))
                {
                    layoutPath = MakeUnusedLayoutPath(targetFolder, LayoutNameFromFileName(primary->Name));
                }
            }

            if (layoutPath.empty())
            {
                result.FailureKey = L"ImportFailedWrite";
                return result;
            }

            auto const layoutFolder = layoutPath.parent_path();

            std::vector<std::pair<std::wstring, std::wstring>> renamed{};

            // The pictures first: a layout written before its artwork would draw empty boxes if
            // anything went wrong halfway.
            for (auto const& file : pack.Files)
            {
                if (&file == primary || file.Name.find(L'/') != std::wstring::npos)
                {
                    continue;
                }

                auto const safe = SanitizeFileName(file.Name);

                if (safe.empty() || safe != file.Name || !IsSupportedPictureFileName(safe))
                {
                    continue;
                }

                auto const placed = PlaceFile(layoutFolder, safe, file.Bytes, mayReplace, result.Files);

                if (placed.empty())
                {
                    result.FailureKey = L"ImportFailedWrite";
                    return result;
                }

                if (placed != safe)
                {
                    renamed.emplace_back(safe, placed);
                }
            }

            auto const bringsTheme = std::any_of(pack.Files.begin(), pack.Files.end(),
                [](StoredZipEntry const& file)
                {
                    std::wstring name{};
                    return IsInThemeFolder(file.Name, name);
                });

            // Only looked up when the pack brings something for it.
            auto const themesFolder = bringsTheme ? std::filesystem::path{ ThemesFolder() } : std::filesystem::path{};

            // The theme it brings, and that theme's picture, go to the themes folder.
            std::optional<Theme> packTheme{};
            std::wstring packThemeFileName{};
            std::vector<uint8_t> packThemeBytes{};
            std::wstring deckRenamedTo{};

            for (auto const& file : pack.Files)
            {
                std::wstring name{};

                if (!IsInThemeFolder(file.Name, name))
                {
                    continue;
                }

                if (IsThemeFileName(name))
                {
                    auto const theme = ReadThemeFromJson(TextOf(file.Bytes));

                    if (theme.Succeeded && !theme.Value.Name.empty() && FindBuiltInTheme(theme.Value.Name) == nullptr)
                    {
                        packTheme = theme.Value;
                        packThemeFileName = ThemeStemOf(name) + ThemeFileExtension;
                        packThemeBytes = file.Bytes;
                    }
                }
            }

            auto const deckName = packTheme.has_value()
                ? packTheme->Deck.ImageFileName
                : (document.HasOwnTheme ? document.OwnTheme.Deck.ImageFileName : std::wstring{});

            if (!deckName.empty() && !themesFolder.empty())
            {
                if (auto const deck = pack.FindFile(InThemeFolder(deckName)))
                {
                    auto const safe = SanitizeFileName(deckName);

                    if (!safe.empty() && safe == deckName && IsSupportedPictureFileName(safe))
                    {
                        auto const placed = PlaceFile(themesFolder, safe, deck->Bytes, {}, result.Files);

                        if (!placed.empty() && placed != safe)
                        {
                            deckRenamedTo = placed;
                        }
                    }
                }
            }

            if (document.HasOwnTheme && !deckRenamedTo.empty())
            {
                document.OwnTheme.Deck.ImageFileName = deckRenamedTo;
                rewrite = true;
            }

            if (packTheme.has_value() && !themesFolder.empty())
            {
                auto theme = *packTheme;

                if (!deckRenamedTo.empty())
                {
                    theme.Deck.ImageFileName = deckRenamedTo;
                }

                auto const existing = CustomThemeFileNamed(theme.Name);

                if (existing.empty())
                {
                    auto const target = UnusedPath(themesFolder, packThemeFileName);

                    auto const written = deckRenamedTo.empty()
                        ? WriteAllBytes(target, packThemeBytes)
                        : WriteThemeFile(theme, target.wstring());

                    if (written)
                    {
                        result.Files.push_back(target.wstring());
                    }
                }
                else
                {
                    auto const current = ReadThemeFile(existing);

                    auto const sameTheme = deckRenamedTo.empty() && HoldsBytes(existing, packThemeBytes);

                    auto const sameItem = current.Succeeded &&
                        current.Value.Provenance.has_value() && theme.Provenance.has_value() &&
                        !theme.Provenance->Id.empty() && current.Value.Provenance->Id == theme.Provenance->Id;

                    if (sameTheme)
                    {
                        result.Files.push_back(existing);
                    }
                    else if (sameItem &&
                        midiapp::CompareContentVersions(theme.Provenance->Version, current.Value.Provenance->Version) > 0 &&
                        !current.IsFromNewerVersion)
                    {
                        // A later version of the same theme replaces the earlier one.
                        if (deckRenamedTo.empty() ? WriteAllBytes(existing, packThemeBytes) : WriteThemeFile(theme, existing))
                        {
                            result.Files.push_back(existing);
                        }
                    }
                    else if (!sameItem)
                    {
                        // A different theme already has the name. The layout carries this one
                        // instead, so it looks the way it was made and nothing here changes.
                        document.HasOwnTheme = true;
                        document.OwnTheme = theme;
                        document.OwnTheme.Provenance.reset();
                        document.OwnTheme.Unknown = nullptr;
                        document.OwnTheme.FilePath.clear();
                        rewrite = true;
                    }
                }
            }

            if (!renamed.empty())
            {
                auto const rename = [&renamed](std::wstring& name)
                    {
                        for (auto const& [was, now] : renamed)
                        {
                            if (name == was)
                            {
                                name = now;
                                return;
                            }
                        }
                    };

                rename(document.BackgroundImage);

                for (auto& page : document.Pages)
                {
                    for (auto& control : page.Controls)
                    {
                        rename(control.Image.FileName);
                    }
                }

                rewrite = true;
            }

            if (rewrite)
            {
                // A newer version's layout is never written from the part of it this version
                // understood.
                if (document.IsFromNewerVersion)
                {
                    result.FailureKey = L"ImportFailedNewerVersion";
                    return result;
                }

                document.FilePath = layoutPath.wstring();

                if (!WriteLayoutFile(document, document.FilePath))
                {
                    result.FailureKey = L"ImportFailedWrite";
                    return result;
                }
            }
            else if (!WriteAllBytes(layoutPath, primary->Bytes))
            {
                result.FailureKey = L"ImportFailedWrite";
                return result;
            }

            result.Files.insert(result.Files.begin(), layoutPath.wstring());
            result.Path = layoutPath.wstring();
            result.Succeeded = true;
        }
        catch (...)
        {
            result = PackInstallResult{};
            result.FailureKey = L"ImportFailedWrite";
        }

        return result;
    }

    _Use_decl_annotations_
    PackInstallResult InstallThemePack(
        OpenedContentPack const& pack,
        std::wstring const& replacePath,
        std::wstring const& newName) noexcept
    {
        PackInstallResult result{};

        try
        {
            auto const primary = PrimaryOf(pack);

            if (!pack.Succeeded() || primary == nullptr || pack.Manifest.Kind != ContentPackKind::GlassTheme ||
                !IsThemeFileName(primary->Name))
            {
                result.FailureKey = L"ImportThemeFailedDetail";
                return result;
            }

            auto const read = ReadThemeFromJson(TextOf(primary->Bytes));

            if (!read.Succeeded || read.Value.Name.empty())
            {
                result.FailureKey = L"ImportThemeFailedDetail";
                return result;
            }

            auto theme = read.Value;
            auto rewrite = false;

            if (!newName.empty() && newName != theme.Name)
            {
                theme.Name = newName;
                rewrite = true;
            }

            if (FindBuiltInTheme(theme.Name) != nullptr)
            {
                result.FailureKey = L"ImportThemeFailedDetail";
                return result;
            }

            auto const folder = std::filesystem::path{ ThemesFolder() };

            if (folder.empty())
            {
                result.FailureKey = L"SaveThemeNoFolder";
                return result;
            }

            std::set<std::wstring> mayReplace{};

            if (!replacePath.empty())
            {
                auto const old = ReadThemeFile(replacePath);

                if (old.Succeeded && !old.Value.Deck.ImageFileName.empty())
                {
                    mayReplace.insert(Lower(old.Value.Deck.ImageFileName));
                }
            }

            if (!theme.Deck.ImageFileName.empty())
            {
                if (auto const deck = pack.FindFile(theme.Deck.ImageFileName))
                {
                    auto const safe = SanitizeFileName(theme.Deck.ImageFileName);

                    if (!safe.empty() && safe == theme.Deck.ImageFileName && IsSupportedPictureFileName(safe))
                    {
                        auto const placed = PlaceFile(folder, safe, deck->Bytes, mayReplace, result.Files);

                        if (!placed.empty() && placed != safe)
                        {
                            theme.Deck.ImageFileName = placed;
                            rewrite = true;
                        }
                    }
                }
            }

            auto const target = !replacePath.empty()
                ? std::filesystem::path{ replacePath }
                : UnusedPath(folder, rewrite ? ThemeFileNameFor(theme.Name) : ThemeStemOf(primary->Name) + ThemeFileExtension);

            if (rewrite)
            {
                // A newer version's theme is never written from the part of it this version read.
                if (read.IsFromNewerVersion)
                {
                    result.FailureKey = L"ImportFailedNewerVersion";
                    return result;
                }

                if (!WriteThemeFile(theme, target.wstring()))
                {
                    result.FailureKey = L"ImportFailedWrite";
                    return result;
                }
            }
            else if (!WriteAllBytes(target, primary->Bytes))
            {
                result.FailureKey = L"ImportFailedWrite";
                return result;
            }

            result.Files.insert(result.Files.begin(), target.wstring());
            result.Path = target.wstring();
            result.Succeeded = true;
        }
        catch (...)
        {
            result = PackInstallResult{};
            result.FailureKey = L"ImportFailedWrite";
        }

        return result;
    }

    _Use_decl_annotations_
    std::wstring UnusedThemeName(std::wstring const& name) noexcept
    {
        try
        {
            auto const themes = AllThemes();

            auto const taken = [&themes](std::wstring const& candidate)
                {
                    return std::any_of(themes.begin(), themes.end(),
                        [&candidate](Theme const& theme) { return theme.Name == candidate; });
                };

            if (!taken(name))
            {
                return name;
            }

            for (int32_t attempt = 2; attempt < 1000; ++attempt)
            {
                auto const candidate = name + L" " + std::to_wstring(attempt);

                if (!taken(candidate))
                {
                    return candidate;
                }
            }

            return {};
        }
        catch (...)
        {
            return {};
        }
    }
}
