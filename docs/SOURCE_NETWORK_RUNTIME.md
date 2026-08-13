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
  `NetService::Connect`. The non-dismissable `svHintPleaseWait` remains until
  the replicated owner `NetPlayer` arrives or a source failure callback ends
  the operation.
- `MainMenu::OnConnectionFailed` and its pre-match disconnect branch now hide
  that loading message and display `svHintHostConnectionFailed`.
  `Menu::OnDisconnectedPlayer` distinguishes loss of the host during a match,
  pauses the race and displays `svHintDisconnect`; `Menu::OnFailed` uses
  `svCriticalNetError`. Confirming either in-match failure executes the
  original `MyDisconnectEvent` transition through `ExitRace`/`ExitMatch`.
- Leaving browser/IP closes the connection; leaving `NetworkFrame` finalizes
  NetLib, matching the Windows menu ownership boundary.

Later source follow-ups preserve this transport boundary and now instantiate
the original `NetRace`/`NetPlayer` model class IDs 1/2. Match/player state,
ready/start/countdown/finish, vehicle state and gameplay RPC feed the active
race. `DialogMenu2::UserChat` sends the original UTF-16 `PushLine`, resolves
the sender by owner ID/gamer ID and renders in the race menu/HUD; no separate
replacement multiplayer protocol is used.

`OptionsMenu::GameFrame` now uses the source host boundary as well. Its eight
NetRace-owned rows publish the original `SetUpgradeMaxLevel`,
`SetWeaponMaxLevel`, `SetCurrentDifficulty`, `SetLapsCount`, `SetMaxPlayers`,
`SetMaxComputers`, `SetSpringBorders`, and `SetEnableMineBug` RPCs. They are
disabled on a client, and incoming values update the active profile and race.
`ChangePlanet` also publishes the source planet/track/weather indices; a
client resolves them through the original track catalog before reloading the
world.

`RaceMenu2::RaceMainFrame` now uses the original five network player images
and two-column source layout. Remote human cards render their gamer photo,
name, car, Host/Ready label and ready-state icon. A client Start click sends
`NetPlayer::RaceReady` and locks the other six menu icons while ready. A host
requires at least one ready opponent, shows the source warning otherwise,
uses `NetGame::DisconnectPlayer` for the kick button, and preserves the
`NetRace::GetLeaverList` confirmation before a repeated race.

`NetPlayer::OnSetGamerId` and `OnSetColor` now retain their two distinct
source events and the serialized `failed` bit. The host rejects a duplicate
gamer or color, discards the attempted broadcast and directs the previous
authoritative value back to the remote owner. The local-host gamer-conflict
branch emits its failure in place, exactly as the Windows code does, rather
than recursively addressing the server itself.

The active `GamersFrame` also restores the source asynchronous boundary: a
network selection displays `svHintPleaseWait`, remains in the frame until the
validated gamer event arrives, enters Garage only on success and displays
`svHintSetGamerFailed` on refusal. A refused Garage color restores the
host-returned color and displays `svHintSetColorFailed`.

New human `NetPlayer` models now run the original `GenerateGamerId` and
`GenerateColor` policy before their first state replication. Gamer IDs are
tested in the order loaded from `tournamet.xml`; colors are tested in the
exact left-then-right order of the fourteen `Player.cpp` palette entries.
Both generators skip values already owned by another human model. The active
Gamers and Garage frames apply the matching `CheckGamerId`/`CheckColor`
visibility rules, preserving empty palette positions instead of moving the
source widgets. `NetPlayer::SetGamerId` also sends its RPC when the generated
ID already equals the chosen ID, because the Windows frame waits for that
authoritative event before advancing.

`NetPlayer::~NetPlayer` is connected to the live race rather than only to
the replicated model list. When the host loses or kicks a remote owner, the
corresponding stable portable racer slot is marked disconnected, its Jolt
vehicle body and car-owned audio/effects are removed, and it no longer
participates in AI, contacts, places, finish completion or result RPCs. This
is the indexed-backend equivalent of Windows `Player::FreeCar(true)` followed
by `Race::DelPlayer`. `PlayerStateFrame::RemoveOpponent` and
`MiniMapFrame::DelPlayer` are preserved by removing its name/life overlay and
map marker immediately; an ordinarily destroyed car retains the original
temporary death/respawn behavior.

`NetRace::ExitMatch` is no longer represented by two cleared booleans. Its
original `DoExitMatch` ownership boundary copies the live player-model list,
deletes every class-ID-2 model locally through NetLib and then publishes the
reliable `OnExitMatch` RPC. Both peers consume `MatchExited`, clear the active
race/HUD/audio state and return to `MainMenu`; this orderly path is kept
separate from host-loss and critical-error dialogs. The source HUD split is
also preserved: a client confirming Exit Race performs local `ExitRace` plus
`ExitMatch`, while a host publishes `ExitRace` with the current result list
and returns to `RaceMenu2`. Selecting Exit there publishes `ExitMatch` before
NetLib finalization.

## Verification

The non-rendering regressions `rrr3d_original_network_session_smoke`,
`rrr3d_original_network_models_smoke` and
`rrr3d_original_user_chat_smoke` verify
initialization, adapter enumeration, LAN search/cancel, source-port hosting,
asynchronous connection refusal classification, error reset,
close/finalization, all eight host-option RPCs and their client gate, model
RPC, remote and local-host gamer/color conflicts with authoritative rollback,
generated identity selection, same-value gamer confirmation, host-side peer
removal, disconnected-racer input/respawn exclusion, Cyrillic chat wire
format, `ExitMatch` player-model cleanup on both peers, restart after cleanup,
and the source chat history/fade model. The
Metal regressions exercise all five source LAN menu frames, the asynchronous
loading/failure dialog lifecycle and the chat overlay in a live race:

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
