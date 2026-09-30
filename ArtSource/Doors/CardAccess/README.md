# Card access presentation

Original project-authored procedural presentation, 2026-09-30. No downloaded or
cloud-generated content and no third-party attribution. Uses Unreal's built-in
100 cm cube mesh under the engine license and existing project ivory/metal finishes.

The editable material source is `Scripts/create_card_access_material.py`.
Run it with Unreal Python for this project, with Play stopped, or through the
PythonScript commandlet. Rerunning replaces only the generated screen graph at
`/Game/Doors/CardAccess/Materials/M_CardReaderScreen`.

The unlit opaque screen blends red/green emissive colors using ISM custom data 0
(locked/unlocked ECS snapshot), passed through a native VertexInterpolator.
One shared material serves every screen; there is no light or dynamic material
per reader. The material is included in the Maze location's preload/cook manifest.

Geometry uses centimeters, Z up. Card body: 8.6 x 5.4 x 0.4 cm, center pivot,
with a thin metal stripe. It has Visibility query collision while available;
no physics or navigation. The selected card has a camera-attached local view,
with no collision or shadow. Readers are 18 x 6 x 28 cm, center pivot, on both
faces of the wall beside the latch edge, at 125 cm height. Screens are 13 x 16 cm.
Reader housings use grouped ISM with Visibility query collision; screens use
grouped ISM without collision. These simple cubes do not need Nanite or custom
LODs, UVs, tangents or imported meshes. They use built-in cube normals/UVs.

Gameplay identity, pickup availability and persistent unlock state belong to
Mass ECS. Representations remain resident and are reset at generation/teardown,
independent of cosmetic chunk eviction. Dedicated servers retain ECS and door
physics, create no reader instances, and keep card query geometry hidden.
Windows is the current target. Gameplay/render inspection is opt-in; the source
and native material graph can be checked without starting Play.
