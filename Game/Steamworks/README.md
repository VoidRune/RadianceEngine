# Steamworks setup

The game runs as Spacewar (app 480, Valve's test app) until it has its own App ID. Steam must be running; `--no-steam` turns the integration off for a run.

## Switching to the real App ID

1. Put the new App ID in `Game/steam_appid.txt`. The build copies it next to the exe. Don't ship that file in a depot.
2. In the Steamworks app admin, create the achievements and stats below and upload `RichPresence.vdf` under Community > Rich Presence. Then publish.
3. For release builds, call `SteamAPI_RestartAppIfNecessary(appId)` before `SteamAPI_InitEx` in `SteamPlatform`, so that starting the exe directly relaunches it through Steam. Don't add it while the App ID is 480.

## Achievements

API names must match `Achievements` in `GameShared/src/GameShared/Content/Achievements.h`.

| API name | Name | Description | Notes |
|---|---|---|---|
| FIRST_FLING | First Fling | Knock a rival into the lava | |
| HAT_TRICK | Hat Trick | Knock out three rivals in one round | |
| BOWLING_NIGHT | Bowling Night | Fling three rivals with one spell | |
| RETURN_TO_SENDER | Return to Sender | Knock out a rival with their own reflected spell | |
| HOOKED | Hooked | Pull a rival into the lava with Chain Hook | |
| SWAP_MEET | Swap Meet | Swap a rival into the lava you were standing in | |
| PACIFIST | Pacifist | Win a round without casting a spell | |
| BY_A_THREAD | By a Thread | Win a round with less than 10 health | |
| COMEBACK_MAGE | Comeback Mage | Win a match after losing the first two rounds | |
| DECK_BUILDER | Deck Builder | Stack two upgrades on one spell | |
| SCHOLAR | Scholar | Win a round with every spell | Progress stat `scholar_spells`, 0 to 13 |
| SKYDIVER | Skydiver | Fall off Emberhold 25 times | Progress stat `falls`, 0 to 25 |
| STARGAZER | Stargazer | Light every constellation above the observatory | |
| FAITHFUL | Faithful | Reach the cat's islet | Hidden |

## Stats

| API name | Type | Min | Max | Set by |
|---|---|---|---|---|
| falls | INT | 0 | 25 | Client, only increases |
| scholar_spells | INT | 0 | 13 | Client, only increases |

## Networking

Friends join through Steam Datagram Relay with `ISteamNetworkingSockets` P2P connections, so no ports need to be opened and no addresses are shared. Direct IP joins still use GameNetworkingSockets.
