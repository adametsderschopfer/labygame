# Procedural room surfaces

The flooded-room water and pool-tile surfaces are project-authored procedural
Unreal materials. They use no external textures, models or third-party source.

- Runtime packages: `/Game/Materials/Laboratory/M_PoolTile`,
  `/Game/Materials/Laboratory/MI_PoolTile`, `/Game/Materials/Laboratory/M_RoomWater`
  and `/Game/Materials/Laboratory/MI_RoomWater`.
- Rebuild: run `py "C:/ue_prj/laby/Scripts/create_room_materials.py"` in the
  Unreal Editor console while Play is stopped.
- Geometry is generated in centimeters by the native maze room pipeline. Pool
  tiles use world-space coordinates; water uses a world-space animated normal
  and translucent surface. The packages are safe to rebuild over their generated
  graphs and contain no manual edits.
