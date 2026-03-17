// Copyright UET2A Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimSequence.h"
#include "RuntimeIKRetargeter.generated.h"

class UIKRetargeter;
class USkeleton;
class USkeletalMesh;

/**
 * Runtime IK Retargeting wrapper.
 * Handles animation retargeting from source skeleton to target skeleton at runtime.
 * Uses FIKRetargetProcessor (runtime API from IKRig module).
 * 
 * Supports two modes:
 * 1. Pre-configured: Use a UIKRetargeter asset created in editor
 * 2. Auto-mapping: Automatic bone name matching (best-effort)
 */
UCLASS(BlueprintType)
class UET2APLUGIN_API URuntimeIKRetargeter : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Retarget an animation sequence from source skeleton to target skeleton.
	 * 
	 * @param SourceAnim      Source animation to retarget
	 * @param SourceSkeleton  Source skeleton (from FBX import)
	 * @param TargetMesh      Target skeletal mesh (with skeleton)
	 * @param RetargetAsset   Pre-configured IKRetargeter asset (optional, nullptr = auto-map)
	 * @return                Retargeted animation for the target skeleton, or nullptr on failure
	 */
	UFUNCTION(BlueprintCallable, Category = "T2A|Retarget")
	UAnimSequence* RetargetAnimation(
		UAnimSequence* SourceAnim,
		USkeleton* SourceSkeleton,
		USkeletalMesh* TargetMesh,
		UIKRetargeter* RetargetAsset);

	/**
	 * Check if source and target skeletons are compatible (same bone hierarchy).
	 * If compatible, retargeting may not be needed.
	 */
	UFUNCTION(BlueprintCallable, Category = "T2A|Retarget")
	static bool AreSkeletonsCompatible(USkeleton* SourceSkeleton, USkeleton* TargetSkeleton);

	/** Clear cached retarget processors */
	UFUNCTION(BlueprintCallable, Category = "T2A|Retarget")
	void ClearCache();

private:
	// Manual retarget using bone name mapping
	UAnimSequence* RetargetWithBoneMapping(
		UAnimSequence* SourceAnim,
		USkeleton* SourceSkeleton,
		USkeleton* TargetSkeleton);

	// Retarget using IKRetargeter asset and FIKRetargetProcessor
	UAnimSequence* RetargetWithProcessor(
		UAnimSequence* SourceAnim,
		USkeleton* SourceSkeleton,
		USkeletalMesh* TargetMesh,
		UIKRetargeter* RetargetAsset);

	// Auto-build bone name mapping between source and target
	TMap<FName, FName> BuildAutoMapping(
		const FReferenceSkeleton& SourceRefSkel,
		const FReferenceSkeleton& TargetRefSkel) const;

	// Common bone name patterns for fuzzy matching
	static FName FindBestMatch(const FName& SourceBone, const TArray<FName>& TargetBoneNames);

	// Cache key for processor instances
	struct FRetargetCacheKey
	{
		TWeakObjectPtr<USkeleton> SourceSkeleton;
		TWeakObjectPtr<USkeletalMesh> TargetMesh;
		TWeakObjectPtr<UIKRetargeter> RetargetAsset;

		bool operator==(const FRetargetCacheKey& Other) const
		{
			return SourceSkeleton == Other.SourceSkeleton && 
				   TargetMesh == Other.TargetMesh && 
				   RetargetAsset == Other.RetargetAsset;
		}

		friend uint32 GetTypeHash(const FRetargetCacheKey& Key)
		{
			return HashCombine(
				GetTypeHash(Key.SourceSkeleton),
				HashCombine(GetTypeHash(Key.TargetMesh), GetTypeHash(Key.RetargetAsset)));
		}
	};

	// Cached bone mappings
	TMap<FRetargetCacheKey, TMap<FName, FName>> CachedMappings;
};
