# Sync to device

Sends a layout from one PC running Windows MIDI Glass to another, like a desktop where the layout is designed and a Surface where it's played. The first send of a layout carries everything. Later sends carry the layout file and only the pictures, videos and theme files the other PC doesn't already have.

This folder is the part with no UI and no XAML, so the unit tests compile it unchanged. The screens are in `SyncDialogs.cpp`, `SyncReceiveWindow.cpp`, `MainWindowSync.cpp`, `EditorSync.cpp` and `RuntimeWindowUpdate.cpp` one folder up. The connection, keys, pairing and discovery are shared code in `midi-app-shared` (`DeviceLink*.h`, `PairedDevices.h`), so Windows MIDI Patchbay can use them later.

## Files

| File | What it does |
| --- | --- |
| `SyncPack.h` | Splits a layout pack into an offer and its files, works out which files the receiver needs, and puts the pack back together on the receiver. |
| `SyncFileIndex.h` | SHA-256 of the files the receiver already has, cached by path, size and last-write time in `%LOCALAPPDATA%\Microsoft\MIDI Glass\SyncFileIndex.json`. |
| `SyncMemory.h` | Approvals for this session, kept in memory only, and what was last installed from each sender, in `SyncRecords.json`. |
| `SyncProtocol.h` | The sender, the receiver and the sending side of pairing, on top of a device link connection. |

## Contract

- It isn't MIDI. Two copies of Glass talk directly over TCP with TLS 1.3 from Windows (SChannel). Both sides show a self-signed certificate around a per-user ECDSA P-256 key that never leaves the Microsoft Software Key Storage Provider.
- PCs pair once. Each side shows a 6-digit code made from both keys and two random nonces, with a commitment sent first (numeric comparison, as in Bluetooth Secure Simple Pairing and ZRTP). A person at each PC says whether the codes match. Each side remembers the other's key hash only when both said yes.
- After that, every connection is checked against the remembered key. The sender sends nothing to a key it didn't pair with. The receiver drops a PC it isn't paired with right after the handshake, unless the person there has pairing open, and then that PC can only pair.
- The receiver listens and advertises (DNS-SD, `_midiglass._tcp`) only while its Receive screen is open. The sender only connects out.
- Sending is always started by a person. There's no live preview and nothing is sent per edit.
- One sync: Offer (layout id, name, manifest bytes, signatures), Need (hashes), the files asked for, Done, Result. The layout file is always sent.

## Invariants

- What arrives is untrusted. The receiver rebuilds the pack and checks it with `midiapp::OpenContentPack`, the same check a pack file from a USB stick gets: the 512 MB cap, every size and hash against the manifest, and every signature against the manifest bytes. It then goes through the same import dialog and install as a pack file (`sharing::ImportOpenedPackAsync`). A signed pack stays signed after a sync that sent only part of it, because the signature covers the manifest. Like any layout from somewhere else, it asks before sending SysEx the first time it runs (`document\SysExReview.h`).
- Every frame has a cap for its type, checked before anything is allocated. Every read has a timeout. The receiver takes only the files it asked for, each once, at the size the manifest lists. One connection at a time.
- A file the receiver thinks it has is read again and hashed when the pack is rebuilt. If it changed, it counts as missing.
- Replacing a layout asks once per Glass session, per layout, per paired sender. Unpairing that sender ends what it was allowed. A copy changed on the receiver since that sender last sent it always asks again and says so.
- A window running the layout shows "Update ready" and loads the new copy only when asked.
- Device names from the network are cleaned of direction overrides, zero-width and control characters before they're shown.

## What it doesn't do

- No "always accept" from a PC. That's not decided.
- No themes on their own, and no Patchbay patches yet.
- Nothing is pushed while editing. No live preview.
- No firewall changes without a person asking. The Receive screen offers to allow Glass on private networks, from the local subnet only, which needs an administrator.
- It doesn't work across the internet or on a network Windows calls Public, and it doesn't try to.
