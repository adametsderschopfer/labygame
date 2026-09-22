# Location preparation and streaming

## Ownership

The maze entity owns the immutable topology, floor transforms, ceiling transform,
seed and generation revision. Gameplay facts remain in focused Mass fragments.
`UMazeECSSubsystem` is the only gameplay bridge. `FMazeChunkSystem` derives visual
chunk payloads; it has no UObject, Actor, online or renderer dependencies.

`UMazeLocationSubsystem` is a world-scoped **engine resource adapter**. Its
readiness is an observation of asset loading, registered resource participants,
World Partition and PSO preparation; it is not a session permission, gameplay
timer or replacement for ECS. `UMazeAssetManager` only extends the engine's cook
hook, using the same resource manifest. Neither class owns gameplay rules.

`AMazeWorld` owns a resident collision shell and disposable `AMazeChunkView`
Actors. A view has no gameplay state, collision, tick or replication. Appearance
is reconstructed from the current ECS snapshot. Destroying a view must never
remove an item, reset a cooldown, respawn loot or change AI decisions.

## Current implementation and budgets

Rooms with doorways range from 1 by 1 cells to the existing maximum of 4 by 7
(the entrance remains 3 by 3). Small, medium and large ranges have weights of
50%, 35% and 15%; this is not a per-map count guarantee. Floor and ceiling heights
are unchanged. Base distribution attempts three rooms per spatial sector
with a target side of 12 cells: the 80-cell map targets up to 147 base rooms plus
the entrance. Crowded slots are skipped after trying the smallest footprint.
Corridor margins separate all sectors and rooms within each sector;
45% of base random rooms have two doorways on adjacent walls, the rest have one.
The entrance retains its single doorway. The added doorframes use the existing wall
mesh and resident collision shell; no additional Actors, components, assets or
worker jobs are allocated per room. Counts are higher but the footprint, sightline
bound and chunk budgets are unchanged; memory/frame cost has not been measured.
A room can straddle chunk boundaries, but its
interior remains within the default two-chunk preload radius from any point
inside it. Enclosed perimeters and narrow, non-aligned doorways limit new room
sightlines; arbitrary corridor sightlines retain the limitations below.
Door jambs and lintels belong to the wall strip's center tile, so each visual
chunk emits its own portion once, with the same full-layout occupancy used by
the halo and resident collision. No new asset, resource participant, map entry,
simulation state or near/far rule is introduced. Existing creation/eviction
budgets, readiness gating and resident physics remain unchanged. Runtime visual
and performance checks require a user-run session; no FPS improvement is claimed.

An additional pass chooses up to 75 compact one-cell rooms on the default map:
two-door rooms on corridor bends or one-door rooms on dead-end branches adjacent
to bends. It preserves existing cell connections and excludes scenic corridors.
The additions use existing room/door geometry, at most 150 additional doorframes,
without new assets or view actors. Smaller footprints and adjacent door directions
do not extend the prior sightline bound; preload radii, budgets, readiness and
resident physics stay unchanged. Runtime cost is not measured. Map highlighting
only changes the tint of already drawn, explored floor polygons on both maps.

Door openings have explicit U-shaped satin-metal trim on both wall faces, using
the same opening dimensions as the collision shell. Generic ceiling-height corner
strips are suppressed at all four doorway corners. Each frame belongs to the
wall strip's center cell, so chunk borders neither duplicate it nor omit one face.
The six thin boxes per door reuse the existing metal section (72 triangles), stay
on the solid side of the opening and stop at its header. No asset, Actor, collision,
gameplay state, sightline or streaming-radius change is needed. Geometry is derived
from the immutable layout and follows existing chunk/revision cleanup.

Route shaping targets ordinary straight runs of at most six cells, while preserving
connectivity when no safe bend is available. The scenic quota counts these residual
long runs first, then may add 8–10-cell runs (two targeted on the default map).
An intentionally added ten-cell run is 46.25 m at the current cell size, within
the minimum 16-cell axial coverage of the default two-chunk preload radius.
This is a sizing review, not a measured readiness guarantee: constrained residual
runs can exceed that target and retain the arbitrary-sightline limitations below.
Room dimensions, traversal speed, creation/eviction limits and hysteresis remain
unchanged. Resident collision still covers the entire map.

Dead-end cabinet, shelf and tally-mark decoration has been removed at the user's
request. No standalone assets, actors or gameplay state were created for these
details. Existing chunk meshes keep their old appearance until rebuilt; newly
built chunks no longer contain them. Ordinary interior trim and fixtures retain
their existing resource/lifecycle rules.

`[/Script/laby.MazeLocationSettings]` in `Config/DefaultGame.ini` defines:

| Setting | Default | Meaning |
| --- | --- | --- |
| ChunkCells | 8 | Side of a visual chunk in maze cells |
| LoadRadius | 2 | Chebyshev radius around each local player's chunk |
| UnloadRadius | 3 | Larger retention radius to prevent boundary thrashing |
| MaxRetainedChunks | 64 | Soft retention limit; required chunks are never evicted to meet it |
| CommitBudgetMs | 3 | Diagnostic target for one indivisible engine commit, not a hard time limit |

The default 80-cell maze has 100 possible chunks. A single interior player needs
25 preload chunks, with additional chunks retained temporarily by hysteresis.
The scheduler permits one worker, one completed view creation and one eviction
per maze per frame. It prioritizes near chunks. A single component registration
or geometry upload can still exceed the target; `Maze_StreamViews` and the
verbose over-budget message make this visible. No hard frame-time guarantee or
measured FPS improvement is claimed.

Only rendering is distance-streamed. **Collision for the entire maze remains
resident**, including the floor-hole mask and exterior landing. This deliberately
keeps remote players, unobserved physics and future non-player objects safe.
The collision wall component creates no render proxy. This is not a complete
bounded-memory solution for arbitrarily large worlds; collision/topology growth
must be budgeted separately before increasing map size substantially.

The generated payload no longer retains the full wall vertex/normal/index
arrays. Collision construction uses a transient surface; visual workers build
only their region and a one-cell context halo for trim joins. CPU worker buffers
are discarded after copying into engine components. Destroyed Actors/components
release their resources through Unreal's normal render cleanup and GC, not
necessarily in the exact eviction frame. Material/fixture assets remain pinned
by the location's load handle until world teardown; texture mips use native
texture streaming.

Keep the preload radius large enough for the longest visible sightline and
maximum supported traversal speed. Radius-based streaming cannot guarantee that
arbitrary long corridors, teleports or new large rooms never expose the edge of
the resident view region. Teleporting into an absent current chunk raises the
preparation overlay again; it does not change server authority or physics.

## Execution and lifetime

1. The world resource subsystem validates settings and requests the registered
   dependency list through `UAssetManager::GetStreamableManager`.
2. GameMode creates the maze. ECS generates topology synchronously, then its
   typed bridge produces the resident collision surface. Collision cooking is
   synchronous so PlayerStart and server physics never depend on an unfinished
   asynchronous body. This initial work is still covered by the loading movie.
3. The world Actor registers as a preparation participant, including on clients
   waiting for a replicated seed. After assets finish loading, it creates shared
   material instances and the existing bounded lamp-audio pool.
4. The Actor's `TG_PrePhysics` tick requests `RequestMazeChunk`. The subsystem
   copies generation parameters and an immutable **thread-safe** shared payload
   into a thread-pool task. The worker runs `FMazeChunkSystem::Build` without
   accessing UObjects or Mass. Gameplay systems remain synchronous; no automatic
   Mass processor is introduced.
5. `TakeMazeChunk` runs on the game thread, validates the world-owned entity and
   revision, and returns immutable data. The adapter additionally rejects chunks
   no longer wanted, creates engine resources and discards the worker payload.
6. First entry waits for the complete preload region. Subsequent ordinary
   movement prepares neighbors in advance; a missing current chunk gates local
   input again. Resource preparation continues while paused; gameplay delta,
   material time and ECS pause semantics are unchanged.
7. Regeneration destroys views, drops pending-job handles, resets readiness and
   replaces materials. EndPlay also unregisters readiness and destroys the maze
   entity. A discarded worker may finish owned CPU work, but has no callback or
   UObject reference with which to publish into a destroyed/replaced world.

Each client streams its own local views. A listen server also streams only its
local views; its collision shell still covers all players. Dedicated servers
skip visual assets, visual chunk work and audio. Split-screen views are collected
for streaming, but this does not make the rest of the game's session/UI logic
split-screen capable.

Cosmetic fixture randomness is keyed by seed and position, so chunk request
order and unloading cannot reroll it. Existing decoration placement changes
from the older sequential stream; topology and gameplay generation are unchanged.

## PSO, shaders and graphics

The project explicitly enables native PSO precaching and texture streaming.
`UMazeVisualMeshComponent` collects material/`FLocalVertexFactory` PSOs for the
procedural mesh and follows the engine's delayed-proxy policy. Static mesh
components request their native precache paths after assigning materials.

Initial readiness waits for `FShaderPipelineCache::NumPrecompilesRemaining`,
stable completion over multiple frames and a render command fence. These cover
requested work, not every possible future material, graphics setting or driver
operation. They do not promise complete ray-tracing PSO coverage or that all
texture mips are resident. Global PSOs requested later do not independently
pause established gameplay. Re-entered absent chunks reopen the resource gate.

`Config/DefaultScalability.ini` extends standard engine GI/reflection presets.
The existing graphics menu already uses `UGameUserSettings`; the world no longer
forces GI/reflection quality overrides over those presets. Texture pool sizes
remain platform/scalability controlled and limited to VRAM. Do not hard-code one
desktop's pool size or increase PSO worker counts without measurements.

## Adding a location

Add one `+Locations=(Map="/Game/Maps/Name.Name",Mode=...,Assets=(...))` entry to
the settings section. Use a full soft object path for the map and each required
resource. The manifest is read by both runtime preparation and the native cook
hook, so registered maps/dependencies are added to cooking automatically.
Asset dependencies referenced by those packages are handled by Unreal's cooker.

- `Procedural`: an adapter must register using `ReportReady(this, false)` before
  starting asynchronous work, report success only after resources exist, and
  unregister on teardown. The current maze adapter supplies this path. Register
  required assets before loading them in the adapter; do not hide first-use
  synchronous loading behind an unlisted `LoadObject` call.
- `Whole`: Unreal loads the authored level normally; the common resource/PSO gate
  covers its manifest. Additional asynchronous features register participants.
- `WorldPartition`: use an actually partitioned, streaming-enabled authored map
  and appropriate engine streaming sources. Readiness also queries the native
  partition subsystem. A mismatched manifest fails visibly. The current
  procedural maze map is **not converted** to World Partition.

Unregistered maps retain Whole behavior; this is compatibility, not validation
of a new feature's dependencies. Every production location must be registered.
A resource manifest does not supply a new location's game mode, spawn rules,
traversal logic or ECS gameplay definitions; those remain feature work.

Run `powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/Validate-Locations.ps1`
after changing a manifest. This checks package paths on disk without opening
Unreal. Runtime validation also rejects invalid budgets, duplicate maps and
unresolved loaded dependencies. Engine cook rejects exclusions of required
packages through logged errors. Packaging/cook validation still needs to be run
when a packaged build is requested.

## Adding a feature

Before implementation, specify ECS owner, server authority, stable identity,
reset/persistence rules, resource dependencies and near/far simulation policy.
Use one source of gameplay truth. Representation creation consumes snapshots;
its destruction releases engine resources only. Do not implement item pickup,
AI dormancy or damage inside a chunk Actor. A new simulation policy needs its
own focused ECS system; this patch does not invent gameplay for absent features.

Declare resource readiness through the world adapter API, including failure
through `Fail`, and pair every registration/load with cleanup. Add dependencies
to the location manifest; cook and initial preload then follow automatically.
Future large feature-specific bundles should use native Asset Manager bundles,
with the same readiness contract, instead of pinning every asset of every level.

## Loading screen observations

The loading screen names map loading, ECS topology generation, resident collision
preparation, resource loading, local visual chunk preparation, World Partition
streaming, PSO preparation, and the first-frame fence wait. AMazeWorld reports the
boundaries of synchronous calls through the resource subsystem; ECS still owns
gameplay generation and collision data.

`ReadPreparationStatus` returns detached observations: native
`FStreamableHandle::GetLoadedCount` for resources, required/resident chunk counts
reported by the adapter, and the existing
`FShaderPipelineCache::NumPrecompilesRemaining` readiness counter. In UE 5.8 this
includes active PSO precache requests, not just a bundled pipeline cache. Shader
work overlapping resource/geometry work appears as a separate remaining count.
The progress bar measures only the current counted stage; unknown totals use an
indeterminate bar. There is no estimated overall percentage or ETA.

The existing native Slate/MoviePlayer path is retained because blocking map loads
cannot safely tick a Widget Blueprint. A mutex protects copied presentation
values read by the loading thread; no widget binding queries UObjects, fragments,
or the world. The viewport receives a separate widget at handover, preserving
elapsed wall time without assigning a widget to two Slate parents. Travel and
teardown reset presentation; generation and participant cleanup reset counts.
Gameplay authority, readiness conditions, scheduling and budgets are unchanged.
Startup before GameInstance/PreLoadMap remains outside this screen's coverage.

The first-launch/driver-change hint explains possible graphics preparation delays
without diagnosing every slow launch as shader compilation. Shipping compilation
and packaging check integration; appearance and cold-start duration still require
a user-run check. Play, automation tests and performance captures are not run.

## Diagnostics and verification

`MazeDiagnosticsToolset.ReadSnapshots` enriches detached ECS snapshots with
resource observations: readiness, failure, pending PSOs, resident/pending chunks
and estimated source geometry bytes. `bHasResourceDiagnostics` distinguishes
this from the pure runtime ECS reader. No runtime ECS code depends on world
Actors to gather these values. The byte estimate is not measured VRAM or total
process memory and excludes resident physics, engine allocations and textures.

Game-target compilation, formatting and static manifest validation are performed
for this change. Play, automation tests, Insights captures, packaged cook and
editor-target build are not run. The editor is left open and unmodified. Applying
the new reflected classes, collision component type and Asset Manager class
requires a full editor build with the editor closed, then reopening the project.

When gameplay checks are explicitly requested, compare cold/warm startup,
chunk-boundary traversal, long sightlines, teleport, regeneration during pending
work, pause, repeated travel, missing assets and two players far apart. Check
memory after GC and frame-time spikes as well as averages. Use PSO validation
to find Missed/TooLate cases; only add a collected bundled cache when evidence
shows gaps. Do not launch these checks implicitly during feature development.
