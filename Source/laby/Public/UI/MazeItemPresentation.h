#pragma once

#include "CoreMinimal.h"
#include "Layout/Geometry.h"

class UUserWidget;
class FSlateWindowElementList;

namespace MazeItemPresentation
{
	LABY_API int32 PaintFocus(const UUserWidget& Owner,
	                          const FGeometry& Geometry,
	                          FSlateWindowElementList& Elements,
	                          int32 Layer);
	LABY_API int32 PaintDoorFocus(const UUserWidget& Owner,
	                              const FGeometry& Geometry,
	                              FSlateWindowElementList& Elements,
	                              int32 Layer);
}
