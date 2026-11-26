// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "PatchFolderMoveTests.h"

#include "PatchFolderMove.h"

#include <windows.h>
#include <winioctl.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using namespace midipatchbay;

namespace
{
    // A folder of the test's own in %TEMP%. It's always new, so removing it can't touch anything else.
    class ScratchFolder
    {
    public:
        ScratchFolder()
        {
            static int s_count{ 0 };

            wchar_t temp[MAX_PATH + 1]{};

            if (::GetTempPathW(MAX_PATH + 1, temp) == 0)
            {
                return;
            }

            m_root = std::filesystem::path{ temp } / (L"MidiPatchbayFolderMoveTest-" +
                std::to_wstring(::GetCurrentProcessId()) + L"-" +
                std::to_wstring(::GetTickCount64()) + L"-" +
                std::to_wstring(++s_count));

            m_created = ::CreateDirectoryW(m_root.c_str(), nullptr) != FALSE;
        }

        ~ScratchFolder()
        {
            if (m_created)
            {
                std::error_code ec{};
                std::filesystem::remove_all(m_root, ec);
            }
        }

        ScratchFolder(ScratchFolder const&) = delete;
        ScratchFolder& operator=(ScratchFolder const&) = delete;

        bool Created() const noexcept { return m_created; }

        std::filesystem::path Path(_In_ std::wstring const& name) const { return m_root / name; }

    private:
        std::filesystem::path m_root{};
        bool m_created{ false };
    };

    void WriteText(_In_ std::filesystem::path const& path, _In_ std::string const& text)
    {
        std::filesystem::create_directories(path.parent_path());

        std::ofstream file{ path, std::ios::binary };
        file << text;
    }

    std::string ReadText(_In_ std::filesystem::path const& path)
    {
        std::ifstream file{ path, std::ios::binary };

        return std::string{ std::istreambuf_iterator<char>{ file }, std::istreambuf_iterator<char>{} };
    }

    bool Exists(_In_ std::filesystem::path const& path)
    {
        return ::GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
    }

    bool MarkReadOnly(_In_ std::filesystem::path const& folder)
    {
        auto const attributes = ::GetFileAttributesW(folder.c_str());

        return attributes != INVALID_FILE_ATTRIBUTES &&
            ::SetFileAttributesW(folder.c_str(), attributes | FILE_ATTRIBUTE_READONLY) != FALSE;
    }

    // The same junction mklink /J makes, which needs no privilege.
    bool MakeJunction(_In_ std::filesystem::path const& link, _In_ std::filesystem::path const& target)
    {
        if (!::CreateDirectoryW(link.c_str(), nullptr))
        {
            return false;
        }

        auto const handle = ::CreateFileW(link.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);

        if (handle == INVALID_HANDLE_VALUE)
        {
            return false;
        }

        auto const substitute = L"\\??\\" + target.wstring();
        auto const print = target.wstring();

        auto const substituteBytes = static_cast<USHORT>(substitute.size() * sizeof(wchar_t));
        auto const printBytes = static_cast<USHORT>(print.size() * sizeof(wchar_t));

        // Mount point layout: tag, length, reserved, name offsets and lengths, then both names with nulls.
        std::vector<BYTE> buffer(16 + substituteBytes + sizeof(wchar_t) + printBytes + sizeof(wchar_t));

        DWORD const tag = IO_REPARSE_TAG_MOUNT_POINT;
        USHORT const fields[] = {
            static_cast<USHORT>(buffer.size() - 8),
            0,
            0,
            substituteBytes,
            static_cast<USHORT>(substituteBytes + sizeof(wchar_t)),
            printBytes };

        std::memcpy(buffer.data(), &tag, sizeof(tag));
        std::memcpy(buffer.data() + 4, fields, sizeof(fields));
        std::memcpy(buffer.data() + 16, substitute.c_str(), substituteBytes);
        std::memcpy(buffer.data() + 16 + substituteBytes + sizeof(wchar_t), print.c_str(), printBytes);

        DWORD returned{ 0 };

        auto const made = ::DeviceIoControl(handle, FSCTL_SET_REPARSE_POINT, buffer.data(),
            static_cast<DWORD>(buffer.size()), nullptr, 0, &returned, nullptr);

        ::CloseHandle(handle);

        return made != FALSE;
    }
}

void PatchFolderMoveTests::AnEarlierFolderMovesWhole()
{
    ScratchFolder scratch{};
    VERIFY_IS_TRUE(scratch.Created());

    auto const previous = scratch.Path(L"MIDI Patchbay");
    auto const current = scratch.Path(L"MIDI Patches");

    WriteText(previous / L"Studio.midipatch", "studio");
    WriteText(previous / L"Synth.midici", "responder");
    WriteText(previous / L"Earlier versions" / L"Studio.midipatch", "version 1");

    VERIFY_ARE_EQUAL(current.wstring(), MoveEarlierPatchFolder(previous.wstring(), current.wstring()));

    VERIFY_IS_FALSE(Exists(previous));
    VERIFY_IS_TRUE(ReadText(current / L"Studio.midipatch") == "studio");
    VERIFY_IS_TRUE(ReadText(current / L"Synth.midici") == "responder");
    VERIFY_IS_TRUE(ReadText(current / L"Earlier versions" / L"Studio.midipatch") == "version 1");
}

void PatchFolderMoveTests::BothFoldersMergeAndNothingIsReplaced()
{
    ScratchFolder scratch{};
    VERIFY_IS_TRUE(scratch.Created());

    auto const previous = scratch.Path(L"MIDI Patchbay");
    auto const current = scratch.Path(L"MIDI Patches");

    WriteText(previous / L"Studio.midipatch", "from the earlier folder");
    WriteText(previous / L"Live.midipatch", "live");
    WriteText(previous / L"Earlier versions" / L"Live.midipatch", "live version 1");
    VERIFY_IS_TRUE(MarkReadOnly(previous / L"Earlier versions"));

    WriteText(current / L"Studio.midipatch", "already here");
    WriteText(current / L"Earlier versions" / L"Old.midipatch", "old version 1");

    VERIFY_ARE_EQUAL(current.wstring(), MoveEarlierPatchFolder(previous.wstring(), current.wstring()));

    // A name both folders had keeps what the new folder had, and the other copy stays put.
    VERIFY_IS_TRUE(ReadText(current / L"Studio.midipatch") == "already here");
    VERIFY_IS_TRUE(ReadText(previous / L"Studio.midipatch") == "from the earlier folder");

    VERIFY_IS_TRUE(ReadText(current / L"Live.midipatch") == "live");
    VERIFY_IS_TRUE(ReadText(current / L"Earlier versions" / L"Live.midipatch") == "live version 1");
    VERIFY_IS_TRUE(ReadText(current / L"Earlier versions" / L"Old.midipatch") == "old version 1");
    VERIFY_IS_FALSE(Exists(previous / L"Live.midipatch"));
    VERIFY_IS_FALSE(Exists(previous / L"Earlier versions"));
}

void PatchFolderMoveTests::AnEmptiedEarlierFolderIsRemoved()
{
    ScratchFolder scratch{};
    VERIFY_IS_TRUE(scratch.Created());

    auto const previous = scratch.Path(L"MIDI Patchbay");
    auto const current = scratch.Path(L"MIDI Patches");

    WriteText(previous / L"Live.midipatch", "live");
    WriteText(current / L"Studio.midipatch", "studio");

    // the way OneDrive and Explorer often leave a Documents folder
    VERIFY_IS_TRUE(MarkReadOnly(previous));

    VERIFY_ARE_EQUAL(current.wstring(), MoveEarlierPatchFolder(previous.wstring(), current.wstring()));

    VERIFY_IS_FALSE(Exists(previous));
    VERIFY_IS_TRUE(ReadText(current / L"Live.midipatch") == "live");
    VERIFY_IS_TRUE(ReadText(current / L"Studio.midipatch") == "studio");
}

void PatchFolderMoveTests::NothingHappensWithoutAnEarlierFolder()
{
    ScratchFolder scratch{};
    VERIFY_IS_TRUE(scratch.Created());

    auto const previous = scratch.Path(L"MIDI Patchbay");
    auto const current = scratch.Path(L"MIDI Patches");

    VERIFY_ARE_EQUAL(current.wstring(), MoveEarlierPatchFolder(previous.wstring(), current.wstring()));
    VERIFY_ARE_EQUAL(current.wstring(), MoveEarlierPatchFolder(std::wstring{}, current.wstring()));

    // The app makes its folder when it first saves a patch.
    VERIFY_IS_FALSE(Exists(current));
    VERIFY_IS_FALSE(Exists(previous));
}

void PatchFolderMoveTests::AFolderInUseStaysUntilItCanMove()
{
    ScratchFolder scratch{};
    VERIFY_IS_TRUE(scratch.Created());

    auto const previous = scratch.Path(L"MIDI Patchbay");
    auto const current = scratch.Path(L"MIDI Patches");

    WriteText(previous / L"Studio.midipatch", "studio");

    // Shared every way, and the folder still can't move while it's open.
    auto const open = ::CreateFileW((previous / L"Studio.midipatch").c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    VERIFY_IS_TRUE(open != INVALID_HANDLE_VALUE);

    auto const whileOpen = MoveEarlierPatchFolder(previous.wstring(), current.wstring());
    ::CloseHandle(open);

    VERIFY_ARE_EQUAL(previous.wstring(), whileOpen);
    VERIFY_IS_FALSE(Exists(current));
    VERIFY_IS_TRUE(ReadText(previous / L"Studio.midipatch") == "studio");

    // the next start tries again
    VERIFY_ARE_EQUAL(current.wstring(), MoveEarlierPatchFolder(previous.wstring(), current.wstring()));
    VERIFY_IS_FALSE(Exists(previous));
    VERIFY_IS_TRUE(ReadText(current / L"Studio.midipatch") == "studio");
}

void PatchFolderMoveTests::AFileWithTheNewNameKeepsTheEarlierFolder()
{
    ScratchFolder scratch{};
    VERIFY_IS_TRUE(scratch.Created());

    auto const previous = scratch.Path(L"MIDI Patchbay");
    auto const current = scratch.Path(L"MIDI Patches");

    WriteText(previous / L"Studio.midipatch", "studio");
    WriteText(current, "a file, not a folder");

    VERIFY_ARE_EQUAL(previous.wstring(), MoveEarlierPatchFolder(previous.wstring(), current.wstring()));

    VERIFY_IS_TRUE(ReadText(previous / L"Studio.midipatch") == "studio");
    VERIFY_IS_TRUE(ReadText(current) == "a file, not a folder");
}

void PatchFolderMoveTests::ALinkedFolderIsNeverEmptied()
{
    ScratchFolder scratch{};
    VERIFY_IS_TRUE(scratch.Created());

    auto const previous = scratch.Path(L"MIDI Patchbay");
    auto const current = scratch.Path(L"MIDI Patches");
    auto const elsewhere = scratch.Path(L"Somewhere else");

    WriteText(elsewhere / L"Studio.midipatch", "studio");
    WriteText(current / L"Live.midipatch", "live");
    VERIFY_IS_TRUE(MakeJunction(previous, elsewhere));

    auto const used = MoveEarlierPatchFolder(previous.wstring(), current.wstring());
    auto const linkStillThere = Exists(previous);

    // the link only; what it points to goes with the scratch folder
    ::RemoveDirectoryW(previous.c_str());

    VERIFY_ARE_EQUAL(current.wstring(), used);
    VERIFY_IS_TRUE(linkStillThere);
    VERIFY_IS_TRUE(ReadText(elsewhere / L"Studio.midipatch") == "studio");
    VERIFY_IS_FALSE(Exists(current / L"Studio.midipatch"));
}
