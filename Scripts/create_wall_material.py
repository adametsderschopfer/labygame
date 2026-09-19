"""Author the wall ceramic with its top row aligned to the ceiling junction."""
from pathlib import Path
import runpy

runpy.run_path(str(Path(__file__).with_name("create_subway_material.py")),
               init_globals={"MATERIAL_FOLDER": "/Game/Materials/Laboratory",
                             "MATERIAL_NAME": "MazeWallCeramic"})
