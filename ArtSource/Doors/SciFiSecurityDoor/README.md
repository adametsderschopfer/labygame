# Sci-Fi Security Door

## Provenance

- Visual references were supplied by the project owner on 2026-09-25.
- Original creator, source URL and license were not supplied. Treat the reference
  images as internal visual direction only; do not redistribute them.
- The model, textures and renders are project-authored procedural derivatives
  generated locally with Blender 5.0 by `Scripts/build_scifi_security_door.py`.
- A Meshy image-to-3D submission was attempted but rejected before task creation
  because the connected free plan does not permit API generation. No Meshy output
  or paid/generated asset is included here.

## Layout

- `Original/`: the supplied open and closed reference images.
- `Prepared/SciFiSecurityDoor.blend`: editable source scene.
- `Prepared/SciFiSecurityDoor.fbx`: Unreal-oriented interchange model.
- `Prepared/SciFiSecurityDoor.glb`: compact preview/interchange model.
- `Prepared/Textures/`: shared metal PBR maps.
- `Prepared/Previews/`: closed and open rendered previews.
- `Scripts/build_scifi_security_door.py`: deterministic rebuild script.

## Rebuild

Run from any shell:

```powershell
& 'C:\Program Files\Blender Foundation\Blender 5.0\blender.exe' --background --python `
  'ArtSource\Doors\SciFiSecurityDoor\Scripts\build_scifi_security_door.py'
```

The script deliberately replaces the generated files under `Prepared/`. Preserve
manual edits in a separate source file or update the generator first.

## Unreal import contract

- Intended runtime size is 4.4 m wide, 3.4 m high and 0.42 m deep.
- Coordinates are authored Z-up. The FBX export uses `-Y` forward and `Z` up; use
  the normal Unreal FBX axis conversion so the imported meshes end at scale 1.
- The assembly contains separate frame geometry and left/right leaf pivots. Each
  leaf moves only along local X by 1.42 m to reach the authored open state.
- The metal surface uses shared base-color, roughness, metallic and tangent-space
  normal maps. Green indicators use an emissive material.
- Gameplay state, access rules and motion timing remain owned by ECS. The meshes
  and pivots are presentation resources only.
- Import into `/Game/Doors/SciFiSecurityDoor` through Unreal-aware tooling, create
  deliberate simple collision and register the production soft paths in the
  relevant `UMazeLocationSettings` manifest before production use.

## Known limitations

- The rear face is project-authored because the references show only the front.
- Typography is an approximation; the procedural source uses the available
  default Blender font.
- The model has been visually checked through the generated Blender renders, but
  it has not yet been imported into Unreal or tested in gameplay.
