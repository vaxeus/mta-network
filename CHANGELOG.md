# Changelog

All notable changes to the MTA:SA Open-Source Network Module (ENet) are documented in this file.

## [1.1.0] — 2026-07-15

### Fixed

- **Protocol error (60) on join** — sub-byte bitstream reads/writes were left-aligned while the SDK bitstream helpers expect right-aligned partial bytes; every `ReadBit(bool&)` and compressed-integer read desynced the stream. Partial-byte bits are now right-aligned in `WriteBits`/`ReadBits`.
- **Protocol error (9) when a second player joined** — the client expects two AC/SD info strings at the head of `PACKET_ID_PLAYER_LIST` (injected by the official closed-source net.dll, never written by the open-source mod layer). The server module now injects them, so player-list packets parse correctly and multiple players can play together.
- **Server crash on `openports`** — download managers were pulsed from the network thread while the main thread pulsed the same curl multi handles (`CGame::DoPulse` / `CRemoteCalls`), a data race inside libcurl. The network thread no longer touches the download managers.
- **"Port testing service unavailable (0: )" and wrong `fetchRemote` results** — HTTP results now carry the HTTP status code on success (callers expect `200`/`2xx`) and the curl error code on transport failure; `GetError()` now returns a real error message.
- **All server resources failing with "is invalid. Please re-compile"** — added the `DeobfuscateScript` / `GetScriptInfo` overrides (pass-through for plain-text and precompiled scripts).

## [1.0.0] — initial release

- ENet 1.3.18 transport replacing RakNet for both `net.dll` (server) and `netc.dll` (client)
- Full `CNetServer` / `CNet` SDK interface implementation — drop-in, no mod/core source changes required
- Complete `NetBitStreamInterface` implementation with per-peer bitstream version negotiation
- Delivery classes mapped to dedicated ENet channels per ordering domain (sync, chat, voice, ...)
- HTTP download manager built on libcurl (resource downloads, `fetchRemote`/`callRemote`, announcements)
- Packet/bandwidth statistics, network simulation options, server id keys, connect password, player limits
- Client serial handshake (random per-install serial carried on `PACKET_ID_PLAYER_JOINDATA`)