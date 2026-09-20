# Hazmat Suit 3

- Source and attribution: `Content/ThirdPartyNotices/HazmatSuit3.txt` (included in packaged builds by the existing staging setting).
- Original FBX and texture maps: `ArtSource/Characters/HazmatSuit3/Original/Hazmat_Suit`.
- Unreal mesh: `/Game/Characters/HazmatSuit3/SK_HazmatSuit3`; skeleton and 13 materials in the same asset folder.
- Reproducible import/material setup: `Scripts/import_hazmat_character.py`, using installed UE 5.8 FBX import APIs and the Material Editing Library. FBX uses the explicit legacy FbxFactory for predictable skeletal import; texture imports use native Interchange. No new plugin dependencies.
- `AMazeCharacter` uses its existing SkeletalMeshComponent in place of the three capsule-shaped placeholder meshes. The model is fitted to the existing capsule height, with the soles at its base, and rotated from imported +Y to character +X. Collision and movement remain owned by CharacterMovement; gameplay rules and state remain in ECS.
- The world mesh uses OwnerNoSee and hidden shadow casting; other players see the complete suit. An owner-only follower mesh shares its evaluated pose through LeaderPoseComponent. UE 5.8 first-person rendering uses FOV 85 and scale 0.5; the camera remains capsule-attached. Separate masked materials hide head equipment and the upper hood in the local view. The original world materials remain intact.
- Skeleton and skin weights are preserved. The source has 67 Blender bones plus Unreal's armature root. **No animation clips are present in the archive.** UE 5.8 template clips supply locomotion, retargeted through the saved Manny/Hazmat IK rigs and `RTG_Manny_Hazmat`. Hand-held items, head aim and a visible headlamp model remain separate future work.
- Existing CharacterMovement mesh-offset handling owns crouch/network smoothing. No second pose/crouch state or manual per-tick mesh displacement was introduced.
- Mesh and materials are referenced by the native character and saved as project assets. Components are owned and destroyed by the character. No Mass fragment/scheduling/authority changes.
- Applying the constructor replacement requires a rebuilt editor module and a fresh editor session. The editor was closed before the build; no Play or automated tests were started.

Verification: Development Editor build succeeded. Import commandlet completed with zero errors; original FBX has no smoothing-group metadata (UE generated normals). Gameplay and animation playback were not tested.

Editor inspection: 68 Unreal bones including the armature root; 55,086 triangles; 34,602 render vertices; all 13 slots reference saved custom materials (no default fallback). Character CDO references the mesh at uniform scale 0.941705 and Z -89.556153 cm. Reference foot/toe positions confirm imported forward +Y. Materials were visually inspected in the skeletal mesh editor after fixing Unreal Array struct-copy assignment. No animation or gameplay verification was performed by the agent.

## Locomotion and first-person body

`UMazeCharacterAnimInstance` is a presentation adapter with a native animation
proxy. Its game-thread PreUpdate snapshots CharacterMovement velocity, falling
and crouched observations. Engine animation nodes blend 8-direction walk/jog,
idle, jump, falling and landing; no gameplay decision is made by the graph.
CharacterMovement continues to move the capsule, including networking.

`BS_HazmatLocomotion` has direction -180..180 degrees (positive = right) and
gait 0..2. Backward samples are duplicated at both ends. Playback speed uses
the measured original clip displacement rates, approximately 311 cm/s for
walking and 621 cm/s for jogging, adjusted for character scale. Existing ECS
walk/sprint speeds are unchanged. The native TwoBoneIK solver bends the knees
while lowering the pelvis during the 0.55-second crouch transition. Crouch
shortens forward/backward stride to 55%; the same directional clips supply
crouch walking and arm swing. This is a procedural crouch, not a separate
motion-capture set.

The imported FBX stores a 100x scale on H_Armature. UE retarget export drops
that scale while writing pelvis offsets in centimeters. The idempotent
`Scripts/prepare_hazmat_animation.py:normalize_clips` restores root scale and
converts pelvis keys back to root-local units. For the constant-speed cyclic
walk/jog clips it removes linear planar travel between first and last keys.
Root motion is disabled, avoiding double capsule/mesh movement. Metadata
`HazmatNormalized=1` prevents applying the conversion twice; a fresh retarget
export must clear that tag before normalization.

The same script's `first_person_materials` rebuilds the local materials using
saved textures. UE's LocalPosition (InstancePreSkinning) is passed through a
vertex interpolator before masking suit vertices above reference Z=145 cm.
Gloves, shirt and boots retain original materials. All local head-equipment
slots use a fully masked material. Other views retain the complete character.

Animation clips and copied Manny source assets come from the installed UE 5.8
Third Person template; they are Epic engine content, separate from the Hazmat
CC BY attribution. Source files are kept under `Content/Characters/Mannequins`
for reauthoring; the native character references only the Hazmat runtime assets.

Applying this addition requires a full editor build and a new editor session:
a reflected animation class, a default mesh component and AnimGraphRuntime /
AnimationCore dependencies were added. Current verification: asset saves,
material shader compilation and sampled retargeted poses; source formatting.
The new C++ module has not yet been compiled. Play and automated tests have
not been run; final animation appearance needs the user's gameplay review.

## Mask view and development camera

The first-person camera is 12 cm forward of the capsule center, at the existing
eye height. Its camera-scoped `M_MaskVisor` post-process runs after tonemapping:
subtle cool glass tint, peripheral gasket, faint sheen and scratches, clear
center. It has no effect on UI or on the separate third-person camera. It is
authored by `prepare_hazmat_animation.py:mask_visor` and compiled in the editor.

The local PlayerCameraManager limits pitch to 55 degrees below and 65 degrees above the
horizon (`ViewPitchMin=-55`, `ViewPitchMax=65`). This clamps ControlRotation through the engine's
normal view update, so camera and aiming remain aligned. The limit applies to
both camera modes; crouch eye-height movement is unchanged.

F6 toggles a development-only third-person camera, next to the F7 minimap key.
F8 is intentionally unused because the editor reserves it for ejecting from PIE.
The native SpringArm uses a 240 cm boom, a 12 cm collision probe and a small
shoulder offset. The full world mesh becomes visible locally; the follower
mesh and mask post-process are absent from this view. Switching back restores
first-person presentation. The mode is a local presentation flag, resets on
unpossession/new pawn and is neither saved nor replicated. The key, toggle
code and camera components are excluded from Shipping and Test builds.
