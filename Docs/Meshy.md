# Meshy 3D generation

Meshy is LABY's approved default cloud service for AI-assisted 3D source-art
generation. Agents should actively consider it when a task asks for a model,
texture, topology pass, rig or animation. It complements Blender and Unreal
authoring; it does not replace the project asset pipeline, ECS ownership or
Unreal's import and resource-readiness contracts.

## Supported integration

The supported route is the user-level Codex plugin
`meshy-openai-plugin@openai-curated-remote`. The verified installation on
2026-09-25 was plugin 0.6.0 with Meshy CLI 0.4.0 on Node.js 24.19.0. These are
machine-level dependencies and are not committed to the repository.

Read the installed `meshy-3d-generation` or `meshy-3d-printing` skill before a
task. Use a global `meshy` command only when its version matches the skill's
pinned version. Otherwise run the pinned temporary package:

```powershell
& 'C:\Program Files\nodejs\npm.cmd' exec --yes --package=meshy-cli@0.4.0 -- meshy <arguments>
```

Do not replace the CLI with custom HTTP calls. The CLI owns authentication,
idempotent task submission, polling, downloads, account profiles and typed error
handling.

## Authentication and read-only availability checks

Use browser OAuth and reuse the stored profile. Never ask the user to paste an
API key into chat, print credentials, read the credential file or copy it into
the repository. The default credential store is outside the project at
`~/.config/meshy/credentials.json`.

When login is required, start the device flow in a persistent shell session:

```powershell
meshy auth login --device --format json --no-update-check
```

Show the returned Meshy URL and one-time user code in progress commentary, let
the user approve in the browser, keep waiting on the same process, then verify
the session. Nothing is pasted back into chat.

The following checks are read-only and spend no credits:

```powershell
meshy auth status --format json --no-update-check
meshy doctor --check-api --output-schema v1 --format json --no-update-check
meshy balance --output-schema v1 --format json --no-update-check
meshy image-to-3d list --page-size 1 --output-schema v1 --format json --no-update-check
```

Use these to report separately:

- whether the credential is authenticated and verified;
- whether the production API is reachable;
- the current API balance;
- whether a read endpoint succeeds.

Do not submit a generation merely to test access. A successful balance/read
check does not guarantee that the current plan is entitled to create every task.
If the server rejects a paid create, preserve the error and stop; do not retry or
reinterpret a visible web-app balance as API entitlement.

## Planning and credit approval

Every paid operation needs an explicit plan before submission. For text-to-3D or
image-to-3D chains, use the CLI planner; it makes no API request and spends no
credits:

```powershell
meshy make "<prompt-or-image-path>" --dry-run --output-schema v1 --format json --no-update-check
```

For other operations, use the current Meshy price source referenced by the
installed skill. Before submission:

1. State the input, model/mode, stages, output files and workspace.
2. Itemize the estimated credits per paid stage and the estimated total.
3. Query the API balance.
4. Obtain explicit user approval for that amount and scope.

Approval covers the stated pipeline once. A second variant, rerun, retexture,
remesh, conversion, rig or animation that was not in the approved plan requires
a new estimate and approval. Never silently spend credits to improve a preview.

## Task lifecycle

Use the exact resource command selected by the installed skill. Submit once with
`create --async`, retain the returned resource and task ID, initialize the local
Meshy project record, and wait on that same task until terminal. A timeout or
unknown response does not authorize another submission. Reconcile and resume the
existing task.

After completion:

1. Download only the required deliverables and a rendered thumbnail when one is
   available.
2. Inspect the thumbnail for subject, completeness and obvious material errors.
   It does not prove topology, UV, rear geometry, scale or collision quality.
3. Report the resource/task IDs, local project folder, actual
   `consumed_credits`, warnings and final file paths.
4. Preserve task lineage. Later requests such as FBX conversion, LOD creation or
   retexturing must continue from the existing task rather than regenerate.

## Choosing inputs and topology

- Use image-to-3D for a specific visual reference and text-to-3D for a new design.
- Multi-image generation requires consistent views of the same object in the
  same state. Do not combine open and closed versions of a door, different poses
  or contradictory variants as if they were camera views.
- For real-time assets, prefer the game-oriented topology mode and an explicit
  polygon budget when it preserves the required silhouette. Use a standard/high
  detail source plus one later remesh when the hero asset needs more fidelity.
- Request PBR output only when textures are in scope. Verify which downloaded
  maps are base color, metallic, roughness, normal and emissive; do not infer a
  channel from its filename alone when metadata disagrees.
- For a moving rigid assembly, separate frame, leaves, panels and other moving
  pieces. Generated segmentation is only a starting point; validate and repair
  pivots locally before Unreal import.

## Project delivery contract

Meshy outputs are external source art. Apply `Docs/AssetPipeline.md` in full:

```text
ArtSource/<Feature>/
  README.md
  Original/       references and retained original delivery
  Prepared/       canonical FBX/GLB/OBJ and textures for import
  Scripts/        deterministic cleanup/import helpers when needed
  meshy_output/   local task lineage when retained for the feature
```

The feature README records input provenance, Meshy task IDs, date, license or
plan basis, actual credits, modifications, known limitations, canonical import
files and intended Unreal path. Do not store OAuth credentials, API keys or
expiring signed URLs.

Before production import, establish centimetre scale, Unreal orientation, frozen
transforms, pivots, material slots, UVs/tangents, collision, LOD/Nanite policy and
target-platform suitability. Use Blender or another deterministic local DCC pass
when generated geometry needs cleanup. Import and rename packages only through
Unreal-aware tooling, register runtime dependencies in the location manifest,
and run `Scripts/Validate-Locations.ps1` after manifest changes.

Meshy never owns gameplay facts. A generated door, item or creature is a visual
resource; Mass fragments and systems remain authoritative for identity, access,
state, persistence, costs, cooldowns and session rules.

## Failure handling

- Insufficient credits or plan entitlement: do not retry; report the typed error
  and preserve existing outputs.
- Authentication failure: distinguish missing credentials from network or token
  verification failure before starting a new device login.
- Timeout: resume waiting on the known task ID.
- Unknown submission: follow the CLI recovery command and reconcile before any
  new create.
- Generation succeeded but download failed: redownload from the same task.
- Unsuitable preview: show it and propose one concrete rerun or local correction
  with its separate cost; do not reroll automatically.
