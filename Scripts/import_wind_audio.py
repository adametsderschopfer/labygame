"""Import the user-selected Pixabay recording with Unreal's native SoundFactory."""
from pathlib import Path
import unreal

source = Path(unreal.Paths.project_dir()) / "SourceAudio" / "dbsound-calm-wind-outside-426735.mp3"
task = unreal.AssetImportTask()
task.filename = str(source.resolve())
task.destination_path = "/Game/Audio/Ambience"
task.destination_name = "S_CalmWindOutside"
task.automated = True
task.replace_existing = True
task.save = False
task.factory = unreal.SoundFactory()
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
sound = unreal.load_asset("/Game/Audio/Ambience/S_CalmWindOutside")
if not isinstance(sound, unreal.SoundWave):
    raise RuntimeError("Wind recording did not import as a SoundWave")
sound.set_editor_property("looping", True)
sound.set_editor_property("volume", 1.0)
if not unreal.EditorAssetLibrary.save_loaded_asset(sound):
    raise RuntimeError("Could not save wind recording")
unreal.log("LABY_WIND_IMPORTED: S_CalmWindOutside; asset volume=1, looping=true")
