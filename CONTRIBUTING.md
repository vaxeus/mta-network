# Contributing to the MTA:SA Open-Source Network Module

Thanks for your interest in improving the module! This project is an open-source ENet-based replacement for MTA:SA's closed-source `net.dll` / `netc.dll`, and contributions are welcome.

## Ground rules

- **Net-module scope only.** All changes must stay inside `Server/net/`, `Client/net/`, `Shared/net/`, and `vendor/enet/`. Do NOT modify MTA mod or core sources (`Client/core`, `Client/mods`, `Server/mods`, SDK headers) — the whole point of this project is that it drops into a stock `mtasa-blue` tree without touching anything else.
- **Wire compatibility is a contract.** The open-source mod layer defines what every packet looks like. Before changing anything in the bitstream or packet paths, compare the client read side (`Client/mods/deathmatch/logic/CPacketHandler.cpp`) with the server write side (`Server/mods/deathmatch/logic/packets/`) — several packets contain fields the official net.dll injects itself
- **Threading matters.** The server module's `DoPulse` runs on the `CNetServerBuffer` service thread, while the mod layer calls many interface methods from the main thread. Anything shared between the two (download managers, peer records, statistics) must be safe for that split. Never pulse the HTTP download managers from the network thread.
- Keep the code clean and comment-free, matching the existing style.

## Building

1. Clone [mtasa-blue](https://github.com/multitheftauto/mtasa-blue) (or use this repository if it ships the full tree)
2. Copy/overwrite `Server/net/`, `Client/net/`, `Shared/net/`, `vendor/enet/` into it
3. Run `win-create-projects.bat` and open `Build/MTASA.sln`
4. Build the **`Network`** project → `net.dll` (server) and the **`Client Network`** project → `netc.dll` (client)

## Submitting changes

1. Fork the repository and create a branch (`fix/short-description`)
2. Make your changes, keeping commits focused (one fix or feature per PR)
3. Describe in the PR: what was broken, the root cause, and how you verified the fix (the test steps above)
4. For crash fixes, include the crash dump analysis or reproduction steps

## Reporting bugs

Open a GitHub issue with:

- Server or client (or both) and the module version
- What happened vs. what you expected
- Reproduction steps, `logs/network.log` output, and any crash dump (`dumps/` folder)
- Whether stock MTA resources are enough to reproduce it

## Community

Join the **Vortex** Discord to discuss development: [discord.gg/rWMsVgQbZ](https://discord.gg/rWMsVgQbZ) — developer: **vaxeus**.

## License

By contributing, you agree that your contributions are licensed under GPLv3, the same license as the project.
