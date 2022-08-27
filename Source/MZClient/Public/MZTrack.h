#include "CoreMinimal.h"
#include "MZTrack.generated.h"

USTRUCT(BlueprintType)
struct MZCLIENT_API FZDTrackInfo
{
	GENERATED_BODY()

		UPROPERTY(BlueprintReadWrite, Category = Reality, Meta = (DisplayName = "Transform"))
		FTransform Transform;

	UPROPERTY(BlueprintReadWrite, Category = Reality, Meta = (DisplayName = "Raw Transform"))
		FTransform RawTransform;

	UPROPERTY(BlueprintReadWrite, Category = Reality, Meta = (DisplayName = "Camera Transform"))
		FTransform CameraTransform;

	UPROPERTY(BlueprintReadWrite, Category = Reality, Meta = (DisplayName = "Device Transform"))
		FTransform DeviceTransform;

	UPROPERTY(BlueprintReadWrite, Category = Reality, Meta = (DisplayName = "Field of View"))
		float FieldOfView;

	UPROPERTY(BlueprintReadWrite, Category = Reality, Meta = (DisplayName = "Focus"))
		float Focus;

	UPROPERTY(BlueprintReadWrite, Category = Reality, Meta = (DisplayName = "Zoom"))
		float Zoom;

	UPROPERTY(BlueprintReadWrite, Category = Reality, Meta = (DisplayName = "K1"))
		float K1;

	UPROPERTY(BlueprintReadWrite, Category = Reality, Meta = (DisplayName = "K2"))
		float K2;

	UPROPERTY(BlueprintReadWrite, Category = Reality, Meta = (DisplayName = "Center Shift"))
		FVector2D CenterShift;

	UPROPERTY(BlueprintReadWrite, Category = Reality, Meta = (DisplayName = "Depth of Field"))
		float DepthOfField;

	UPROPERTY(BlueprintReadWrite, Category = Reality, Meta = (DisplayName = "Render Ratio"))
		float RenderRatio;

	UPROPERTY(BlueprintReadWrite, Category = Reality, Meta = (DisplayName = "Distortion Scale"))
		float DistortionScale;

	UPROPERTY(BlueprintReadWrite, Category = Reality, Meta = (DisplayName = "Sensor Size"))
		FVector2D SensorSize;

	UPROPERTY(BlueprintReadWrite, Category = Reality, Meta = (DisplayName = "Pixel Aspect Ratio"))
		float PixelAspectRatio;

	UPROPERTY(BlueprintReadWrite, Category = Reality, Meta = (DisplayName = "Nodal Offset"))
		float NodalOffset;

	UPROPERTY(BlueprintReadWrite, Category = Reality, Meta = (DisplayName = "WorldToClip Matrix Row0"))
		FLinearColor WorldToClip0;

	UPROPERTY(BlueprintReadWrite, Category = Reality, Meta = (DisplayName = "WorldToClip Matrix Row1"))
		FLinearColor WorldToClip1;

	UPROPERTY(BlueprintReadWrite, Category = Reality, Meta = (DisplayName = "WorldToClip Matrix Row2"))
		FLinearColor WorldToClip2;

	UPROPERTY(BlueprintReadWrite, Category = Reality, Meta = (DisplayName = "WorldToClip Matrix Row3"))
		FLinearColor WorldToClip3;

};