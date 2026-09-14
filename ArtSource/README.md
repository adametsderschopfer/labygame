# Ground and night environment

Ground: user-supplied `Rugged_Gravel_Texture_Surface_01_2k.rar`.
Original images are preserved in `Ground/RuggedGravel`. The archive did not
contain license metadata. The ground material uses Albedo, Normal, Roughness
and AO; displacement is retained as source only and does not alter collision.

Night: [Qwantani Moon Noon](https://polyhaven.com/a/qwantani_moon_noon),
photography by Greg Zaal, processing by Jarod Guest, Poly Haven, CC0.
Downloaded 2026-09-14 using the Poly Haven API (4K HDR).
Source MD5: `d743c2f70019ab08ac8d05be6b0f5d71`.

Reimport/create assets with `Scripts/create_night_environment.py` using Unreal's
Python commandlet. Existing imported textures are reused; the two generated
materials are rebuilt. Do not rerun over manual material edits without saving
those edits separately.

`M_MazeGround.TileSizeCm` defaults to 200 cm and uses world XY coordinates.
Normal maps are sampled as world-space directions for the horizontal floor.
The source archive does not specify the normal-map Y convention; the imported
texture exposes Flip Green Channel for adjustment if needed.

`M_MoonNight.SkyBrightness` defaults to 0.015. The sky and light are installed
by `AMazeWorld::BeginPlay`, with fixed EV100 -2, moonlight 0.35 lux and skylight
intensity 0.08. The moon direction matches the brightest source HDRI pixel
(2463, 333 at 4096 x 2048). Presentation components belong to the maze actor
and are recreated for each world; gameplay remains in ECS.

The generated assets are stored under Content. Environment and Materials
are explicitly cooked, including their texture dependencies.
