#pragma once
#if WITH_EDITORONLY_DATA
#include "MZTrack.generated.h"
/** Track data used for connecting with mediaZ */
USTRUCT(BlueprintType)
struct FMZTrack
{
	GENERATED_BODY()
public:
	/** Please add a variable description */
	UPROPERTY(EditAnywhere, meta = (DisplayName = "location", Category = "",  MakeStructureDefaultValue = "0.000000,0.000000,0.000000"))
		FVector location;

	/** Please add a variable description */
	UPROPERTY(EditAnywhere, meta = (DisplayName = "rotation", Category = "", MakeStructureDefaultValue = "0.000000,0.000000,0.000000"))
		FVector rotation;

	/** Please add a variable description */
	UPROPERTY(EditAnywhere, meta = (DisplayName = "fov", Category = "", MakeStructureDefaultValue = "60.000000"))
		double fov;

	/** Please add a variable description */
	UPROPERTY(EditAnywhere, meta = (DisplayName = "focus_distance", Category = "", MakeStructureDefaultValue = "0.000000"))
		double focus_distance;

	/** Please add a variable description */
	UPROPERTY(EditAnywhere, meta = (DisplayName = "center_shift", Category = "", MakeStructureDefaultValue = "(X=0.000000,Y=0.000000)"))
		FVector2D center_shift;

	/** Please add a variable description */
	UPROPERTY(EditAnywhere, meta = (DisplayName = "k1", Category = "", MakeStructureDefaultValue = "0.000000"))
		double zoom;

	/** Please add a variable description */
	UPROPERTY(EditAnywhere, meta = (DisplayName = "k1", Category = "", MakeStructureDefaultValue = "0.000000"))
		float k1;

	/** Please add a variable description */
	UPROPERTY(EditAnywhere, meta = (DisplayName = "k2", Category = "",MakeStructureDefaultValue = "0.000000"))
		float k2;

	/** Please add a variable description */
	UPROPERTY(EditAnywhere, meta = (DisplayName = "render_ratio", Category = "", MakeStructureDefaultValue = "1.000000"))
		double render_ratio;

	/** Please add a variable description */
	UPROPERTY(EditAnywhere, meta = (DisplayName = "distortion_scale", Category = "", MakeStructureDefaultValue = "0.000000"))
		float distortion_scale;

	/** Please add a variable description */
	UPROPERTY(EditAnywhere, meta = (DisplayName = "sensor_size", Category = "", MakeStructureDefaultValue = "(X=0.000000,Y=0.000000)"))
		FVector2D sensor_size;

	/** Please add a variable description */
	UPROPERTY(EditAnywhere, meta = (DisplayName = "pixel_aspect_ratio", Category = "", MakeStructureDefaultValue = "1.000000"))
		double pixel_aspect_ratio;

	/** Please add a variable description */
	UPROPERTY(EditAnywhere, meta = (DisplayName = "nodal_offset", Category = "", MakeStructureDefaultValue = "0.000000"))
		double nodal_offset;
};

#endif