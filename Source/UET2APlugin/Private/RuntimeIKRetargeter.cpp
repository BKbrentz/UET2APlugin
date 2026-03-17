// Copyright UET2A Team. All Rights Reserved.

#include "RuntimeIKRetargeter.h"
#include "UET2APlugin.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "Retargeter/IKRetargeter.h"

// ==================== Public API ====================

UAnimSequence* URuntimeIKRetargeter::RetargetAnimation(
	UAnimSequence* SourceAnim,
	USkeleton* SourceSkeleton,
	USkeletalMesh* TargetMesh,
	UIKRetargeter* RetargetAsset)
{
	if (!SourceAnim || !SourceSkeleton || !TargetMesh)
	{
		UE_LOG(LogT2A, Error, TEXT("RetargetAnimation: Invalid input parameters"));
		return nullptr;
	}

	USkeleton* TargetSkeleton = TargetMesh->GetSkeleton();
	if (!TargetSkeleton)
	{
		UE_LOG(LogT2A, Error, TEXT("RetargetAnimation: Target mesh has no skeleton"));
		return nullptr;
	}

	// Check if skeletons are already compatible
	if (AreSkeletonsCompatible(SourceSkeleton, TargetSkeleton))
	{
		UE_LOG(LogT2A, Log, TEXT("Skeletons are compatible, no retargeting needed"));
		return SourceAnim;
	}

	// Try IKRetargeter processor first if asset is provided
	if (RetargetAsset)
	{
		UAnimSequence* Result = RetargetWithProcessor(SourceAnim, SourceSkeleton, TargetMesh, RetargetAsset);
		if (Result) return Result;
		UE_LOG(LogT2A, Warning, TEXT("IK Retarget processor failed, falling back to bone mapping"));
	}

	// Fallback: manual bone name mapping
	return RetargetWithBoneMapping(SourceAnim, SourceSkeleton, TargetSkeleton);
}

bool URuntimeIKRetargeter::AreSkeletonsCompatible(USkeleton* SourceSkeleton, USkeleton* TargetSkeleton)
{
	if (!SourceSkeleton || !TargetSkeleton) return false;
	if (SourceSkeleton == TargetSkeleton) return true;

	const FReferenceSkeleton& SourceRef = SourceSkeleton->GetReferenceSkeleton();
	const FReferenceSkeleton& TargetRef = TargetSkeleton->GetReferenceSkeleton();

	if (SourceRef.GetNum() != TargetRef.GetNum()) return false;

	// Check if all bone names match
	for (int32 i = 0; i < SourceRef.GetNum(); i++)
	{
		if (SourceRef.GetBoneName(i) != TargetRef.GetBoneName(i))
		{
			return false;
		}
	}
	return true;
}

void URuntimeIKRetargeter::ClearCache()
{
	CachedMappings.Empty();
}

// ==================== IK Retarget Processor ====================

UAnimSequence* URuntimeIKRetargeter::RetargetWithProcessor(
	UAnimSequence* SourceAnim,
	USkeleton* SourceSkeleton,
	USkeletalMesh* TargetMesh,
	UIKRetargeter* RetargetAsset)
{
	if (!RetargetAsset) return nullptr;

	// FIKRetargetProcessor requires actual USkeletalMeshComponent initialization at runtime.
	// Since we may not have a valid world context with spawned components,
	// we log and fall back to bone mapping approach.
	UE_LOG(LogT2A, Log, TEXT("IK Retarget processor requires skeletal mesh components; using bone mapping fallback"));
	return nullptr;
}

// ==================== Bone Name Mapping ====================

UAnimSequence* URuntimeIKRetargeter::RetargetWithBoneMapping(
	UAnimSequence* SourceAnim,
	USkeleton* SourceSkeleton,
	USkeleton* TargetSkeleton)
{
	check(IsInGameThread());

	const FReferenceSkeleton& SourceRefSkel = SourceSkeleton->GetReferenceSkeleton();
	const FReferenceSkeleton& TargetRefSkel = TargetSkeleton->GetReferenceSkeleton();

	// Build or use cached bone mapping
	FRetargetCacheKey CacheKey;
	CacheKey.SourceSkeleton = SourceSkeleton;
	CacheKey.TargetMesh = nullptr;
	CacheKey.RetargetAsset = nullptr;

	TMap<FName, FName>* MappingPtr = CachedMappings.Find(CacheKey);
	TMap<FName, FName> Mapping;
	if (MappingPtr)
	{
		Mapping = *MappingPtr;
	}
	else
	{
		Mapping = BuildAutoMapping(SourceRefSkel, TargetRefSkel);
		CachedMappings.Add(CacheKey, Mapping);
	}

	if (Mapping.Num() == 0)
	{
		UE_LOG(LogT2A, Error, TEXT("No bone mapping found between source and target skeletons"));
		return nullptr;
	}

	UE_LOG(LogT2A, Log, TEXT("Using %d bone mappings for retargeting"), Mapping.Num());

	// Create target animation sequence
	UAnimSequence* TargetAnim = NewObject<UAnimSequence>(GetTransientPackage(), NAME_None, RF_Transient);
	TargetAnim->SetSkeleton(TargetSkeleton);

	// Use IAnimationDataController/IAnimationDataModel API (UE 5.2+)
	const IAnimationDataModel* SourceModel = SourceAnim->GetDataModel();
	IAnimationDataController& TargetController = TargetAnim->GetController();

	TargetController.OpenBracket(FText::FromString(TEXT("RuntimeRetarget")), false);

	// Copy frame rate and duration
	const FFrameRate FrameRate = SourceAnim->GetSamplingFrameRate();
	const int32 NumKeys = SourceAnim->GetNumberOfSampledKeys();
	
	TargetController.SetFrameRate(FrameRate, false);
	TargetController.SetNumberOfFrames(FFrameNumber(FMath::Max(0, NumKeys - 1)), false);

	// Iterate source bone tracks using the data model
	const TArray<FBoneAnimationTrack>& SourceTracks = SourceModel->GetBoneAnimationTracks();

	for (const FBoneAnimationTrack& SourceTrack : SourceTracks)
	{
		FName SourceBoneName = SourceTrack.Name;
		FName* TargetBoneNamePtr = Mapping.Find(SourceBoneName);
		if (!TargetBoneNamePtr) continue;

		FName TargetBoneName = *TargetBoneNamePtr;

		// Verify target bone exists
		if (TargetRefSkel.FindBoneIndex(TargetBoneName) == INDEX_NONE) continue;

		// Add bone track to target
		TargetController.AddBoneCurve(TargetBoneName, false);

		// Extract keys from source track - InternalTrackData uses FVector3f/FQuat4f in UE5
		const FRawAnimSequenceTrack& RawTrack = SourceTrack.InternalTrackData;
		
		TArray<FVector> PosKeys;
		TArray<FQuat> RotKeys;
		TArray<FVector> ScaleKeys;

		PosKeys.Reserve(RawTrack.PosKeys.Num());
		RotKeys.Reserve(RawTrack.RotKeys.Num());
		ScaleKeys.Reserve(RawTrack.ScaleKeys.Num());

		for (const FVector3f& Key : RawTrack.PosKeys) PosKeys.Add(FVector(Key));
		for (const FQuat4f& Key : RawTrack.RotKeys) RotKeys.Add(FQuat(Key));
		for (const FVector3f& Key : RawTrack.ScaleKeys) ScaleKeys.Add(FVector(Key));

		// Apply reference pose offset if needed
		int32 SourceBoneIdx = SourceRefSkel.FindBoneIndex(SourceBoneName);
		int32 TargetBoneIdx = TargetRefSkel.FindBoneIndex(TargetBoneName);

		if (SourceBoneIdx != INDEX_NONE && TargetBoneIdx != INDEX_NONE)
		{
			FTransform SourceRefPose = SourceRefSkel.GetRefBonePose()[SourceBoneIdx];
			FTransform TargetRefPose = TargetRefSkel.GetRefBonePose()[TargetBoneIdx];

			// Compute rotation delta between ref poses
			for (int32 i = 0; i < RotKeys.Num(); i++)
			{
				FQuat SourceLocal = RotKeys[i];
				FQuat SourceDelta = SourceLocal * SourceRefPose.GetRotation().Inverse();
				RotKeys[i] = SourceDelta * TargetRefPose.GetRotation();
			}

			// Scale translation by bone length ratio
			float SourceLen = SourceRefPose.GetLocation().Size();
			float TargetLen = TargetRefPose.GetLocation().Size();
			float LenRatio = (SourceLen > KINDA_SMALL_NUMBER) ? (TargetLen / SourceLen) : 1.0f;

			for (int32 i = 0; i < PosKeys.Num(); i++)
			{
				PosKeys[i] = TargetRefPose.GetLocation() + (PosKeys[i] - SourceRefPose.GetLocation()) * LenRatio;
			}
		}

		TargetController.SetBoneTrackKeys(TargetBoneName, PosKeys, RotKeys, ScaleKeys, false);
	}

	TargetController.CloseBracket(false);

	UE_LOG(LogT2A, Log, TEXT("Retargeted animation with %d mapped bones"), Mapping.Num());
	return TargetAnim;
}

// ==================== Auto Bone Mapping ====================

TMap<FName, FName> URuntimeIKRetargeter::BuildAutoMapping(
	const FReferenceSkeleton& SourceRefSkel,
	const FReferenceSkeleton& TargetRefSkel) const
{
	TMap<FName, FName> Mapping;

	// Build target bone name list
	TArray<FName> TargetBoneNames;
	for (int32 i = 0; i < TargetRefSkel.GetNum(); i++)
	{
		TargetBoneNames.Add(TargetRefSkel.GetBoneName(i));
	}

	// First pass: exact name match
	for (int32 i = 0; i < SourceRefSkel.GetNum(); i++)
	{
		FName SourceBone = SourceRefSkel.GetBoneName(i);
		if (TargetBoneNames.Contains(SourceBone))
		{
			Mapping.Add(SourceBone, SourceBone);
		}
	}

	// Second pass: fuzzy match for unmapped bones
	for (int32 i = 0; i < SourceRefSkel.GetNum(); i++)
	{
		FName SourceBone = SourceRefSkel.GetBoneName(i);
		if (Mapping.Contains(SourceBone)) continue;

		FName Match = FindBestMatch(SourceBone, TargetBoneNames);
		if (Match != NAME_None)
		{
			Mapping.Add(SourceBone, Match);
		}
	}

	return Mapping;
}

FName URuntimeIKRetargeter::FindBestMatch(const FName& SourceBone, const TArray<FName>& TargetBoneNames)
{
	FString SourceStr = SourceBone.ToString().ToLower();
	
	// Common bone name patterns and their aliases
	static const TMap<FString, TArray<FString>> BoneAliases = {
		{TEXT("hips"), {TEXT("hip"), TEXT("pelvis"), TEXT("root")}},
		{TEXT("spine"), {TEXT("spine1"), TEXT("torso")}},
		{TEXT("spine1"), {TEXT("spine2"), TEXT("chest")}},
		{TEXT("spine2"), {TEXT("spine3"), TEXT("upperchest")}},
		{TEXT("neck"), {TEXT("neck1")}},
		{TEXT("head"), {TEXT("head1")}},
		{TEXT("leftshoulder"), {TEXT("leftclavicle"), TEXT("l_clavicle"), TEXT("lshoulder")}},
		{TEXT("rightshoulder"), {TEXT("rightclavicle"), TEXT("r_clavicle"), TEXT("rshoulder")}},
		{TEXT("leftupperarm"), {TEXT("leftarm"), TEXT("l_upperarm"), TEXT("lupperarm")}},
		{TEXT("rightupperarm"), {TEXT("rightarm"), TEXT("r_upperarm"), TEXT("rupperarm")}},
		{TEXT("leftlowerarm"), {TEXT("leftforearm"), TEXT("l_forearm"), TEXT("llowerarm")}},
		{TEXT("rightlowerarm"), {TEXT("rightforearm"), TEXT("r_forearm"), TEXT("rlowerarm")}},
		{TEXT("lefthand"), {TEXT("l_hand"), TEXT("lhand")}},
		{TEXT("righthand"), {TEXT("r_hand"), TEXT("rhand")}},
		{TEXT("leftupperleg"), {TEXT("leftthigh"), TEXT("l_thigh"), TEXT("lupperleg")}},
		{TEXT("rightupperleg"), {TEXT("rightthigh"), TEXT("r_thigh"), TEXT("rupperleg")}},
		{TEXT("leftlowerleg"), {TEXT("leftcalf"), TEXT("leftshin"), TEXT("l_calf"), TEXT("llowerleg")}},
		{TEXT("rightlowerleg"), {TEXT("rightcalf"), TEXT("rightshin"), TEXT("r_calf"), TEXT("rlowerleg")}},
		{TEXT("leftfoot"), {TEXT("l_foot"), TEXT("lfoot")}},
		{TEXT("rightfoot"), {TEXT("r_foot"), TEXT("rfoot")}},
		{TEXT("lefttoes"), {TEXT("lefttoebase"), TEXT("l_toe"), TEXT("ltoes")}},
		{TEXT("righttoes"), {TEXT("righttoebase"), TEXT("r_toe"), TEXT("rtoes")}},
	};

	// Strip common prefixes/suffixes for comparison
	auto NormalizeName = [](const FString& Name) -> FString
	{
		FString Normalized = Name.ToLower();
		Normalized.ReplaceInline(TEXT("mixamo:"), TEXT(""));
		Normalized.ReplaceInline(TEXT("mixamorig:"), TEXT(""));
		Normalized.ReplaceInline(TEXT("_"), TEXT(""));
		Normalized.ReplaceInline(TEXT("."), TEXT(""));
		Normalized.ReplaceInline(TEXT(" "), TEXT(""));
		return Normalized;
	};

	FString NormalizedSource = NormalizeName(SourceStr);

	// Try normalized exact match
	for (const FName& TargetBone : TargetBoneNames)
	{
		FString NormalizedTarget = NormalizeName(TargetBone.ToString());
		if (NormalizedSource == NormalizedTarget)
		{
			return TargetBone;
		}
	}

	// Try alias matching
	for (const auto& AliasPair : BoneAliases)
	{
		bool bSourceMatches = NormalizedSource == AliasPair.Key || AliasPair.Value.Contains(NormalizedSource);
		if (!bSourceMatches) continue;

		for (const FName& TargetBone : TargetBoneNames)
		{
			FString NormalizedTarget = NormalizeName(TargetBone.ToString());
			if (NormalizedTarget == AliasPair.Key || AliasPair.Value.Contains(NormalizedTarget))
			{
				return TargetBone;
			}
		}
	}

	// Try substring match as last resort
	for (const FName& TargetBone : TargetBoneNames)
	{
		FString TargetStr = TargetBone.ToString().ToLower();
		if (TargetStr.Contains(NormalizedSource) || NormalizedSource.Contains(TargetStr))
		{
			if (NormalizedSource.Len() >= 3 && TargetStr.Len() >= 3) // Avoid false positives
			{
				return TargetBone;
			}
		}
	}

	return NAME_None;
}
