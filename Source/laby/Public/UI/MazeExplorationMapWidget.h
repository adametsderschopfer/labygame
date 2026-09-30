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
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply NativeOnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply NativeOnMouseWheel(const FGeometry& Geometry, const FPointerEvent& Event) override;

private:
	static constexpr float DefaultZoom = 24.f;
	static constexpr float MinZoom = DefaultZoom * 0.75f;

	friend class SMazeMapSurface;
	UFUNCTION()
	void CloseMap();
	void RefreshControls();
	int32 PaintMap(const FGeometry& Geometry,
	               FSlateWindowElementList& Elements,
	               int32 Layer,
	               const FWidgetStyle& Style) const;

	FVector2D Center = FVector2D::ZeroVector;
	float Zoom = DefaultZoom;

	void ClampCenter();

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	mutable TArray<int32> DevelopmentRoute;
	mutable uint32 DevelopmentRouteRevision = 0;
	mutable int32 DevelopmentRouteSeed = 0;
	mutable int32 DevelopmentRouteCell = INDEX_NONE;
#endif
};
