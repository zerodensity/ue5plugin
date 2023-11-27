#pragma once
//#if WITH_EDITORONLY_DATA
#include "NOSTrack.generated.h"
/** Track data used for connecting with Nodos */
USTRUCT(Blueprintable)
struct NOSDATASTRUCTURES_API FNOSTrack 
{
	GENERATED_BODY()
public:
	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "Location", Category = "", MakeStructureDefaultValue = "0.000000,0.000000,0.000000"))
		FVector location = FVector::ZeroVector;

	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "Rotation", Category = "", MakeStructureDefaultValue = "0.000000,0.000000,0.000000"))
		FVector rotation = FVector::ZeroVector;

	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "FOV", Category = "", MakeStructureDefaultValue = "60.000000"))
		double fov = 60.0f;

	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "Focus Distance", Category = "", MakeStructureDefaultValue = "0.000000"))
		double focus_distance = 0.0f;

	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "Center Shift", Category = "", MakeStructureDefaultValue = "(X=0.000000,Y=0.000000)"))
		FVector2D center_shift = FVector2D::ZeroVector;

	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "zoom", Category = "", MakeStructureDefaultValue = "0.000000"))
		double zoom = 0.0f;

	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "k1", Category = "", MakeStructureDefaultValue = "0.000000"))
		float k1 = 0.0f;

	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "k2", Category = "", MakeStructureDefaultValue = "0.000000"))
		float k2 = 0.0f;

	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "Render Ratio", Category = "", MakeStructureDefaultValue = "1.000000"))
		double render_ratio = 1.0f;

	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "Distortion Scale", Category = "", MakeStructureDefaultValue = "0.000000"))
		float distortion_scale = 1.0f;

	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "Sensor Size", Category = "", MakeStructureDefaultValue = "(X=0.000000,Y=0.000000)"))
		FVector2D sensor_size = FVector2D(9.590f, 5.394f);

	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "pixel_aspect_ratio", Category = "", MakeStructureDefaultValue = "1.000000"))
		double pixel_aspect_ratio = 1.0f;

	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, meta = (DisplayName = "nodal_offset", Category = "", MakeStructureDefaultValue = "0.000000"))
		double nodal_offset = 0.0f;
};
