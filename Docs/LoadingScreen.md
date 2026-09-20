# Loading screen

`UMazeOnlineGameInstance` shows MoviePlayer during blocking LoadMap (topology and
resident collision included), then hands off to a viewport overlay while
`UMazeLocationSubsystem` prepares asynchronous dependencies and visual chunks.
PIE uses the viewport overlay throughout. Both use the shared
`MazeInterfaceStyle::MakeLoadingScreen`; MoviePlayer content has no UObject
bindings. The minimum movie display time is 0.5 seconds; no invented percentage
is displayed.

PostLoadMap no longer means resource readiness. The initial gate waits for
registered participants, dependencies, requested PSOs, several stable frames and
a render fence. Procedural clients wait for the replicated maze seed and their
initial visual region. Authored World Partition definitions also query native
streaming completion. Missing current chunks can reopen the overlay after a
teleport. Local Character input availability is observed by ECS; this never
replaces server session permissions.

Resource preparation runs while paused; gameplay simulation keeps its existing
pause behavior. Missing dependencies or invalid settings display a preparation
error rather than releasing the player into incomplete content. Technical paths
are logged, not shown in the UI. Network/travel failure and Shutdown clear the
screen; delegates are unbound and GameInstance retains no world entity handles.

Readiness covers requested work, not every future shader variant, texture mip or
driver operation. See [Streaming.md](Streaming.md) for ownership, budgets,
lifetime, future location registration and verification limits.

The new reflected classes and collision component type require a full build
with the editor closed before reopening. Compiling the separate game target does
not update the running editor. Play and gameplay tests remain opt-in.
