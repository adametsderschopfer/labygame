# Ward interface: approved reference and rebuild contract

User-approved direction, 2026-09-26. The newest hospital menu/HUD references
supersede the earlier green abstract "LABY / ЛАБИРИНТ" concept.

Reference images are supplied by the user and retained here for design comparison
only. They are **not runtime backgrounds**, textures, or game-level geometry.
No third-party icon/font package is introduced. Icons are project-authored UMG
geometry; typography uses Unreal's bundled Roboto Light. Existing original menu
audio remains in `ArtSource/UI/Glass` with its provenance.

## Non-negotiable visual direction

- Neutral dark gray-green gradient on menu, settings and full map. Never use
  the photograph of a hospital corridor as the background.
- Latest palette revision overrides the reference colors: thin gray-green borders,
  translucent dark panels and restrained sage highlights, matching the actual floor.
  `MazeInterfaceStyle::Palette()` derives all UI swatches from the linear RGB
  `FloorColor (0.20, 0.265, 0.235)` inherited by `MI_LabVinylSatin` (verified in
  the editor, no instance override). No runtime material loading is introduced.
- Sentence-case captions, no tracking on normal text. Widely spaced LABY wordmark.
- Narrow framed menu buttons with a right chevron; primary action sage filled.
  Main-menu captions and geometric chevrons form one centered auto-sized group
  with a 10 px gap, like Close. Chevron strokes inherit button foreground states.
- HUD: square 320×320 minimap lower right; health/stamina lower left; four empty
  inventory slots lower center. Inventory is presentation only, not fake items.
- Latest user revisions override the images: no health/stamina icons; that panel
  is reduced from 500×122 to 375×92. Inventory slots are 54×54, down from 80×80.
  Vitals labels sit above their bars. Inventory has no numbers or keycap badges.
  Minimap has no inner frame or corner brackets. Restore all compass directions
  around the map (N top, E right, S bottom, W left) and the bearing dots, not a
  lone N in a header. Place “M — Карта” above the panel at its left edge; the key
  follows the actual binding. Only the extra decorative inner frame is removed.
  The map fills the entire square without inner gutters; compass letters overlay
  the map near its edges, rather than reserving margins around the map.
  Full map fills the screen, with an accent-filled Close button at the upper right
  and a small two-column floating legend at the lower left. No map title badge.
  Close is 176×48 with a centered auto-sized HorizontalBox: 18 pt caption,
  10 px gap, 14×14 geometric X, not a baseline-aligned font glyph.
  Map backdrops are translucent (full 80%, mini 68% opaque), markers stay legible.
- Loading: neutral gradient, LABY and one changing status, no timer or counters.
- No new sanity mechanic, interaction rule, objective, floor selector or room
  topology merely because one appears in the concept image.
- A separate Sound settings page for menu music and interface sound volume.
- Cursor: small rounded triangle, like a conventional pointer. No circle or
  circular hover animation. The tip matches Slate's centered software hotspot.

## Independent runtime assets

`/Game/UI/Ward/WBP_WardMainMenu`, `WBP_WardPauseMenu`, `WBP_WardSettings`,
`WBP_WardHUD`, `WBP_WardMap` are created from empty WidgetTrees by
`Source/labyEditor/Private/UI/MazeWardUIAssets.cpp`. They are **not duplicates,
children or modified versions of the old /Game/UI/WBP_* packages**. Parent native
classes only provide existing event bindings and read-only ECS presentation.

`laby.UI.BuildWard` creates/compiles/saves the five assets with Play stopped.
Rerunning replaces layouts **only in the Ward family**; preserve any manual Ward
layout edits before rerunning. It never loads or modifies legacy WBPs.
`laby.UI.PreviewWard` writes static design previews to `Saved/UI/WardPreview`.
`laby.UI.BuildWardHUDMap` rebuilds only the HUD and map, preserving menu layouts.
`laby.UI.BuildWardMainMenu` rebuilds only the main menu, preserving other screens.
These previews are not proof of runtime interaction behavior.

The focused typed `MazeUIAssets` catalog selects this family. All five classes,
hover/press sounds and ambient music are in the location resource manifest for
preload/cook coverage. HUD/gameplay ownership remains unchanged. Loading remains
native Slate because the MoviePlayer must draw during blocking game-thread loads.
