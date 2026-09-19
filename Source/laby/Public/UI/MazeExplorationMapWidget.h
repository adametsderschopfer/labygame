#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MazeExplorationMapWidget.generated.h"

// Presentation only: discovery is read from the owning player's Mass entity.
UCLASS()
class LABY_API UMazeExplorationMapWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void CenterOnPlayer();

protected:
	virtual int32 NativePaint(const FPaintArgs& Args,
	                          const FGeometry& Geometry,
	                          const FSlateRect& CullingRect,
	                          FSlateWindowElementList& Elements,
	                          int32 Layer,
	                          const FWidgetStyle& Style,
	                          bool bParentEnabled) const override;
	virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply NativeOnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply NativeOnMouseWheel(const FGeometry& Geometry, const FPointerEvent& Event) override;

private:
	FVector2D Center = FVector2D::ZeroVector;
	float Zoom = 24.f;

	void ClampCenter();
};
