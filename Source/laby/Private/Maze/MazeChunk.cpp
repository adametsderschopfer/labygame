#include "Maze/MazeChunk.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

int64 FMazeChunkData::GetGeometryBytes() const
{
	const auto Bytes = [](const FMazeSurface& Surface)
	{
		return Surface.Vertices.GetAllocatedSize() + Surface.Normals.GetAllocatedSize() +
		       Surface.Triangles.GetAllocatedSize();
	};
	int64 Total = Bytes(Walls) + Floors.GetAllocatedSize();

	for (const auto& Section : Interior.Sections)
		Total += Bytes(Section);

	return Total + Interior.SocketTransforms.GetAllocatedSize() + Interior.DetectorTransforms.GetAllocatedSize();
}
