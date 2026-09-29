# Frosted panel door

- `Original/DoorReference.png` is the project owner's visual reference, supplied on 2026-09-29. Creator and external license were not supplied; keep it within this project.
- `Original/TripoDoorSource.glb` is the single Tripo standard text-to-model result requested on 2026-09-29. Blender MCP Premium request ID: `b6e36c1b-e082-472d-85e5-5f3216d9d7f4`. It used one standard generation (24 remained). The source mesh has about 1.38 million triangles, combines the frame and leaf, and is retained only as source art. It is not imported into the game.
- `Scripts/build_frosted_door.py` builds the separate frame, leaf and handle meshes in Blender 5.0. `Prepared/FrostedPanelDoor.blend` is the editable scene; the three FBX files and wire-glass texture are the current Unreal import inputs. Running the script replaces those prepared files.
- Runtime candidates are under `/Game/Doors/FrostedPanelDoor`. The mesh dimensions are normalized to 100 cm and are scaled by the existing door presentation adapter. The resident collision box and gameplay state remain separate.
- The Unreal meshes and visual code have been authored, but compilation and in-game appearance have not been verified. Play and gameplay tests were not run.
