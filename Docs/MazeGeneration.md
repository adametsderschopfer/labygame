# Rooms and floor holes

Generation remains deterministic from the replicated maze seed. `FMazeLayout`
carves rooms and selects holes; `FMazeGenerationSystem` produces the immutable
topology, floor-instance transforms, spawn positions and exit positions.
The ECS bridge produces the transient collision surface and schedules visual
chunk generation through `FMazeChunkSystem`. `AMazeWorld` consumes these results
to create engine resources. See [Streaming.md](Streaming.md) for lifecycle,
memory ownership, thread scheduling and current resident-collision policy.

Narrow-passage generation is currently disabled. The retained generator can mark a soft
target of 5% of eligible straight corridor cells as full-height narrow passages.
Segments are 2–3 cells long, keep three cells of spacing, and exclude room cells,
the entrance, the exit and scenic runs. A straight corridor cell may retain a
side door into a room; the inserted wall is split around that opening and leads
to the canonical room doorway through an enclosed door-height tunnel. Metal
frames remain at its two accessible ends, while the hidden middle copy and
generic full-height trim on narrow-wall bends are suppressed. Segment cells and a one-cell approach halo are
protected from holes. The immutable axis mask drives resident collision, chunk
visuals and the physically narrow floor ribbon on both explored maps; it adds no
movement restriction, trigger actor or per-wall entity.

Default 80 x 80 maps target up to 147 base rooms and 75 additional dead-end rooms,
plus at most 64 single-cell holes. Rooms range from 1 x 1 to 3 x 5 cells and
keep a corridor gap between their rectangles. A separate two-cell, single-width
dead end at the west edge holds the spawn and opens directly into the maze without
a door. Placement attempts are bounded; fewer rooms or holes are accepted when
the constraints require it.

The spawn dead end, its approach halo and all boundary cells have solid floor.
Holes never touch, including diagonally. Every tentative hole is removed from
the walking graph, then a flood fill starts at the spawn cell. The hole is kept
only if all remaining floor cells are reachable. This invariant is checked after
each accepted hole, so a series of holes cannot jointly disconnect the level.
The protected exit cell is included, and no route requires jumping over a hole.

Floor collision follows the hole mask: contiguous solid row runs become cube
instances. Four exterior strips retain the landing outside the boundary without
covering any interior hole. There is no hidden monolithic floor beneath the gaps.
Rooms and holes use the same replicated seed on all peers.

The minimap marks holes with orange crosses. Falling 500 units below the maze
floor is fatal through the server ECS hazard system; existing death UI and player
replication display the result. Positions below the floor do not count as reaching
an exit.

`Laby.Maze.RoomsAndHolesRemainWalkable` checks connectivity independently, spawn
and exit safety, room interiors, and geometric floor coverage for multiple seeds
and sizes. The existing determinism check now also compares rooms and holes.
These automation tests were added/updated but not run; gameplay testing is left
to the user. Changed C++ translation units are checked by compilation only.

Stop Play before applying a new generation build, then start Play again to create
fresh immutable generation data. All multiplayer peers need the same build: a
seed alone cannot reconcile different versions of the generation algorithm.
