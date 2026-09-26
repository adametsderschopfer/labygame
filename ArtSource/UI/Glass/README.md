# Glass interface artwork

`MenuBackdrop.png` is an earlier laboratory concept plate. Its imported
`/Game/UI/Glass/T_MenuBackdrop` remains for provenance but is not displayed.
The current menu and settings use a neutral, slowly moving blue-graphite gradient
drawn in Slate beneath the editable UMG layout. The full map draws a matching
gradient. No photographic backdrop is used on these screens.

The three WAV files are original deterministic synthesis from
`Scripts/create_ui_sounds.py`. `S_UIHover` is a quiet airy cue, `S_UIPress` a soft
confirmation, and `S_MenuAmbient` a 48-second stereo loop of low tones and distant
texture. No downloaded recordings, speech, or third-party samples are used.
The runtime Sound Waves live in `/Game/UI/Glass`; the ambient sound is also listed
in the Maze location asset manifest and loaded asynchronously when a menu opens.
