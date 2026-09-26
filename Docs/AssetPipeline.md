# 3D asset and content pipeline

This document defines the target contract for new and touched visual content in
LABY. It covers source files, Unreal packages, prefab-like composition, runtime
representation, loading and quality gates. It does not require a bulk migration
of legacy content. When a feature is changed, improve the content it owns without
moving unrelated packages.

## Ownership model

Keep three concerns separate:

1. **Asset**: mesh, skeleton, animation, material, texture, sound or VFX package.
2. **Representation**: the Unreal component, instance set, Blueprint or level
   assembly that displays and operates engine resources.
3. **Gameplay**: authoritative identity, state, rules, persistence and decisions.

Assets and representations do not own gameplay facts. Mass fragments and focused
systems remain authoritative. An Unreal adapter reads detached ECS snapshots,
creates or updates representations and submits observations or requests through
`UMazeECSSubsystem`. Removing a visual representation must not delete a door,
item, creature, access rule or other persistent gameplay fact.

Mass fragments must not contain `UObject`, Actor, component or mutable asset
references. Use stable value identifiers such as a focused enum, tag, definition
ID or other serializable value. Resolve that identifier to presentation resources
at the adapter boundary.

## Repository layout

Organize both source art and runtime content by feature so a feature can be
understood, licensed, migrated and removed as a unit.

```text
ArtSource/<Feature>/
  README.md                 provenance, license, modifications and import recipe
  Original/                 unchanged vendor or artist delivery when retained
  Prepared/                 cleaned export used by Unreal
  Scripts/                  feature-local conversion helpers, when justified

Content/<Feature>/
  Meshes/                   SM_*, SK_*
  Materials/                M_*, MI_*, MF_*
  Textures/                 T_*
  Animations/               A_*, AM_*, ABP_*
  Blueprints/               BP_*
  Data/                     DA_*, DT_*
```

Create only the subfolders a feature needs. A compact feature may keep a few
closely related packages directly below its feature root. Shared content belongs
under a deliberate shared feature only after two real consumers need it; do not
create a generic dumping ground in advance.

Existing top-level folders such as `/Game/Materials`, `/Game/Textures`,
`/Game/Doors` and `/Game/Fixtures` remain valid until their owning feature is
deliberately migrated. Their current shape is not the template for new features.
Do not reorganize them merely for visual consistency.

Third-party deliveries stay isolated in `ArtSource`. If vendor packages must be
imported unchanged, isolate them under `/Game/ThirdParty/<Vendor>/<Pack>` and put
project-authored derivatives or adapters under the owning feature. Production
packages must never reference `/Game/Developers/...`.

Do not commit DCC autosaves, derived caches, preview renders with no review value,
OS metadata such as `__MACOSX`, or redundant vendor archives. Keep an original
archive only when it is needed for provenance/recovery and repository storage
policy permits it. Introducing or migrating Git LFS is a repository-wide decision;
do not silently change storage filters as part of one asset import.

## Naming and package stability

Use Unreal-style prefixes consistently:

| Asset | Prefix |
| --- | --- |
| Static mesh | `SM_` |
| Skeletal mesh | `SK_` |
| Skeleton | `SKEL_` |
| Physics asset | `PHYS_` |
| Texture | `T_` |
| Material | `M_` |
| Material instance | `MI_` |
| Material function | `MF_` |
| Blueprint Actor | `BP_` |
| Animation Blueprint | `ABP_` |
| Animation sequence | `A_` |
| Animation montage | `AM_` |
| Data Asset | `DA_` |
| Niagara system | `NS_` |
| Sound asset | `S_` |

Use descriptive role names, not workflow history: avoid `_New`, `_Final`,
`_Final2` and dates in production package names. Variants should describe a real
semantic or visual difference.

Preserve an existing object path when a replacement is genuinely compatible.
Scale, pivot, sockets, skeleton, material-slot order, collision and bounds are
part of that compatibility contract. When one changes incompatibly, introduce a
temporary migration asset, update and verify consumers, then remove or rename it
through the editor. Do not leave permanent version suffixes after migration.

Rename and move `.uasset` or `.umap` packages only through the Content Browser,
an Unreal Python/editor API or an equivalent Unreal-aware tool. Save affected
packages, update soft paths and manifests, fix redirectors and verify references.
Filesystem moves can silently break package references and are prohibited.

## Source and import contract

Every externally sourced feature needs an `ArtSource/<Feature>/README.md` that
records:

- creator/vendor and original URL or delivery source;
- license and required attribution or redistribution conditions;
- download/delivery date and version when relevant;
- what was modified and which files are the canonical import inputs;
- the Unreal destination path and a repeatable import/rebuild procedure;
- any known limitation, such as uncertain normal convention or missing source;
- whether generated packages may be safely rebuilt over manual edits.

Before import, establish the following explicitly:

- Unreal centimeters, positive X forward and positive Z up at the runtime
  boundary; apply DCC conversion deliberately rather than compensating with
  arbitrary Actor scale;
- a meaningful pivot: base/contact pivot for placed props, hinge pivot for
  rotating parts and a stable assembly origin for modules;
- frozen transforms and expected default scale of `1,1,1` in Unreal;
- intentional hard/soft edges, normals, tangents, UV channels and texture color
  spaces;
- the smallest practical material-slot count and reuse of project master
  materials through material instances;
- simple collision, complex collision or no collision based on actual gameplay
  need, not importer defaults;
- LOD chain or Nanite choice based on usage, deformation, target platform,
  instance count and measured cost;
- skeleton, sockets, root motion and animation retargeting contract for skeletal
  content.

Use a skeletal mesh only when deformation or skeletal animation is needed.
Independent rigid moving parts can remain static meshes driven by transforms,
as the current door leaves are. Nanite is a rendering choice; it does not create
gameplay collision, navigation, replication or a streaming policy.

Prefer a reproducible Unreal Python/editor script for procedural, generated or
bulk-imported families. A script must use explicit source and destination paths,
set material/collision/import properties deliberately, report failures and be
safe to rerun or clearly warn that it replaces packages. Generated Unreal assets
remain tracked outputs; scripts do not replace visual inspection.

### Meshy-generated source assets

Meshy is the approved default cloud generator for new 3D source assets when a
task benefits from image-to-3D, text-to-3D, texturing, remeshing, rigging or
animation. Follow `Docs/Meshy.md`. Meshy is not an alternative runtime asset
system and does not bypass this document:

- Store references, downloaded models, texture maps, rendered previews and a
  provenance/readme record under the owning `ArtSource/<Feature>/` folder.
- Record the Meshy resource type and task ID, generation date, account plan or
  license basis, input description or reference provenance, actual credits
  consumed and any follow-up processing tasks. Never record credentials or
  signed download URLs.
- Treat a generated model as untrusted source art until scale, axes, pivots,
  topology, UVs, PBR channel meanings, material slots, collision and separated
  moving parts have been checked. A thumbnail establishes appearance only.
- Prefer one approved source generation and local deterministic preparation.
  Reuse the existing Meshy task for LODs, conversion, retexture or rigging; do
  not regenerate merely to obtain another format or a lower polygon count.
- For interactive assemblies such as doors, request or prepare separate rigid
  parts with deliberate pivots. Meshy geometry remains presentation; Mass ECS
  continues to own access rules, state transitions and authoritative motion.
- Import prepared files through Unreal-aware editor tooling into the owning
  `Content/<Feature>/` path. Generated files must never be written directly as
  `.uasset` packages or loaded synchronously from `ArtSource` at runtime.

## Choosing a prefab-like representation

Unreal has several prefab-like tools. Choose by runtime responsibility rather
than making every composition a Blueprint.

| Need | Preferred representation |
| --- | --- |
| One unique static visual | Static mesh component or authored map placement |
| Repeated identical visual | `UInstancedStaticMeshComponent` grouped by compatible state |
| Large non-Nanite instance set where hierarchical culling measures better | HISM after profiling |
| Predominantly static reusable multi-mesh assembly supported by the packer | Packed Level Actor |
| Authored module retaining multiple Actors or unsupported packed components | Level Instance |
| Assembly requiring lights, audio, VFX, sockets or another engine-owned resource | Thin Actor/Blueprint adapter |
| Interactive or persistent world object | Mass entity plus one of the above representations |
| Many dynamic entities with representation LOD needs | Evaluate native Mass Representation explicitly |

Packed Level Actors are for supported predominantly static assemblies. Unsupported
component types can produce incomplete packed output; use a regular Level Instance
or focused adapter instead. A Level Instance is not automatically part of the
procedural maze's custom chunk lifecycle. Integrating one requires an explicit
loading, ownership and teardown design.

Blueprints may compose components, expose presentation parameters and forward
engine events. They must not own authoritative access, health, inventory, damage,
pickup, cooldown, AI, room or progress rules. Avoid Blueprint Tick when an event,
ECS snapshot update or batched component update is sufficient.

Do not create one Actor per repeated socket, detector, trim piece, clutter item
or similar decoration. Batch compatible objects by mesh, material set, mobility
and collision policy. Prefer per-instance custom data over one dynamic material
instance per prop when the material can consume it. Use HISM only when its
hierarchy improves a measured non-Nanite case; ISM is the default for the current
procedural presentation.

Mass Representation is a valid native option for sufficiently large dynamic
entity populations, but adopting it would introduce processor scheduling and a
new representation lifecycle. It requires an explicit design compatible with
the project's synchronous subsystem scheduling; it is not a default replacement
for the existing world adapters.

PCG may assist editor-authored cosmetic dressing when it has a clear authoring
benefit. It must not become a second authority for deterministic maze topology,
gameplay spawns or persistent identity. Do not enable it solely to replace a
small pure placement function.

## Asset references and loading

Do not scatter hard-coded `/Game/...` paths throughout runtime `.cpp` files. Put
typed `TSoftObjectPtr`, `TSoftClassPtr` or other focused references in a settings
object or asset definition owned by the presentation feature. For a small fixed
set, a focused settings type is enough. Introduce a Data Asset/catalog when it
provides real authoring value, and native Asset Manager bundles when variants or
feature bundles justify them.

The resource contract remains `UMazeLocationSettings` and
`UMazeLocationSubsystem`:

1. Register every production map and required runtime dependency.
2. Start asynchronous work only after registering a readiness participant.
3. Resolve presentation references after the required resources are ready.
4. Report success or failure and unregister during teardown.
5. Keep handles alive for the intended residency period and reject stale
   world/entity/revision results.

A soft reference does not load its target. Loading a Data Asset does not
automatically preload every soft reference stored inside it. List the required
targets in the location manifest or load an explicitly integrated Asset Manager
bundle before use. Never conceal first-use synchronous disk access behind an
unregistered `LoadObject` call.

Run `Scripts/Validate-Locations.ps1` after changing the location manifest. The
manifest and Asset Manager cook hook provide package coverage; a separate
`DirectoriesToAlwaysCook` entry is not the preferred registration mechanism for
new feature assets.

## Procedural placement, collision and lifetime

Cosmetic placement must be derived from the authoritative seed, stable position
or another explicit deterministic input. Chunk request order, asynchronous
completion and unload/reload must not change a chosen variant or transform.

Classify every placed model:

- **pure cosmetic**: no collision, navigation, interaction or persistent state;
  its representation may follow visual chunk lifetime;
- **physical environment**: affects movement, traces or navigation; its collision
  policy must be safe for every server-side player and physics user;
- **gameplay object**: has stable Mass identity and explicit authority,
  persistence, reset and near/far simulation rules.

The current maze distance-streams visual chunks while retaining world collision.
New colliding props must preserve that safety model or introduce a separately
reviewed server-wide collision residency design. Never unload collision merely
because the local client cannot see the model. Dedicated servers should avoid
cosmetic meshes and materials but must retain the state and collision they need.

Pair component creation, instance buffers, load handles, delegates and readiness
participants with cleanup on regeneration, `EndPlay` and world teardown. Validate
world, entity and generation revision before publishing asynchronous results.

## Quality and performance gates

There is no universal triangle, texture or memory budget for every asset. Choose
budgets from screen size, repetition, target platform and measurement. Before an
asset becomes a production dependency, check:

- silhouette, shading, normals/tangents and visible texture seams;
- real size, orientation, pivot and placement at scale `1,1,1`;
- material-slot count, texture resolution, compression and color-space settings;
- LOD transitions or Nanite fallback behavior on supported target platforms;
- simple/complex collision accuracy and collision profile;
- bounds, occlusion/culling behavior and repeated-instance compatibility;
- skeletal hierarchy, physics asset, sockets, animation compression and
  retargeting when applicable;
- first-load behavior, PSO precache path and absence of hidden synchronous loads;
- location manifest/bundle coverage and packaged-cook inclusion;
- cleanup, regeneration, multiplayer authority and dedicated-server behavior.

Do not promise performance improvements from Nanite, HISM, merging or texture
changes without a relevant measurement. Avoid premature one-off optimization,
but do not ship obvious per-prop Actors, duplicated master materials or accidental
high-cost collision when the native batched alternative fits.

## Change workflow

For a new or replaced 3D asset:

1. Identify the owning feature, ECS identity if any, representation type,
   collision authority, lifetime and location dependencies.
2. Add or update source/provenance under `ArtSource/<Feature>`.
3. Prepare the canonical export with the scale, axes, pivot, slots, UVs and
   collision contract defined above.
4. Import or generate through Unreal-aware tooling into the feature-owned
   `Content` path; never write a fake or hand-edited `.uasset`.
5. Wire the asset through typed presentation references and the location
   readiness contract.
6. Inspect the saved package and references in the editor. For batch scripts,
   verify representative assets and every reported failure.
7. Run static manifest validation when applicable. Do not launch Play, gameplay
   tests or rendering captures unless the user requested them.
8. Commit source metadata/scripts and runtime packages that form one reproducible
   change together. Do not include caches, autosaves or unrelated binary saves.

Player-visible visual improvements may update `CHANGELOG.txt`. Pipeline rules,
folder cleanup, import scripts and internal asset metadata alone do not belong in
the player-facing changelog.
