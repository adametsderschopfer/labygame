# Rooms and floor holes

Generation remains deterministic from the replicated maze seed. `FMazeLayout`
carves rooms and selects holes; `FMazeGenerationSystem` produces the immutable
topology, floor-instance transforms, spawn positions and exit positions.
The ECS bridge produces the transient collision surface and schedules visual
chunk generation through `FMazeChunkSystem`. `AMazeWorld` consumes these results
to create engine resources. See [Streaming.md](Streaming.md) for lifecycle,
memory ownership, thread scheduling and current resident-collision policy.

Default 80 x 80 maps target 18 rooms (including the 3 x 3 spawn room) and at most
64 single-cell holes. Random rooms range from 3 x 3 to 7 x 7 cells, stay inside
the boundary and keep a corridor gap between their rectangles. They remove only
internal walls and preserve the existing maze connections. Placement attempts
are bounded; fewer rooms or holes are accepted when the constraints require it.

A 5 x 5 area around the spawn cell and all boundary cells have solid floor.
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
