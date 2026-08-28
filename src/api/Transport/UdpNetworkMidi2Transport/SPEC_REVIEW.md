# Network MIDI 2.0 (UDP) Transport — Spec Conformance Review

**Scope:** `Midi2.NetworkMidiTransport` under `src/api/Transport/UdpNetworkMidi2Transport`
**Reference:** MIDI Association *Network MIDI 2.0 — UDP Transport*, **M2-124-UM v1.0**, 2024-11-06

Spec statements below have been read from the specification itself rather than recalled. Where a
requirement is quoted, it is quoted verbatim. Commands are referred to by name rather than
section number, because the spec numbers the invitation commands inconsistently between its own
sections.

---

## 1. Open items

### 1.1 Authentication is not implemented

`Invitation with Authentication`, `Invitation with User Authentication`, and the two
`Invitation Reply: ... Authentication Required` commands are unimplemented. Tracked as
[#733](https://github.com/microsoft/MIDI/issues/733). This is the only remaining feature-sized
gap in the protocol.

Current behavior is deliberately fail-closed rather than fail-open:

- Configuration validation **refuses to start a host** configured for any authentication mode, so
  a user who asks for a password never silently gets an unprotected host.
- Inbound authentication commands are answered with a Bye rather than ignored.
- `MidiNetworkCredentials.h` holds the credential-resolution seam, the security constraints, and
  the open questions about where secrets live. Read that before starting the work.
- `MidiNetworkGenerateCryptoNonce` is implemented (`BCryptGenRandom`).
- `ComputeAuthenticationDigest` is deliberately **not** implemented. It must follow Appendix B
  exactly; an approximation would interoperate with nothing while appearing to work locally.

The capabilities byte is correctly a bitmap (spec Table 11: D0 = supports Invitation with
Authentication, D1 = supports Invitation with User Authentication), but we advertise
`Capabilities_None` because we support neither.

### 1.2 A device connected in both roles is declined rather than correlated

> "Devices which act as both a Host and as a Client and which represent the same UMP Endpoint
> shall use the same Endpoint Name and Product Instance Id in both roles... By using the same
> Endpoint Name and Product Instance Id, other Hosts and Clients can better recognize Devices
> which may already be connected."

Endpoint identity is role-free, so such a device resolves to one endpoint and the second session
is detected and declined with a Bye (`TooManyOpenSessions` from the host side,
`InvitationCanceled` from the client side) plus a clear log. Related to
[#750](https://github.com/microsoft/MIDI/issues/750).

Whether we should instead *correlate* the two connections onto one endpoint is an open design
question: it requires deciding which connection wins, or multiplexing one endpoint across two.
Declining is safe and diagnosable, but it is not what the spec's wording is reaching for.

### 1.3 Retransmit request timing is not yet spec-shaped

> "The Device should delay sending the Retransmit Request Command for a short duration, for
> example 10 milliseconds. That will help recovering from out of order packets and it prevents
> sending Retransmit Requests too often."
> "The Retransmit Request Command should be repeated (with increasing delay) until the Device
> receives the requested UMP Data Command(s), a Retransmit Error Command, a NAK Command, or until
> a timeout occurs."

We request immediately on gap detection and repeat at most once per received datagram, up to a
bounded attempt count. Both are `should`, and the practical impact is limited because the forward
error correction window already absorbs most reordering, but a short delay and a backoff would
reduce needless requests on a lossy link. Requires a timer, which the connection does not have.

### 1.4 Smaller items

- **Only the first advertised IP address is used.** `System.Devices.IpAddress` can carry several
  (the spec shows a host with two A records and one AAAA); we take `array.at(0)` and never try
  the others if it is unreachable.
- **Discovery does not exclude our own hosts.** `OnDeviceWatcherAdded` carries a
  `TODO: Search our host entries to make sure the host is not *this* host`. A machine running
  both a host and a matching client entry could invite itself.
- **IP allow-list matching is by canonical name string.** Works for the literal addresses the
  JSON accepts, but will not match an equivalent-but-differently-formatted address.

---

## 2. Verified as conforming

Recorded so these are not re-investigated.

### Identity and addressing

- **Endpoint identity.** The SWD instance id is derived from UMP Endpoint Name plus Product
  Instance Id, which is what the spec nominates: "Operating systems and devices may use the
  UMPEndpointName and ProductInstanceId to recall Device properties when reconnecting to
  devices." Neither field alone is sufficient — Product Instance Id is only "statistically
  unique" and is shared across a device's Host instances, while the UMP Endpoint Name is required
  to differ per Host instance but is unique only within a device. Address, port and role are
  deliberately excluded, and the role is published as
  `PKEY_MIDI_NetworkMidiConnectionRole` instead.
- **Connection keying.** The host keys connections by remote address and port, per "the Host
  shall uniquely identify the connection for each Client via the Client's source IP address and
  UDP port number." Separate from, and correctly independent of, endpoint identity.
- **Ports are not identity.** "Clients may use a new UDP port number for every Session with a
  Host", and clients have no mDNS presence at all.
- **Missing identity is refused.** A remote which omits either the UMP Endpoint Name or the
  Product Instance Id is logged and refused rather than being given a synthesised identity.

### Discovery

- mDNS PTR/SRV/TXT publication, with `UMPEndpointName` and `ProductInstanceId` TXT keys.
- TXT records are parsed on discovery. Configured client entries match against the mDNS device
  id, the advertised Product Instance Id, or the advertised UMP Endpoint Name, all
  case-insensitively — RFC 6763 makes TXT keys case-insensitive.

### Session lifecycle

- **Invitation retry.** "The Invitation Command should be sent repeatedly, with a reasonable
  delay between Invitation Commands, until a reply is received... If the Client considers the
  Invitation failed, the Client shall terminate the Invitation with a Bye Command with reason
  0x80 (Invitation Canceled)." Retries are driven by the watchdog tick and end with that Bye.
- **Re-invitation on an established session.** "If a Host receives an Invitation from a remote
  Client with which it is already in a Session, then it shall respond with Invitation Reply:
  Accepted" — implemented.
- **Session Not Established.** UMP Data, Retransmit Request, Retransmit Error, Session Reset and
  Session Reset Reply all answer with Bye reason 0x05 when received outside an established
  session, as the spec separately requires for each. Limited to one Bye per datagram so a peer
  talking to a dead session cannot make us flood it.
- **Session Reset / Session Reset Reply** clear sequence state and acknowledge.
- **First command must be an invitation.** A host allocates nothing for a remote whose opening
  command is anything else, which is both spec conformance and the primary defense against a
  forged source address costing us a connection and a thread.

### Data path

- **Maximum datagram size.** Outbound datagrams are packed to a 1400-byte payload budget, the
  spec's own limit: "UDP packets should not exceed 1400 bytes." With `DontFragment` set this
  stays inside a 1500-byte MTU for both IPv4 (+28) and IPv6 (+48).
- **UMP Data commands** carry whole UMP messages only, capped at 64 words, each with its own
  sequence number and retransmit-buffer entry.
- **Sequence number wrap.** `MidiSequenceNumber` compares using a 60000-tick comparison delta
  rather than a plain `uint16_t` compare.
- **Retransmit refusal.** A request we cannot satisfy is answered with a Retransmit Error, or
  with NAK `CommandNotSupported` when buffering is disabled, per "If a Device does not implement
  the Retransmit mechanism, it shall reply to the Retransmit Request Command with a NAK Command
  with reason 0x01."
- **Honoring a peer's refusal.** A NAK of `CommandNotSupported` for a retransmit request stops
  us asking for the rest of the session, and a Retransmit Error abandons the current gap. Gaps
  are abandoned after a bounded number of attempts and the receiver resynchronizes rather than
  stalling — the subject of [#1003](https://github.com/microsoft/MIDI/issues/1003).
- **Compound packet parsing.** Each command's declared payload length is validated against the
  datagram before parsing, and any payload a handler does not consume is skipped so that payload
  bytes can never be interpreted as the next command header.
- **String framing.** Declared word length and bytes written are guaranteed to agree, UTF-8
  truncation backs up to a whole character, and inbound strings are trimmed at the first padding
  null.

---

## 3. Deliberate decisions, not gaps

- **`Bye` reason 0x03 (Too Many Missing UMP Packets) is never sent automatically.** It appears in
  the Bye reason table but the spec contains no requirement to send it — there is no "shall" for
  it anywhere. Ending a session over unrecoverable loss is exactly the behavior reported as a
  bug in [#1003](https://github.com/microsoft/MIDI/issues/1003), where the reporter asked that we
  "accept that some data unfortunately has been lost and just keep going." We resynchronize and
  continue instead.
- **`query-capabilities` reports `false` for `RESTART_ENDPOINT`, `DISCONNECT_ENDPOINT` and
  `RECONNECT_ENDPOINT`.** These are the generic per-endpoint verbs, which this transport does not
  implement; it exposes `start-host`, `stop-host`, `connect-direct` and `disconnect-client`
  instead, because starting a host and connecting a client are not the same operation and cannot
  share one endpoint-level verb. An earlier revision of this document called this a defect; it is
  not.

---

## 4. Corrections to earlier revisions of this document

- **`MidiNetworkAdvertiser::Advertise` was reported as falling off the end without a return.** It
  does not. Every switch case either returns `S_OK` or goes through `RETURN_IF_FAILED`, so the
  trailing block is unreachable. No defect.
- **`UniqueIdentifier` was suspected of causing a device-manager collision behind #750.** It does
  not. `commonProperties.UniqueIdentifier` is only copied to `PKEY_MIDI_SerialNumber` as an
  informational property; it is not a dedup or match key.
- **TXT record key casing was flagged as needing verification, then as unparsed.** Now parsed,
  case-insensitively. See §2.
- **`query-capabilities` was listed as under-reporting.** It is not. See §3.
- **NAK/Bye maximum text lengths.** The first revision was right that the NAK constant was wrong,
  and the corrected value is 1016 bytes: the payload length field counts the mandatory
  original-command header word, leaving 254 words of text. Bye has no leading payload word, so
  1020 was already correct.

---

## 5. Spec coverage summary

| Command / feature | Status |
|---|---|
| mDNS service discovery, PTR/SRV/TXT publication | Implemented |
| mDNS TXT consumption on discovery | Implemented |
| Invitation | Implemented, with retry and cancel |
| Invitation with (User) Authentication | **Not implemented** (§1.1) |
| Invitation Reply: Accepted | Implemented |
| Invitation Reply: Pending | Implemented (no action required) |
| Invitation Reply: (User) Authentication Required | Declines cleanly (§1.1) |
| Ping / Ping Reply | Implemented, with latency tracking |
| NAK | Sent and honored |
| Bye / Bye Reply | Implemented, including reason 0x05 |
| Session Reset / Reset Reply | Implemented |
| UMP Data | Implemented, MTU-aware, gap-tolerant |
| Retransmit Request / Error | Implemented both directions; timing refinement open (§1.3) |
| Forward Error Correction | Implemented, budget-aware |

---

*Items in §2 have been checked against M2-124-UM v1.0 and should not need re-verification unless
the implementation changes. §3 records decisions that look like gaps but are intentional.*
