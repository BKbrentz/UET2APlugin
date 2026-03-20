// Copyright UET2A Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HunyuanMotionTypes.generated.h"

/** Task status from Hunyuan API */
UENUM(BlueprintType)
enum class EHunyuanTaskStatus : uint8
{
	Unknown		UMETA(DisplayName = "Unknown"),
	Pending		UMETA(DisplayName = "Pending"),
	Processing	UMETA(DisplayName = "Processing"),
	Success		UMETA(DisplayName = "Success"),
	Failed		UMETA(DisplayName = "Failed")
};

/** Request parameters for Hunyuan Motion generation */
USTRUCT(BlueprintType)
struct UET2APLUGIN_API FHunyuanMotionRequest
{
	GENERATED_BODY()

	/** Text prompt describing the desired animation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "T2A|API")
	FString TextPrompt;

	/** Model identifier (default: HY-Motion-1.0-T2M) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "T2A|API")
	FString Model = TEXT("HY-Motion-1.0-T2M");

	/** Animation duration in seconds (1-12) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "T2A|API", meta=(ClampMin="1", ClampMax="12"))
	int32 Duration = 5;

	/** Random seed (empty = random) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "T2A|API")
	FString Seeds;

	/** Output format type */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "T2A|API")
	FString OutputType = TEXT("fbx");

	/** Whether to include mesh in output (false for faster generation at runtime) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "T2A|API")
	bool bMesh = false;

	/** Disable prompt rewriting */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "T2A|API")
	bool bDisableRewrite = false;

	/** Disable automatic duration estimation so Duration is treated as an explicit target by default */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "T2A|API")
	bool bDisableDurationEstimate = true;
};

/** Single motion result entry from the API */
USTRUCT(BlueprintType)
struct UET2APLUGIN_API FHunyuanMotionResultEntry
{
	GENERATED_BODY()

	/** Duration of the generated animation */
	UPROPERTY(BlueprintReadOnly, Category = "T2A|API")
	float Duration = 0.0f;

	/** FBX file URLs */
	UPROPERTY(BlueprintReadOnly, Category = "T2A|API")
	TArray<FString> Files;

	/** Number of generated files */
	UPROPERTY(BlueprintReadOnly, Category = "T2A|API")
	int32 Num = 0;

	/** The prompt (possibly rewritten) used for generation */
	UPROPERTY(BlueprintReadOnly, Category = "T2A|API")
	FString Prompt;

	/** Output type */
	UPROPERTY(BlueprintReadOnly, Category = "T2A|API")
	FString Type;
};

/** Complete result from the Hunyuan Motion API */
USTRUCT(BlueprintType)
struct UET2APLUGIN_API FHunyuanMotionResult
{
	GENERATED_BODY()

	/** Task ID (for async API) */
	UPROPERTY(BlueprintReadOnly, Category = "T2A|API")
	FString TaskId;

	/** Current task status */
	UPROPERTY(BlueprintReadOnly, Category = "T2A|API")
	EHunyuanTaskStatus Status = EHunyuanTaskStatus::Unknown;

	/** Motion results array */
	UPROPERTY(BlueprintReadOnly, Category = "T2A|API")
	TArray<FHunyuanMotionResultEntry> MotionEntries;

	/** Error message if failed */
	UPROPERTY(BlueprintReadOnly, Category = "T2A|API")
	FString ErrorMessage;

	/** Error code */
	UPROPERTY(BlueprintReadOnly, Category = "T2A|API")
	int32 ErrorCode = 0;
};

/** API error info for detailed error handling */
USTRUCT(BlueprintType)
struct UET2APLUGIN_API FHunyuanAPIError
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "T2A|API")
	int32 HttpStatusCode = 0;

	UPROPERTY(BlueprintReadOnly, Category = "T2A|API")
	int32 BusinessErrorCode = 0;

	UPROPERTY(BlueprintReadOnly, Category = "T2A|API")
	FString Message;

	/** Get a user-friendly error description */
	FString GetFriendlyMessage() const;

	/** Check if this is a rate limit error */
	bool IsRateLimitError() const { return HttpStatusCode == 429; }

	/** Check if this is an auth error */
	bool IsAuthError() const { return HttpStatusCode == 401; }
};
