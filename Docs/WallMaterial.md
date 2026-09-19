# Procedural ceramic walls

`/Game/Materials/Laboratory/MI_MazeWallCeramic` is the active wall material instance. Edit its **Subway
Tiles** parameters to change the appearance without changing code or rebuilding
the parent graph. The parent is `/Game/Materials/Laboratory/M_MazeWallCeramic`.

Defaults: warm white 30 x 15 cm tile modules, alternating half-tile offset,
2.6 mm graphite grout, 2.4 mm bevel, 1.2 mm shading relief and a soft 0.7 mm
convex crown. Module dimensions include the grout. Colors are linear RGB;
roughness is 0.19 for glaze and 0.85 for grout. Relief changes lighting normals,
not geometry, collision or silhouette; it does not create parallax occlusion.
`CrownHeightCm` controls the broad tile bulge, `GlazeWavinessCm` the subtle
glaze irregularity and `GroutGrainCm` the fine cement grain. These have separate
filtered spatial scales and roughness variation. Recesses receive mild AO,
while grout stays dark gray rather than unlit black.

The material uses world-space planar projection selected by the geometric face
normal. This suits the generated, static, axis-aligned wall faces, including
their top caps, and requires neither UVs nor tangents. Rows align in world Z;
individual tiles can be cut at corners. This is not UV unwrapping for arbitrary
curved, rotating or moving meshes: moving a wall slides it through the pattern.

The custom shader integrates tile coverage over the pixel footprint separately
for both alternating rows to filter thin grout lines. Small per-tile color
variation and analytically derived bevel/crown normals fade out when tiles
become subpixel; finer glaze and grout detail fade by their own frequencies.
No texture downloads, generated bitmap maps or tessellation are used.

`AMazeWorld::BeginPlay` assigns the instance on rendering worlds. This is a
presentation resource owned by the wall component, with no ECS state or new
gameplay rules. The existing `/Game/Materials` packaging directory includes it
in cooked builds. Live Coding changes apply when the next maze actor starts.

To rebuild the active coordinated set, execute `Scripts/create_laboratory_materials.py`
through Unreal Editor Python or the Python commandlet. Rebuilding preserves existing
instance parameter overrides. The standalone `create_subway_material.py` still
targets the original `/Game/Materials` folder; the laboratory builder passes its
own destination to it. Original assets remain available but are no longer assigned.

The laboratory set also contains `MI_LabVinylSatin` (muted gray-green, fine filtered
pigment flecks and satin roughness) and `MI_LabCeilingMineral` (square white cassettes,
2.4 cm rails and cool-white diffusers in a centered pattern).
All dimensions, colors and light emission are editable instance parameters.
The ceiling's relief is shading only; its luminous panels feed the project's
existing Lumen GI/reflections. They are not separate direct-light components.
Actual indirect illumination and reflections depend on Lumen's surface coverage
and must be assessed in-game, especially on the large generated ceiling slab.
Fixed exposure is EV100 0.5 for this interior. No gameplay state, entity ownership,
collision, topology or generated floor holes are changed.

### Satin vinyl variation

`Scripts/create_vinyl_material.py` authors the active `M_LabVinylSatin` /
`MI_LabVinylSatin` pair. The original `MI_LabVinyl` remains available for comparison.
The green base color is unchanged. Smooth, rotated noise at two physical scales
replaces the original regularly spaced dots. Pigment has no height. Analytic
normal gradients add only 0.01 mm microrelief and 0.02 mm broader waviness;
low-contrast polishing variation mostly affects roughness rather than albedo.
Fine detail fades by pixel footprint. No new geometry, textures, collision or
gameplay state is introduced. Existing Unreal material normal/roughness inputs
and the project's Substrate conversion remain in use; a separate coating/layer
stack is unnecessary for this single dielectric surface. Final appearance and
shader cost at gameplay resolution have not been measured in Play.

### Cell-aligned square ceiling

`Scripts/create_ceiling_material.py` rebuilds only the ceiling. Shared authoring
helpers live in `Scripts/material_builder.py`; the full laboratory builder calls
the ceiling script too. The old rectangular `MI_LabCeiling` is no longer assigned.

`AMazeWorld::Build` creates a component-owned dynamic instance on rendering worlds
and passes `CellSizeCm`, `WallThicknessCm`, `MazeOrigin` and two exact 16-bit halves
of the ECS seed. These are presentation mirrors, refreshed whenever generation
publishes new data. There is no cached entity handle or fragment pointer in the
material. The actor's reflected layout and component types are unchanged.

The grid covers the whole bay pitch continuously, including open connections,
with no neutral wall-width bands. Four squares of 115.625 cm cover 462.5 cm; the runtime MID explicitly supplies TargetPanelSizeCm=120 to also update previously saved ceiling assets.
Passive tiles terminate at actual wall faces (ordinary perimeter cuts); every
light diffuser must fit wholly inside the clear bay span, so walls cannot cut it.
There is one fixture per bay instead of nine, with matte porous acoustic infill
and thin gray rails. This is a material layout, not individual ceiling geometry.

Spatial hashes of tile ID and seed assign 18% of fixtures off, 12% intermittently
dimming and the rest steady. Off fixtures retain a pale diffuser. Each flickering
fixture has an independent phase, a smooth 1.5-second dip every roughly 9.6–15.6
seconds, and a 25–65% dip depth. Material GameTime respects world pause. Phase
is local cosmetic time; fixture selection follows seed. No gameplay state or
additional direct-light components are introduced.

Emissive strength was reduced from 6 to 2 and clamped to 4; tint is nearly neutral.
The local post-process sets Lumen Final Gather and Reflection Quality to 2 and
chromatic aberration to zero. These are mitigations for suspected emissive noise /
temporal artifacts, not a verified root-cause fix: the supplied images show real
ceiling references, not the in-game artifact. Lumen still provides panel illumination
and may retain noise/temporal lag on the single large ceiling slab. Higher quality
has a GPU cost that has not been profiled. Confirm appearance/performance in Play.
See Epic's emissive-material documentation for bright-source noise limitations:
https://dev.epicgames.com/documentation/unreal-engine/using-the-emissive-material-input-in-unreal-engine
Example command (editor asset authoring, not gameplay):

```powershell
& 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' 'C:/ue_prj/laby/laby.uproject' -run=pythonscript -script='C:/ue_prj/laby/Scripts/create_laboratory_materials.py' -AllowCommandletRendering -unattended -nosplash -NoSound -ddc=InstalledNoZenLocalFallback
```

Final visual approval should check both wall orientations, corners, grazing
angles, distant lines and the glaze under the game's actual lighting.


### Flush wall courses at the ceiling

The active wall asset is built by `Scripts/create_wall_material.py`, which invokes
 the shared ceramic builder with the name `MazeWallCeramic`. `AMazeWorld::Build`
assigns a component-owned MID and sets `TileOrigin` to `(Origin.X, Origin.Y,
Origin.Z + WallHeight)` from the current ECS snapshot. Rows now run downward from
the ceiling junction: the former 5 cm remainder of 320 / 15 cm no longer appears
as a narrow top course. TileHeightCm is now fitted to WallHeight / round(WallHeight / 15): at 320 cm this gives 21 complete courses of 15.238 cm, removing the partial bottom course too. This does not move
wall/ceiling geometry or collision. Both MIDs refresh with the generated payload;
BeginPlay no longer replaces the wall MID with an unaligned static instance.

Exposure was raised by half a stop (EV100 1 to 0.5, approximately 1.414x exposure)
without increasing emissive strength. The larger diffusers also increase emitting
area; final visual balance must be checked in the user's Play session.

Ceiling infill now samples TextureCan Tiles 0013 CC0 color, DirectX normal and roughness maps from ArtSource/Ceiling/TextureCan0013. The 16x16 atlas is sampled inside each cell, preserving the existing procedural rails, lights and layout. No baked AO texture is multiplied onto the ceiling; contact-shadow behavior at the wall/floor has not been visually verified. See the source README for licensing.
