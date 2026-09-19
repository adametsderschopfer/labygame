"""Author the coordinated Laboratory material set using Unreal Editor Python.

Run in the editor with: py "C:/ue_prj/laby/Scripts/create_laboratory_materials.py"
Rebuilding preserves material instance overrides. No level is modified.
"""
from pathlib import Path
import runpy
import unreal

FOLDER = "/Game/Materials/Laboratory"
LIB = unreal.MaterialEditingLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
runpy.run_path(str(Path(__file__).with_name("create_wall_material.py")))


runpy.run_path(str(Path(__file__).with_name("create_vinyl_material.py")))

runpy.run_path(str(Path(__file__).with_name("create_ceiling_material.py")))
unreal.log("LABY_LABORATORY_MATERIALS_SAVED")


