# Network MIDI 2.0 (UDP) Transport — Spec Conformance Review

**Scope:** `Midi2.NetworkMidiTransport` project under `src/api/Transport/UdpNetworkMidi2Transport`
**Reference:** MIDI Association *Network MIDI 2.0 — UDP Transport*, document **M2-124-UM v1.0**

This document captures gaps, bugs, and TODOs found while walking the project against the spec. Items are grouped by severity and ordered roughly by impact.

---

## 1. Critical / Spec-Required Gaps

### 1.1 Incoming command handlers are missing or stubbed
`MidiNetworkConnection::ProcessIncomingMessage` has several command codes with empty or `TODO` bodies. Per spec, a receiver MUST respond appropriately:

| Command | Status | Spec requirement |
|---|---|---|
| `CommandCommon_NAK` (0x8F) | empty | Should at minimum log/correlate to original command and potentially abort retransmit attempts |
| `CommandCommon_RetransmitError` (0x81) | empty | Receiver must stop expecting the missing packet and may issue a Bye `TooManyMissingUmps` |
| `CommandCommon_SessionReset` (0x82) | empty | MUST reply with `SessionResetReply`, clear sequence numbers, clear FEC/retransmit buffers, and resume |
| `CommandCommon_SessionResetReply` (0x83) | empty | Must complete the reset state machine |
| `CommandClientToHost_InvitationWithAuthentication` (0x02) | empty | If unsupported you must NAK or send a Bye (`NoMatchingAuthenticationMethod`) |
| `CommandClientToHost_InvitationWithUserAuthentication` (0x03) | empty | Same as above |
| `CommandHostToClient_InvitationReplyPending` (0x11) | empty | Client should keep waiting (don't time out aggressively) |
| `CommandHostToClient_InvitationReplyAuthenticationRequired` (0x12) | empty | Client must respond with `InvitationWithAuthentication` or Bye |
| `CommandHostToClient_InvitationReplyUserAuthenticationRequired` (0x13) | empty | Same as above |
| `default:` branch | comment only | Spec says unknown command codes SHOULD be NAK'd with `CommandNotSupported` |

### 1.2 Receiving UMP Data without an active session
In the `CommandCommon_UmpData` case the code has a `TODO: If the session isn't active, we should reject any UMP data and NAK or BYE`. The spec is explicit: respond with Bye `SessionNotEstablished (0x05)`. The enum value is defined but never used.

### 1.3 Sender does nothing about `TooManyMissingUmps`
`MidiNetworkConnection::RequestMissingPackets` has a TODO acknowledging no rate-limiting/dedup. The spec requires that if the gap exceeds the local retransmit buffer (or after too many failed retransmit attempts) the receiver send Bye `TooManyMissingUmps (0x03)`. Currently it will spam retransmit requests indefinitely.

### 1.4 Authentication not implemented
- `WriteCommandInvitationWithAuthentication`, `WriteCommandInvitationWithUserAuthentication`, `WriteCommandInvitationReplyPending`, `WriteCommandInvitationReplyAuthenticationRequired`, `WriteCommandInvitationReplyUserAuthenticationRequired` all return `E_NOTIMPL`.
- `MidiNetworkHostDefinition` exposes `PasswordAuthentication` / `UserAuthentication` modes in JSON, but the host accepts any invitation regardless of policy (the comment in `HandleIncomingInvitation` says *"TODO: Also need to check auth mechanism and follow instructions in 6.4 and send a Bye if not supported"*). A non-compliant client will currently get a session anyway.

This is the single biggest missing chunk relative to the spec (sections **6.4–6.6** and **Appendix B**).

### 1.5 Capabilities byte in Invitation is treated as an enum, not a bitmap
````````
enum MidiNetworkCommandInvitationCapabilities : byte
{
    Capabilities_None = 0x00,
    Capabilities_ClientSupportsInvitationWithAuthentication = 0x01,
    Capabilities_ClientSupportsInvitationWithUserAuthentication = 0x02,
};
````````
Spec section **6.4** defines this as a bitmap (a client may advertise both auth methods). The TODO in `MidiNetworkDataWriter.h` notes it. Today it's hardcoded to `Capabilities_None` in `SendInvitation`.

### 1.6 ConnectionPolicy (IP allow-list / range) is not enforced
`MidiNetworkHostDefinition::ConnectionPolicy` exists, with JSON parsing in `Midi2.NetworkMidiConfigurationManager.cpp`, but `MidiNetworkHost::OnMessageReceived` blindly calls `CreateNetworkConnection` on the first packet from any remote without checking the policy. The validation in `ValidateHostDefinition` is even commented out. This is both a spec/security gap and a DoS vector.

### 1.7 First packet from a remote isn't required to be an Invitation
`MidiNetworkHost::OnMessageReceived` creates a `MidiNetworkConnection` for any UDP datagram bearing the `MIDI` header, regardless of whether it's a valid invitation. Spec **6.4** says the first command from a client MUST be an Invitation type. Code should refuse/ignore (or Bye-`NoPendingSession`) anything else before session establishment.

### 1.8 Invitation isn't retried by the client
`MidiNetworkClient::Start` sends a single `SendInvitation()` and the TODO in the code notes *"should be in a loop so it's repeated if there's no response"*. Spec recommends retry with backoff before giving up.

### 1.9 Polite Bye sequence concurrency / lifetime
`AttemptPoliteByeSequence` uses `Sleep(200)` between attempts but the loop will exit as soon as it detects `m_byeReplyReceived`. However, the writer is being torn down in `EndActiveSession` simultaneously, and `AttemptPoliteByeSequence` dereferences `m_writer` unconditionally. It also doesn't check for in-protocol completion before clearing state.

### 1.10 Bye/timeout doesn't fully tear down the connection object
`EndActiveSessionDueToTimeout` has a comment *"This should do more, causing the entire connection to go away"*. The connection remains in `TransportState`'s map indefinitely, leaking state and preventing a new session for the same remote until process restart.

---

## 2. Spec Conformance Bugs

### 2.1 NAK/Bye text length limits (constants are wrong)
`WriteCommandNAK` text length cap is 1020 bytes, but the payload length field is 1 byte (number of 32-bit words). Maximum is 255 × 4 = 1020 bytes **including** the original command header word, so the *text* max is actually 1016 bytes. Same logic for `Bye`. The constants `MIDI_MAX_NAK_MESSAGE_BYTE_COUNT` / `MIDI_MAX_BYE_MESSAGE_BYTE_COUNT` should be 1016 (and 1020 for Bye since Bye has no leading payload word).

### 2.2 `CalculatePaddedStringSizeIn32BitWords` silently truncates non-multiple-of-4 caps
When `s.size() >= maxByteCount` you return `maxByteCount / sizeof(uint32_t)`. Verify against:
- `MIDI_MAX_UMP_ENDPOINT_NAME_BYTE_COUNT = 98` (not a multiple of 4)
- `MIDI_MAX_UMP_PRODUCT_INSTANCE_ID_BYTE_COUNT = 42` (not a multiple of 4)

A 42-byte product instance ID would round up to 11 words (44 bytes), but your cap rounds down to `42/4 = 10` words (40 bytes), silently truncating two characters. The cap constants should be rounded up to the next multiple of 4 (100 and 44), or the rounding logic must be fixed.

### 2.3 `WritePaddedString` zero-padding correctness
```
if (count % sizeof(uint32_t) != 0)
{
    for (int i = 0; i < sizeof(uint32_t) - (count % sizeof(uint32_t)); i++)
        m_dataWriter.WriteByte(0);
}
```
If the maxByteCount loop exited via `return` due to hitting the cap, `count` may already be a multiple of 4 and you'll skip the padding. OK in that case. When the string is shorter than max and its length happens to be a multiple of 4, no padding bytes are added (correct), and the length-in-words calculation likewise produces the right count — make sure these stay in sync given the above bug.

### 2.4 `MidiNetworkAdvertiser::Advertise` missing return on success path
The function falls off the end after the `switch` without a `return S_OK`. With the default `case` going through `RETURN_IF_FAILED(E_FAIL)` it's hit, but the success path does `return S_OK` early. After the switch you have a `TraceLoggingWrite` block with no `return` — undefined behavior. Compiler should warn.

### 2.5 mDNS TXT records — verify case and protocol semantics
You advertise:
```
m_serviceInstance.TextAttributes().Insert(L"UMPEndpointName", midiEndpointName);
m_serviceInstance.TextAttributes().Insert(L"ProductInstanceId", midiProductInstanceId);
```
The Network MIDI 2.0 spec (**section 5**) defines these keys. mDNS TXT keys are case-insensitive per RFC 6763 §6.4, but the spec uses these capitalizations. Some interoperating implementations also rely on the exact casing — make sure your discovery parsing accepts any case. (Your reader code looks for exact key names via `HasKey`, so verify equivalent.)

### 2.6 `DontFragment(true)` set, but no MTU / packet-size accounting
`SendQueuedMidiMessagesToNetwork` has a `TODO: Use the UMP iterator to fill up the UDP packet with as many whole messages as we can fit without going over`. Today it dumps the whole queue into a single packet plus FEC headers, then sends it. With `DontFragment(true)` set, a big queue or a path with a small MTU will silently drop the packet. The spec recommends keeping each UDP datagram ≤ path MTU and not exceeding the 64-word UMP limit per command (which you check). You need to split datagrams when they would exceed ~1400 bytes.

### 2.7 FEC packet count upper bound
Spec section 7.2 supports more aggressive FEC (the payload length is a byte, so a UDP packet can contain many UmpData commands). `MIDI_NETWORK_FEC_PACKET_COUNT_UPPER_BOUND = 10` is a reasonable practical limit, but if you're enforcing it via `set_capacity(max(retransmit, FEC))`, ensure your config validator rejects values above this, not just clamps silently.

### 2.8 Retransmit buffer iteration on `boost::circular_buffer`
In `HandleIncomingRetransmitRequest`:
```
for (auto& it = firstPacket; it != m_retransmitBuffer.end() && it < endPacket && it->SequenceNumber >= startingSequenceNumber; it++)
```
- `endPacket = firstPacket + retransmitPacketCount` can overflow past `end()` for valid requests where the buffer doesn't have that many entries; rely on `it != end()` only.
- `it->SequenceNumber >= startingSequenceNumber` assumes monotonic sequence numbers, but they wrap at 16 bits. After wrap, your `MidiSequenceNumber` comparison may stop the loop prematurely.

### 2.9 Sequence number comparisons across wrap
`MidiNetworkConnection::ProcessIncomingMessage` uses `<=`, `+1`, and `>` on `MidiSequenceNumber`. Confirm `MidiSequenceNumber::operator<` and friends use "modulo distance" semantics (like TCP's `SEQ_LT` half-window). If it's a plain `uint16_t` compare, sequence wrap at 65535 → 0 will trigger spurious retransmits and a permanently broken session.

### 2.10 Empty UMP Data (keep-alive) sequence number handling
The spec says a UmpData command with zero UMP words is allowed as a keep-alive. Your code's branch for `sequenceNumber == m_lastReceivedUmpCommandSequenceNumber + 1` falls through correctly, but the "skipped data" branch (`sequenceNumber > m_lastReceivedUmpCommandSequenceNumber + 1`) does **not** advance `m_lastReceivedUmpCommandSequenceNumber` after the retransmit succeeds — meaning each subsequent UmpData also gets `RequestMissingPackets`'d until the retransmit arrives. Per spec you should either advance with each delivered packet from the retransmit set or queue-and-reorder. There's also no guard against requesting before the previous request completes (TODO noted in code).

### 2.11 Session reset (sections 6.13 / 6.14) is not wired up
You can `WriteCommandSessionReset` and `WriteCommandSessionResetReply`, but nothing in the codebase ever invokes them, and inbound handlers are empty TODOs. Per spec, a session reset must:
1. Clear retransmit / FEC / receive sequence state
2. Reply with `SessionResetReply`
3. Restart sequence numbers from 0

The infrastructure to do this exists in `ResetSequenceNumbers()` but it isn't wired up.

### 2.12 **Compound packet payload parsing bug (high impact)**
`HandleIncomingBye()` ignores the supplied reason byte and text payload (it doesn't even read them). Look at the inner `while` loop in `ProcessIncomingMessage`: when Bye is decoded, the header has `CommandPayloadLength` reason text words that you must `reader.ReadUInt32()`-skip; otherwise the next command parse picks up payload bytes as a command code.

The same bug exists for **NAK** (you don't read the original command header word + text), **RetransmitError**, **invitation auth payloads**, and the unknown-command `default` path. Any compound UDP packet that contains anything but UmpData/Ping/Invitation will be mis-parsed.

### 2.13 Padding-zero handling on inbound strings
`ReadUtf8String` reads exactly `byteCount` bytes, then constructs `std::wstring`. Zero-padding bytes from the wire become embedded NULs in the resulting string. You should trim trailing `\0` and validate UTF-8. As written, comparisons like `s == L"foo"` will fail because of trailing nulls.

### 2.14 Host-side endpoint naming uses client-supplied values (correct, FYI)
You correctly use `clientUmpEndpointName` / `clientProductInstanceId` for `CreateNewHostEndpointToRemoteClient`. ✅ No change needed.

---

## 3. Smaller Issues / Cleanup

- `MidiNetworkConnection::Initialize` calls `socket.GetOutputStreamAsync(hostName, port).get()` synchronously on the calling thread. For host-side sockets bound to a service name, that's fine; for a client socket that's been `ConnectAsync`'d to the remote, you should use `socket.OutputStream` instead — using `GetOutputStreamAsync` after `ConnectAsync` may fail or create a redundant stream.
- `m_emptyUmpIterationCounter` and `m_emptyUmpIterationIntervalBeforeSending` in `MidiNetworkConnection.h` are declared but never read or written.
- `MidiNetworkHost::Stop` has a `TODO: Stop packet processing thread using the jthread stop token` but there's no such jthread in the host class.
- `MidiNetworkHost::Start` calls `BindServiceNameAsync(...).get()` with no error handling and no fallback if the port is in use.
- Server-side `MidiNetworkHostConnectionPolicy::PolicyAllowFromIpRange` / `PolicyAllowFromIpList` enums exist but the underlying IP range / list storage is commented out in `MidiNetworkHostDefinition`.
- `m_socketWriterLock` is taken inside `RequestMissingPackets`, then later in `SendQueuedMidiMessagesToNetwork` from a different thread, while `m_outgoingUmpMessageQueueLock` is also held. Verify no lock inversion exists (specifically watch the case where `HandleIncomingRetransmitRequest` is invoked while `OutboundProcessingThreadWorker` is mid-send).
- `MidiNetworkConnection::EndActiveSession` resets `m_writer`. Anything still in flight from `OutboundProcessingThreadWorker` will hit `RETURN_HR_IF_NULL(E_UNEXPECTED, m_writer)` or worse if the null check races. Consider stopping the outbound thread and *joining* it before nulling `m_writer`.
- `MidiNetworkClient::Shutdown` uses `m_networkConnection->Shutdown()` after iterating `GetAllNetworkConnectionsForClient` which already shut it down — double-shutdown risk.
- The Configuration Manager `query-capabilities` response advertises `RESTART_ENDPOINT`, `DISCONNECT_ENDPOINT`, `RECONNECT_ENDPOINT` as `false` even though `DisconnectClient` and `StartHost`/`StopHost` verbs are implemented.

---

## 4. Suggested Prioritized Fix Order

1. **Parsing correctness** (§2.12, §2.13, §2.9, §2.10) — without these, anything beyond the happy path can misbehave.
2. **String length math** (§2.1, §2.2) — silent truncation of endpoint names / product IDs is a hard-to-debug interop failure.
3. **Session lifecycle gaps** (§1.1 SessionReset, §1.2 SessionNotEstablished Bye, §1.3 TooManyMissingUmps Bye, §1.10 connection teardown on timeout).
4. **Connection policy enforcement** (§1.6, §1.7) — security / DoS.
5. **MTU-aware UDP packetization** (§2.6).
6. **Authentication** (§1.4, §1.5) — large feature, but needed for spec compliance to claim full support.
7. Cleanup of `TODO`s and lifecycle/concurrency tightening (§1.9, §2.7, and items in §3).

---

## 5. Quick Reference — Spec Sections Touched

| Spec Section | Topic | Implementation Status |
|---|---|---|
| §5 | mDNS service discovery / TXT records | Implemented; verify case-insensitive parsing |
| §6.4 | Invitation (basic) | Implemented |
| §6.5 | Invitation w/ shared-secret auth | **Not implemented** |
| §6.6 | Invitation w/ user auth | **Not implemented** |
| §6.7 | Invitation Reply Accepted | Implemented |
| §6.8 | Invitation Reply Pending | **Not implemented** |
| §6.9 | Invitation Reply Auth Required | **Not implemented** |
| §6.10 | Invitation Reply User Auth Required | **Not implemented** |
| §6.11 | Ping / Ping Reply | Implemented (latency calc TODO) |
| §6.12 | NAK | Outbound only; inbound ignored |
| §6.13 | Session Reset | Outbound only; inbound ignored |
| §6.14 | Session Reset Reply | Outbound only; inbound ignored |
| §6.15 | NAK message size | **Constant likely off-by-one (1020 vs 1016 bytes)** |
| §6.16 | Bye / Bye Reply | Implemented; **payload not consumed on read** |
| §7.1 | UMP Data command | Implemented; **no MTU-aware packing** |
| §7.2 | Retransmit Request / Error | Request implemented; **inbound RetransmitError ignored** |
| §7.2 | Forward Error Correction | Implemented |

---

*Generated for review of the `Midi2.NetworkMidiTransport` C++ project; not all spec mandates have been individually verified — please cross-check critical items against the PDF before acting.*