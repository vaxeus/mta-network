# MTA:SA Open-Source Network Module (ENet)

An open-source, drop-in replacement for the closed-source `net.dll` (server) and `netc.dll` (client) of Multi Theft Auto: San Andreas, built on [ENet](https://github.com/lsalzman/enet) 1.3.18.

The module implements the complete `CNetServer` / `CNet` SDK interfaces, so **no changes to the MTA mod or core sources are required** — you build it against a stock `mtasa-blue` tree and drop the files in.

## What's included

| Path | Contents |
|------|----------|
| `Server/net/` | Server module → `net.dll` |
| `Client/net/` | Client module → `netc.dll` |
| `Shared/net/` | Shared bitstream, HTTP download manager (libcurl), channel mapping |
| `vendor/enet/` | Vendored ENet 1.3.18 |

## Features

- Full ENet transport: reliable / unreliable / unreliable-sequenced delivery, mapped onto separate ENet channels per ordering domain (sync, chat, voice, ...) so unrelated streams never stall each other
- Complete `NetBitStreamInterface` implementation, including compressed integers, normalized vectors/quaternions, and per-peer bitstream version negotiation (version-gated packet fields parse correctly across builds)
- HTTP download manager built on libcurl (resource downloads, `fetchRemote`/`callRemote`, master server announcements, port tester)
- Packet statistics, bandwidth statistics, network simulation options (packet loss / extra ping), server id keys (`server-id.keys`), connect password, player limits
- Server Lua script loading (pass-through `DeobfuscateScript` for plain-text and precompiled scripts)

## Differences from the official module

- **No anti-cheat.** The AC/SD subsystem lives in the official closed-source module and is not reimplemented. AC info fields are sent empty for wire compatibility.
- **Client serials.** Client serials are generated per install instead of derived from hardware.
- **No obfuscated scripts.** Scripts protected with luac.multitheftauto.com encryption (`0x1C` format) are not supported; plain-text and standard precompiled Lua work.
- **Protocol incompatible with official builds.** Server and client must BOTH run this module. An official client cannot join a server running this `net.dll`, and vice versa.


## Changelog

See [CHANGELOG.md](CHANGELOG.md) for the full version history.

## Notes

- This is an unofficial version, not affiliated with MTA, but rather a different, open-source version that can be modified and improved.

## Community

Questions, bug reports, and contributions are welcome — join the **Vortex** Discord: [discord.gg/rWMsVgQbZ](https://discord.gg/rWMsVgQbZ) (developer: **vaxeus**). See [CONTRIBUTING.md](CONTRIBUTING.md) if you want to help improve the module.

## License

GPLv3, same as mtasa-blue. ENet is MIT-licensed. Not affiliated with or endorsed by the Multi Theft Auto team.

Developed by **vaxeus** — [Vortex Discord](https://discord.gg/rWMsVgQbZ)
