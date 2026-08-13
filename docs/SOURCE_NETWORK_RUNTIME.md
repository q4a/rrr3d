# Source LAN runtime follow-up

This stage connects the already ported original `NetLib` TCP/UDP transport to
the active Apple Silicon game executable. It is an integration of source
behavior, not a replacement multiplayer mechanic.

## Ported source behavior

- `OriginalNetworkSession` owns `net::GetNetService()` with the lifecycle from
  `NetGame::Initializate`, `Process`, `Close`, and `Finalizate`.
- The source port is unchanged: TCP host and UDP discovery use `58213`.
- The source `syncRate` is `70` ms. Debug LAN discovery uses `500/250` ms and
  Release uses `3000/500` ms, matching `NetGame::PingHosts`.
- `NetworkFrame` displays up to six real IPv4 adapter addresses.
- `ServerTypeFrame` exposes the original local-server branch.
- `ClientTypeFrame` exposes LAN broadcast discovery and manual IPv4 entry.
- `NetBrowserFrame` shows real `endpointList()` results and connects through
  `NetService::Connect`; connection/refusal/disconnect callbacks update the
  source hint state.
- Leaving browser/IP closes the connection; leaving `NetworkFrame` finalizes
  NetLib, matching the Windows menu ownership boundary.

Later source follow-ups preserve this transport boundary and now instantiate
the original `NetRace`/`NetPlayer` model class IDs 1/2. Match/player state,
ready/start/countdown/finish, vehicle state and gameplay RPC feed the active
race. `DialogMenu2::UserChat` sends the original UTF-16 `PushLine`, resolves
the sender by owner ID/gamer ID and renders in the race menu/HUD; no separate
replacement multiplayer protocol is used.

## Verification

The non-rendering regressions `rrr3d_original_network_session_smoke`,
`rrr3d_original_network_models_smoke` and
`rrr3d_original_user_chat_smoke` verify
initialization, adapter enumeration, LAN search/cancel, source-port hosting,
close/finalization, model RPC, Cyrillic chat wire format and the source chat
history/fade model. The Metal regressions exercise all five source LAN menu
frames and the chat overlay in a live race:

```bash
cmake --preset macos-arm64-m9
cmake --build --preset macos-arm64-m9 --parallel 8
ctest --test-dir build/macos-arm64-m9 --output-on-failure
build/macos-arm64-m9/Debug/RRR3d \
  --data-dir=resources/game-data --network-menu-smoke-test
```

The final Debug and Release bundle presets now enable `RRR3D_ENABLE_NETWORK`.
Boost remains statically/header-only linked; its license is included in the
application bundle.
