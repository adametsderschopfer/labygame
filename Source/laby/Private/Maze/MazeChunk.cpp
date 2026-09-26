#include "Maze/MazeChunk.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

int64 FMazeChunkData::GetGeometryBytes() const
{
	const auto Bytes = [](const FMazeSurface& Surface)
	{
		return Surface.Vertices.GetAllocatedSize() + Surface.Normals.GetAllocatedSize() +
		       Surface.Triangles.GetAllocatedSize();
	};
	int64 Total = Bytes(Walls) + Bytes(Water) + Floors.GetAllocatedSize() + Ceilings.GetAllocatedSize();

	for (const auto& Section : Interior.Sections)
		Total += Bytes(Section);

	for (const auto& Surfaces : RoomSurfaces)
		Total += Surfaces.GetAllocatedSize();

	return Total + Interior.SocketTransforms.GetAllocatedSize() + Interior.DetectorTransforms.GetAllocatedSize();
}
