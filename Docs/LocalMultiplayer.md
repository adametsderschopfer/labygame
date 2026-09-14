# Local multiplayer testing

Local play is an additional transport mode. EOS rooms and their codes remain
available through Create room / Join room. Local rooms use the same ECS room,
4-player limit, host-only start, central spawns and gameplay replication.
No Epic sign-in, EOS artifact or external service is required for local rooms.

## Apply the changes

Build `labyEditor Win64 Development` with the editor closed, then reopen it.
The runtime module now depends on Sockets to show the host's local address.

## Two windows on one PC

Run this command twice, in separate PowerShell terminals, after building:

```powershell
& 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe' 'C:/ue_prj/laby/laby.uproject' -game -windowed -ResX=1280 -ResY=900 -DDC=InstalledNoZenLocalFallback
```

These are independent standalone game processes using the project's editor build;
a packaged build is not required. The DDC option bypasses the unavailable local
Zen cache seen in this machine's logs.

1. In the first window, choose **Create local room**.
2. In the second window, choose **Join local room** and enter `127.0.0.1:7777`.
   If the host displays another port because 7777 was occupied, use that port.
3. Both players stay in the waiting room. The host presses **Start game**.
4. Repeat with more windows to reach the maximum of 4 players.
5. Use **Leave room** to return to the main menu. Closing the host disconnects guests.

The normal single-player PIE viewport can also host a local room, but its network
port may differ. Use the address/port displayed by the host. Do not use automatic
PIE client/server world creation to test the menu flow: use independent standalone
processes so each starts in the main menu.

## Two PCs on the same LAN

Run the same build on both PCs. On the guest, enter the host address displayed in
the room, for example `192.168.1.25:7777`. **Copy address** copies this endpoint.
`127.0.0.1` always means the guest's own PC, so do not use it across two PCs.
If the host has multiple network adapters or a VPN, use the IPv4 address of the
adapter shared with the guest (`ipconfig` lists these). Allow the game through
Windows Firewall on the LAN if Windows asks.

The local input accepts IPv4 or `localhost`, optionally followed by a port from
1 to 65535. Arbitrary travel URLs and extra URL options are not accepted.
Joining after the host starts is rejected, as in EOS mode. Local mode does not
provide internet matchmaking, relay, or NAT traversal.

## Implementation

The local host travels with `listen?Room=1?Local=1?bUseIPSockets?Port=7777`.
UE 5.8's EOS net driver explicitly selects IP passthrough for `bUseIPSockets`,
and for non-EOS connection addresses. The configured IpNetDriver fallback also
covers an unavailable EOS socket subsystem. EOS configuration is left intact.
Local connection state in GameInstance is transport metadata; authoritative room
membership, admission and game-start rules remain in Mass ECS.

Compilation can be checked without running Play. Actual multiplayer behavior must
be verified in the game; no automated Play sessions were started by the agent.
