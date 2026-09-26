# Procedural room surfaces

The flooded-room water and pool-tile surfaces are project-authored procedural
Unreal materials. They use no external textures, models or third-party source.

- Runtime packages: `/Game/Materials/Laboratory/M_PoolTile`,
  `/Game/Materials/Laboratory/MI_PoolTile`, `/Game/Materials/Laboratory/M_RoomWater`
  and `/Game/Materials/Laboratory/MI_RoomWater`.
- Rebuild: run `py "C:/ue_prj/laby/Scripts/create_room_materials.py"` in the
  Unreal Editor console while Play is stopped.
- Geometry is generated in centimeters by the native maze room pipeline. Pool
  tiles use 15 cm world-space ceramic squares with narrow grout and beveled
  normals. Water uses Unreal 5.8's native Single Layer Water shading model on
  one upward-facing quad, with depth-dependent absorption/scattering and small
  animated world-space ripples. It has no emissive tint, cube side faces, physical
  collision, displacement, swimming, underwater post-process or buoyancy.
  Material time pauses with gameplay. No experimental Water plugin is required.
- Target: the project's desktop deferred renderer. Reflection quality follows
  the existing Unreal scalability settings; low-end/mobile parity is not claimed.
  No new textures, Nanite meshes or LOD chains are needed for a two-triangle surface.
- Existing compatible package paths are retained to preserve references. The
  script overwrites generated room graphs; preserve manual changes in another
  asset before rebuilding. It also enables instancing on the existing ceiling
  and wall masters without rewriting their graphs or material-instance tuning.
