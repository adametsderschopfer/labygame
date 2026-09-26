# Meshy Secure Access Door

## Provenance

- The current source is the project owner's updated-texture Meshy delivery from
  2026-09-25: `Meshy_AI_Secure_Access_Door_0925014413_texture_fbx.zip`.
- The preceding delivery, `Meshy_AI_Secure_Access_Door_0925012127_texture_fbx.zip`,
  is retained under `Original/` for provenance; it is no longer the prepared
  texture source.
- The Meshy task ID, source prompt/reference, account plan, credits consumed,
  public source URL and separate license terms were not included in the delivery.
  Treat it as project-owner supplied source art and do not redistribute it until
  those terms are recorded.
- The original download is preserved unchanged under `Original/`.
- The prepared meshes are derived locally in Blender 5.0 by
  `Scripts/build_meshy_secure_access_door.py`.

## Prepared asset

- `SM_MeshySecureDoor_Frame.fbx`: the static frame and mounted details.
- `SM_MeshySecureDoor_LeafLeft.fbx`: left sliding leaf with the shared door origin.
- `SM_MeshySecureDoor_LeafRight.fbx`: right sliding leaf with the shared door origin.
- `Textures/`: the supplied base-color, metallic, normal and roughness maps with stable names.
- `Previews/`: closed and open Blender renders used for visual verification.
- `MeshySecureAccessDoor.blend`: editable prepared source.

The prepared model is 1.80 m wide, 2.50 m high and 0.58 m deep. Each leaf moves
0.62 m away from the center to reach the authored open pose. UVs and the supplied
texture atlas are preserved; no generated replacement texture is used.
The current delivery contains valid PNG source maps. The Blender pass still
normalizes every prepared map into a stable, verified PNG container so the
same rebuild path also remains compatible with the earlier delivery.

Canonical Unreal import inputs are the three prepared FBX files and the four
prepared texture maps. Their destination is `/Game/MeshySecureAccessDoor`.
The FBX files are emitted with centimeter vertex coordinates and import at scale
`1.0`. Import them as three separate static meshes without generated materials or
textures, generate lightmap UVs, enable Nanite, and keep collision disabled on the
visual components. Import BaseColor as sRGB/Default, Normal as linear/Normalmap,
and Metallic/Roughness as linear/Masks. `M_MeshySecureDoor` connects those four
maps directly to the matching PBR inputs; `MI_MeshySecureDoor` is assigned to all
three meshes. `M_MeshySecureDoorStatus` is the green emissive ISM material used by
the separate top and reader indicators.
The Blender rebuild replaces prepared FBX files, textures, previews and the
editable `.blend`; do not make manual edits inside `Prepared/` without first
updating the rebuild script.

## Rebuild

```powershell
& 'C:\Program Files\Blender Foundation\Blender 5.0\blender.exe' --background --python `
  'ArtSource\MeshySecureAccessDoor\Scripts\build_meshy_secure_access_door.py'
```

## Runtime contract

Gameplay state and proximity decisions belong to the Mass ECS door fragment and
system. The frame and leaves are presentation resources. Runtime collision uses
simple invisible blockers instead of the dense source mesh.

The normal map is treated as tangent-space by the delivery naming and visual
inspection; Meshy metadata confirming its channel convention was not supplied.
The source is dense, so Unreal uses Nanite for the visual meshes while collision
remains separate and simple.
