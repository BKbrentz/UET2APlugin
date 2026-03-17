// Copyright UET2A Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimSequence.h"
#include "RuntimeFBXImporter.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnFBXImportCompleted, UAnimSequence*, Animation, USkeleton*, Skeleton, const FString&, ErrorMsg);

/** Intermediate bone keyframe data extracted from FBX (non-UObject, thread-safe) */
struct FT2ABoneKeyframeData
{
	FString BoneName;
	TArray<FVector> Positions;
	TArray<FQuat> Rotations;
	TArray<FVector> Scales;
};

/** Intermediate skeleton data extracted from FBX (non-UObject, thread-safe) */
struct FT2ASkeletonData
{
	struct FBoneInfo
	{
		FString Name;
		int32 ParentIndex;
		FTransform LocalTransform;
	};
	TArray<FBoneInfo> Bones;
};

/** Intermediate animation data extracted from FBX (non-UObject, thread-safe) */
struct FT2AAnimationData
{
	FString AnimationName;
	float Duration;
	float FrameRate;
	int32 NumFrames;
	TArray<FT2ABoneKeyframeData> BoneTracks;
};

/**
 * Runtime FBX Animation Importer.
 * Uses FBX SDK directly to parse skeletal animation data from FBX files.
 * Does NOT depend on UnrealEd - fully runtime compatible.
 * 
 * Usage:
 *   1. Call ImportFBXAnimationAsync() - parses FBX on background thread
 *   2. OnImportCompleted fires on GameThread with UAnimSequence result
 */
UCLASS(BlueprintType)
class UET2APLUGIN_API URuntimeFBXImporter : public UObject
{
	GENERATED_BODY()

public:
	/** Import FBX animation asynchronously (recommended) */
	UFUNCTION(BlueprintCallable, Category = "T2A|Import")
	void ImportFBXAnimationAsync(const FString& FilePath);

	/** Import FBX animation synchronously (blocks GameThread - use for small files only) */
	UFUNCTION(BlueprintCallable, Category = "T2A|Import")
	bool ImportFBXAnimation(const FString& FilePath, UAnimSequence*& OutAnimation, USkeleton*& OutSkeleton);

	/** Fired when async import completes */
	UPROPERTY(BlueprintAssignable, Category = "T2A|Import")
	FOnFBXImportCompleted OnImportCompleted;

private:
	// Background thread: Parse FBX file using FBX SDK
	static bool ParseFBXFile(const FString& FilePath, FT2ASkeletonData& OutSkeleton, FT2AAnimationData& OutAnimation, FString& OutError);

	// GameThread: Build UE objects from parsed data
	static USkeleton* BuildSkeleton(const FT2ASkeletonData& SkeletonData);
	static UAnimSequence* BuildAnimSequence(const FT2AAnimationData& AnimData, USkeleton* Skeleton);

#if WITH_FBX_SDK
	// FBX SDK helpers
	static void ExtractSkeletonRecursive(void* InNode, int32 ParentIndex, FT2ASkeletonData& OutSkeleton);
	static void ExtractAnimationCurves(void* InScene, const FT2ASkeletonData& SkeletonData, FT2AAnimationData& OutAnimation);
#endif
};
