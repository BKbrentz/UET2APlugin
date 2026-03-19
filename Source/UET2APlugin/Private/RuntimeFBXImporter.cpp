// Copyright UET2A Team. All Rights Reserved.

#include "RuntimeFBXImporter.h"
#include "UET2APlugin.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Async/Async.h"
#include "Engine/SkeletalMesh.h"
#include "Misc/Guid.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

#if WITH_FBX_SDK
// Disable warnings from FBX SDK headers
THIRD_PARTY_INCLUDES_START
#include <fbxsdk.h>
THIRD_PARTY_INCLUDES_END
#endif

namespace
{
FString NormalizeDestinationAssetFolder(const FString& DestinationAssetFolder)
{
	FString NormalizedFolder = DestinationAssetFolder.TrimStartAndEnd();
	if (NormalizedFolder.IsEmpty())
	{
		return FString();
	}

	if (!NormalizedFolder.StartsWith(TEXT("/Game")))
	{
		NormalizedFolder.RemoveFromStart(TEXT("/"));
		NormalizedFolder = TEXT("/Game/") + NormalizedFolder;
	}

	while (NormalizedFolder.Len() > 5 && NormalizedFolder.EndsWith(TEXT("/")))
	{
		NormalizedFolder.LeftChopInline(1, false);
	}

	return NormalizedFolder;
}

FString SanitizeAssetName(const FString& InName, const TCHAR* FallbackName)
{
	FString SanitizedName;
	SanitizedName.Reserve(InName.Len());

	for (const TCHAR Character : InName)
	{
		SanitizedName.AppendChar((FChar::IsAlnum(Character) || Character == TEXT('_')) ? Character : TEXT('_'));
	}

	while (SanitizedName.ReplaceInline(TEXT("__"), TEXT("_")) > 0)
	{
	}

	if (SanitizedName.IsEmpty())
	{
		SanitizedName = FallbackName;
	}

	return SanitizedName;
}

FString MakeImportedAssetBaseName(const FString& InName)
{
	const FString BaseName = SanitizeAssetName(InName, TEXT("ImportedAnimation"));
	const FString UniqueSuffix = FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8);
	return FString::Printf(TEXT("%s_%s"), *BaseName, *UniqueSuffix);
}

#if WITH_EDITOR
bool SaveCreatedAsset(UObject* Asset)
{
	if (!Asset)
	{
		return false;
	}

	UPackage* Package = Asset->GetOutermost();
	if (!Package)
	{
		return false;
	}

	Asset->MarkPackageDirty();
	Package->MarkPackageDirty();
	FAssetRegistryModule::AssetCreated(Asset);

	const FString PackageFileName = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;

	return UPackage::SavePackage(Package, Asset, *PackageFileName, SaveArgs);
}
#endif
} // namespace

// ==================== Async Import ====================

void URuntimeFBXImporter::ImportFBXAnimationAsync(const FString& FilePath)
{
	ImportFBXAnimationAsyncToFolder(FilePath, FString());
}

void URuntimeFBXImporter::ImportFBXAnimationAsyncToFolder(const FString& FilePath, const FString& DestinationAssetFolder)
{
	const FString NormalizedDestinationAssetFolder = NormalizeDestinationAssetFolder(DestinationAssetFolder);

#if !WITH_EDITOR
	if (!NormalizedDestinationAssetFolder.IsEmpty())
	{
		UE_LOG(LogT2A, Warning, TEXT("Requested import to Content folder '%s', but asset saving is only available in editor builds. Falling back to transient import."), *NormalizedDestinationAssetFolder);
	}
#endif

	// Prevent garbage collection during async operation
	AddToRoot();

	// Parse FBX on background thread
	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [this, FilePath, NormalizedDestinationAssetFolder]()
	{
		FT2ASkeletonData SkeletonData;
		FT2AAnimationData AnimData;
		FString Error;

		const bool bSuccess = ParseFBXFile(FilePath, SkeletonData, AnimData, Error);
		if (bSuccess && AnimData.AnimationName.IsEmpty())
		{
			AnimData.AnimationName = FPaths::GetBaseFilename(FilePath);
		}

		const FString AssetBaseName = NormalizedDestinationAssetFolder.IsEmpty()
			? FString()
			: MakeImportedAssetBaseName(AnimData.AnimationName);

		// Build UE objects on GameThread
		AsyncTask(ENamedThreads::GameThread, [this, bSuccess, SkeletonData = MoveTemp(SkeletonData), AnimData = MoveTemp(AnimData), Error, NormalizedDestinationAssetFolder, AssetBaseName]()
		{
			RemoveFromRoot();

			if (!bSuccess)
			{
				UE_LOG(LogT2A, Error, TEXT("FBX import failed: %s"), *Error);
				OnImportCompleted.Broadcast(nullptr, nullptr, Error);
				return;
			}

			USkeleton* Skeleton = BuildSkeleton(SkeletonData, NormalizedDestinationAssetFolder, AssetBaseName);
			if (!Skeleton)
			{
				OnImportCompleted.Broadcast(nullptr, nullptr, TEXT("Failed to build skeleton"));
				return;
			}

			UAnimSequence* AnimSequence = BuildAnimSequence(AnimData, Skeleton, NormalizedDestinationAssetFolder, AssetBaseName);
			if (!AnimSequence)
			{
				OnImportCompleted.Broadcast(nullptr, Skeleton, TEXT("Failed to build animation sequence"));
				return;
			}

			UE_LOG(LogT2A, Log, TEXT("FBX import completed: %s (%.2fs, %d bones, %d frames)"),
				*AnimData.AnimationName, AnimData.Duration, SkeletonData.Bones.Num(), AnimData.NumFrames);

			OnImportCompleted.Broadcast(AnimSequence, Skeleton, TEXT(""));
		});
	});
}

bool URuntimeFBXImporter::ImportFBXAnimation(const FString& FilePath, UAnimSequence*& OutAnimation, USkeleton*& OutSkeleton)
{
	return ImportFBXAnimationToFolder(FilePath, OutAnimation, OutSkeleton, FString());
}

bool URuntimeFBXImporter::ImportFBXAnimationToFolder(const FString& FilePath, UAnimSequence*& OutAnimation, USkeleton*& OutSkeleton, const FString& DestinationAssetFolder)
{
	const FString NormalizedDestinationAssetFolder = NormalizeDestinationAssetFolder(DestinationAssetFolder);

#if !WITH_EDITOR
	if (!NormalizedDestinationAssetFolder.IsEmpty())
	{
		UE_LOG(LogT2A, Warning, TEXT("Requested import to Content folder '%s', but asset saving is only available in editor builds. Falling back to transient import."), *NormalizedDestinationAssetFolder);
	}
#endif

	FT2ASkeletonData SkeletonData;
	FT2AAnimationData AnimData;
	FString Error;

	if (!ParseFBXFile(FilePath, SkeletonData, AnimData, Error))
	{
		UE_LOG(LogT2A, Error, TEXT("FBX import failed: %s"), *Error);
		return false;
	}

	if (AnimData.AnimationName.IsEmpty())
	{
		AnimData.AnimationName = FPaths::GetBaseFilename(FilePath);
	}

	const FString AssetBaseName = NormalizedDestinationAssetFolder.IsEmpty()
		? FString()
		: MakeImportedAssetBaseName(AnimData.AnimationName);

	OutSkeleton = BuildSkeleton(SkeletonData, NormalizedDestinationAssetFolder, AssetBaseName);
	if (!OutSkeleton)
	{
		UE_LOG(LogT2A, Error, TEXT("Failed to build skeleton from FBX"));
		return false;
	}

	OutAnimation = BuildAnimSequence(AnimData, OutSkeleton, NormalizedDestinationAssetFolder, AssetBaseName);
	if (!OutAnimation)
	{
		UE_LOG(LogT2A, Error, TEXT("Failed to build animation from FBX"));
		return false;
	}

	return true;
}

// ==================== FBX SDK Parsing ====================

#if WITH_FBX_SDK

bool URuntimeFBXImporter::ParseFBXFile(const FString& FilePath, FT2ASkeletonData& OutSkeleton, FT2AAnimationData& OutAnimation, FString& OutError)
{
	// Initialize FBX SDK
	FbxManager* SdkManager = FbxManager::Create();
	if (!SdkManager)
	{
		OutError = TEXT("Failed to create FBX Manager");
		return false;
	}

	FbxIOSettings* IOSettings = FbxIOSettings::Create(SdkManager, IOSROOT);
	SdkManager->SetIOSettings(IOSettings);

	FbxImporter* Importer = FbxImporter::Create(SdkManager, "");
	
	// Initialize importer with file
	if (!Importer->Initialize(TCHAR_TO_UTF8(*FilePath), -1, SdkManager->GetIOSettings()))
	{
		OutError = FString::Printf(TEXT("Failed to initialize FBX importer: %s"), 
			UTF8_TO_TCHAR(Importer->GetStatus().GetErrorString()));
		SdkManager->Destroy();
		return false;
	}

	// Import scene
	FbxScene* Scene = FbxScene::Create(SdkManager, "ImportScene");
	if (!Importer->Import(Scene))
	{
		OutError = FString::Printf(TEXT("Failed to import FBX scene: %s"),
			UTF8_TO_TCHAR(Importer->GetStatus().GetErrorString()));
		Importer->Destroy();
		SdkManager->Destroy();
		return false;
	}
	Importer->Destroy();

	// Convert to standard axis/units
	FbxAxisSystem::EFrontVector FrontVector = (FbxAxisSystem::EFrontVector) - FbxAxisSystem::eParityOdd;
	const FbxAxisSystem UnrealAxisSystem(FbxAxisSystem::eZAxis, FrontVector, FbxAxisSystem::eRightHanded);
	if (Scene->GetGlobalSettings().GetAxisSystem() != UnrealAxisSystem)
	{
		// Note: we handle conversion in bone transforms instead, as ConvertScene can affect animations
	}

	FbxSystemUnit::cm.ConvertScene(Scene);

	// Find skeleton root
	FbxNode* RootNode = Scene->GetRootNode();
	if (!RootNode)
	{
		OutError = TEXT("FBX scene has no root node");
		SdkManager->Destroy();
		return false;
	}

	// Extract skeleton hierarchy
	// Find the first skeleton node
	TFunction<FbxNode*(FbxNode*)> FindSkeletonRoot = [&FindSkeletonRoot](FbxNode* Node) -> FbxNode*
	{
		if (Node->GetNodeAttribute() && Node->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eSkeleton)
		{
			return Node;
		}
		for (int i = 0; i < Node->GetChildCount(); i++)
		{
			FbxNode* Found = FindSkeletonRoot(Node->GetChild(i));
			if (Found) return Found;
		}
		return nullptr;
	};

	FbxNode* SkeletonRoot = FindSkeletonRoot(RootNode);
	if (!SkeletonRoot)
	{
		OutError = TEXT("No skeleton found in FBX file");
		SdkManager->Destroy();
		return false;
	}

	// Extract skeleton hierarchy
	ExtractSkeletonRecursive(SkeletonRoot, INDEX_NONE, OutSkeleton);

	if (OutSkeleton.Bones.Num() == 0)
	{
		OutError = TEXT("No bones extracted from FBX skeleton");
		SdkManager->Destroy();
		return false;
	}

	UE_LOG(LogT2A, Log, TEXT("Extracted %d bones from FBX"), OutSkeleton.Bones.Num());

	// Extract animation data
	ExtractAnimationCurves(Scene, OutSkeleton, OutAnimation);

	if (OutAnimation.NumFrames == 0)
	{
		OutError = TEXT("No animation data found in FBX file");
		SdkManager->Destroy();
		return false;
	}

	UE_LOG(LogT2A, Log, TEXT("Extracted animation: %s, %.2fs, %d frames @ %.1f FPS"),
		*OutAnimation.AnimationName, OutAnimation.Duration, OutAnimation.NumFrames, OutAnimation.FrameRate);

	// Cleanup
	SdkManager->Destroy();
	return true;
}

void URuntimeFBXImporter::ExtractSkeletonRecursive(void* InNode, int32 ParentIndex, FT2ASkeletonData& OutSkeleton)
{
	FbxNode* Node = static_cast<FbxNode*>(InNode);
	if (!Node) return;

	int32 CurrentIndex = OutSkeleton.Bones.Num();

	FT2ASkeletonData::FBoneInfo BoneInfo;
	BoneInfo.Name = UTF8_TO_TCHAR(Node->GetName());
	BoneInfo.ParentIndex = ParentIndex;

	// Get local transform
	FbxAMatrix LocalMatrix = Node->EvaluateLocalTransform();
	
	FbxVector4 T = LocalMatrix.GetT();
	FbxQuaternion Q = LocalMatrix.GetQ();
	FbxVector4 S = LocalMatrix.GetS();

	// Convert FBX coordinate system to UE (FBX: Y-up, right-handed -> UE: Z-up, left-handed)
	BoneInfo.LocalTransform.SetLocation(FVector(T[0], -T[1], T[2]));
	BoneInfo.LocalTransform.SetRotation(FQuat(-Q[0], Q[1], -Q[2], Q[3]));
	BoneInfo.LocalTransform.SetScale3D(FVector(S[0], S[1], S[2]));

	OutSkeleton.Bones.Add(BoneInfo);

	// Recurse children
	for (int i = 0; i < Node->GetChildCount(); i++)
	{
		FbxNode* Child = Node->GetChild(i);
		if (Child->GetNodeAttribute() && 
			Child->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eSkeleton)
		{
			ExtractSkeletonRecursive(Child, CurrentIndex, OutSkeleton);
		}
	}
}

void URuntimeFBXImporter::ExtractAnimationCurves(void* InScene, const FT2ASkeletonData& SkeletonData, FT2AAnimationData& OutAnimation)
{
	FbxScene* Scene = static_cast<FbxScene*>(InScene);
	if (!Scene) return;

	// Get the first animation stack
	int32 AnimStackCount = Scene->GetSrcObjectCount<FbxAnimStack>();
	if (AnimStackCount == 0)
	{
		UE_LOG(LogT2A, Warning, TEXT("No animation stacks in FBX"));
		return;
	}

	FbxAnimStack* AnimStack = Scene->GetSrcObject<FbxAnimStack>(0);
	if (!AnimStack) return;

	OutAnimation.AnimationName = UTF8_TO_TCHAR(AnimStack->GetName());

	// Get time span
	FbxTimeSpan TimeSpan = AnimStack->GetLocalTimeSpan();
	FbxTime StartTime = TimeSpan.GetStart();
	FbxTime EndTime = TimeSpan.GetStop();
	
	// Get frame rate
	FbxGlobalSettings& GlobalSettings = Scene->GetGlobalSettings();
	FbxTime::EMode TimeMode = GlobalSettings.GetTimeMode();
	OutAnimation.FrameRate = FbxTime::GetFrameRate(TimeMode);
	if (OutAnimation.FrameRate <= 0.0f)
	{
		OutAnimation.FrameRate = 30.0f; // Default to 30 FPS
	}

	OutAnimation.Duration = (float)(EndTime.GetSecondDouble() - StartTime.GetSecondDouble());
	OutAnimation.NumFrames = FMath::Max(1, (int32)(OutAnimation.Duration * OutAnimation.FrameRate) + 1);

	// Set evaluator to use this animation stack
	Scene->SetCurrentAnimationStack(AnimStack);

	FbxAnimLayer* AnimLayer = AnimStack->GetMember<FbxAnimLayer>(0);
	if (!AnimLayer) return;

	// Build bone name -> node map
	TMap<FString, FbxNode*> BoneNodeMap;
	TFunction<void(FbxNode*)> BuildNodeMap = [&BuildNodeMap, &BoneNodeMap](FbxNode* Node)
	{
		if (Node)
		{
			BoneNodeMap.Add(UTF8_TO_TCHAR(Node->GetName()), Node);
			for (int i = 0; i < Node->GetChildCount(); i++)
			{
				BuildNodeMap(Node->GetChild(i));
			}
		}
	};
	BuildNodeMap(Scene->GetRootNode());

	// Extract keyframe data for each bone
	OutAnimation.BoneTracks.SetNum(SkeletonData.Bones.Num());

	for (int32 BoneIdx = 0; BoneIdx < SkeletonData.Bones.Num(); BoneIdx++)
	{
		const FString& BoneName = SkeletonData.Bones[BoneIdx].Name;
		FT2ABoneKeyframeData& Track = OutAnimation.BoneTracks[BoneIdx];
		Track.BoneName = BoneName;

		FbxNode** NodePtr = BoneNodeMap.Find(BoneName);
		if (!NodePtr || !(*NodePtr)) continue;

		FbxNode* BoneNode = *NodePtr;

		Track.Positions.SetNum(OutAnimation.NumFrames);
		Track.Rotations.SetNum(OutAnimation.NumFrames);
		Track.Scales.SetNum(OutAnimation.NumFrames);

		for (int32 FrameIdx = 0; FrameIdx < OutAnimation.NumFrames; FrameIdx++)
		{
			FbxTime CurTime;
			CurTime.SetSecondDouble(StartTime.GetSecondDouble() + (double)FrameIdx / OutAnimation.FrameRate);

			FbxAMatrix LocalMatrix = BoneNode->EvaluateLocalTransform(CurTime);

			FbxVector4 T = LocalMatrix.GetT();
			FbxQuaternion Q = LocalMatrix.GetQ();
			FbxVector4 S = LocalMatrix.GetS();

			// Convert coordinate system
			Track.Positions[FrameIdx] = FVector(T[0], -T[1], T[2]);
			Track.Rotations[FrameIdx] = FQuat(-Q[0], Q[1], -Q[2], Q[3]);
			Track.Scales[FrameIdx] = FVector(S[0], S[1], S[2]);
		}
	}
}

#else // !WITH_FBX_SDK

bool URuntimeFBXImporter::ParseFBXFile(const FString& FilePath, FT2ASkeletonData& OutSkeleton, FT2AAnimationData& OutAnimation, FString& OutError)
{
	OutError = TEXT("FBX SDK is not available. Runtime FBX import is disabled.");
	return false;
}

#endif // WITH_FBX_SDK

// ==================== Build UE Objects (GameThread) ====================

USkeleton* URuntimeFBXImporter::BuildSkeleton(const FT2ASkeletonData& SkeletonData, const FString& DestinationAssetFolder, const FString& AssetBaseName)
{
	check(IsInGameThread());

	if (SkeletonData.Bones.Num() == 0)
	{
		return nullptr;
	}

	USkeleton* Skeleton = nullptr;
	const bool bSaveToContent = !DestinationAssetFolder.IsEmpty();

#if WITH_EDITOR
	if (bSaveToContent)
	{
		const FString SafeAssetBaseName = AssetBaseName.IsEmpty() ? MakeImportedAssetBaseName(TEXT("ImportedAnimation")) : AssetBaseName;
		const FString SkeletonAssetName = SanitizeAssetName(SafeAssetBaseName + TEXT("_Skeleton"), TEXT("ImportedSkeleton"));
		const FString SkeletonPackageName = DestinationAssetFolder + TEXT("/") + SkeletonAssetName;
		UPackage* SkeletonPackage = CreatePackage(*SkeletonPackageName);
		Skeleton = NewObject<USkeleton>(SkeletonPackage, *SkeletonAssetName, RF_Public | RF_Standalone);
	}
	else
#endif
	{
		Skeleton = NewObject<USkeleton>(GetTransientPackage(), NAME_None, RF_Transient);
	}

	// Set reference skeleton - use the merge path which is more robust across UE5 versions
	// We need to create a temporary skeletal mesh to properly initialize the skeleton
	USkeletalMesh* TempMesh = NewObject<USkeletalMesh>(GetTransientPackage(), NAME_None, RF_Transient);

	// Set the reference skeleton on the mesh's imported model
	FReferenceSkeleton& MeshRefSkel = TempMesh->GetRefSkeleton();
	{
		FReferenceSkeletonModifier MeshModifier(MeshRefSkel, Skeleton);
		for (int32 BoneIdx = 0; BoneIdx < SkeletonData.Bones.Num(); BoneIdx++)
		{
			const FT2ASkeletonData::FBoneInfo& Bone = SkeletonData.Bones[BoneIdx];
			FMeshBoneInfo BoneInfo;
			BoneInfo.Name = FName(*Bone.Name);
			BoneInfo.ParentIndex = Bone.ParentIndex;
			MeshModifier.Add(BoneInfo, Bone.LocalTransform);
		}
	}

	// Use MergeAllBonesToBoneTree to properly set up the skeleton
	Skeleton->MergeAllBonesToBoneTree(TempMesh);

#if WITH_EDITOR
	if (bSaveToContent)
	{
		if (SaveCreatedAsset(Skeleton))
		{
			UE_LOG(LogT2A, Log, TEXT("Built and saved skeleton asset: %s"), *Skeleton->GetPathName());
		}
		else
		{
			UE_LOG(LogT2A, Warning, TEXT("Built skeleton asset but failed to save package: %s"), *Skeleton->GetPathName());
		}
	}
	else
#endif
	{
		UE_LOG(LogT2A, Log, TEXT("Built transient skeleton with %d bones"), SkeletonData.Bones.Num());
	}

	return Skeleton;
}

UAnimSequence* URuntimeFBXImporter::BuildAnimSequence(const FT2AAnimationData& AnimData, USkeleton* Skeleton, const FString& DestinationAssetFolder, const FString& AssetBaseName)
{
	check(IsInGameThread());

	if (!Skeleton || AnimData.NumFrames == 0)
	{
		return nullptr;
	}

	UAnimSequence* AnimSequence = nullptr;
	const bool bSaveToContent = !DestinationAssetFolder.IsEmpty();

#if WITH_EDITOR
	if (bSaveToContent)
	{
		const FString SafeAssetBaseName = AssetBaseName.IsEmpty() ? MakeImportedAssetBaseName(AnimData.AnimationName) : AssetBaseName;
		const FString AnimAssetName = SanitizeAssetName(SafeAssetBaseName, TEXT("ImportedAnimation"));
		const FString AnimPackageName = DestinationAssetFolder + TEXT("/") + AnimAssetName;
		UPackage* AnimPackage = CreatePackage(*AnimPackageName);
		AnimSequence = NewObject<UAnimSequence>(AnimPackage, *AnimAssetName, RF_Public | RF_Standalone);
	}
	else
#endif
	{
		AnimSequence = NewObject<UAnimSequence>(GetTransientPackage(), NAME_None, RF_Transient);
	}

	AnimSequence->SetSkeleton(Skeleton);

	// Use IAnimationDataController (UE 5.2+)
	IAnimationDataController& Controller = AnimSequence->GetController();

	Controller.OpenBracket(FText::FromString(TEXT("RuntimeFBXImport")), false);
	Controller.SetFrameRate(FFrameRate(FMath::RoundToInt(AnimData.FrameRate), 1), false);
	Controller.SetNumberOfFrames(FFrameNumber(AnimData.NumFrames - 1), false);

	const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();

	for (int32 TrackIdx = 0; TrackIdx < AnimData.BoneTracks.Num(); TrackIdx++)
	{
		const FT2ABoneKeyframeData& Track = AnimData.BoneTracks[TrackIdx];
		if (Track.Positions.Num() == 0)
		{
			continue;
		}

		const FName BoneName(*Track.BoneName);

		// Verify bone exists in skeleton
		const int32 BoneIndex = RefSkeleton.FindBoneIndex(BoneName);
		if (BoneIndex == INDEX_NONE)
		{
			UE_LOG(LogT2A, Warning, TEXT("Bone '%s' not found in skeleton, skipping"), *Track.BoneName);
			continue;
		}

		// Add bone track
		Controller.AddBoneCurve(BoneName, false);

		// Build transform keys - convert FVector/FQuat to what the controller expects
		TArray<FVector> PosKeys = Track.Positions;
		TArray<FQuat> RotKeys = Track.Rotations;
		TArray<FVector> ScaleKeys = Track.Scales;

		Controller.SetBoneTrackKeys(BoneName, PosKeys, RotKeys, ScaleKeys, false);
	}

	Controller.CloseBracket(false);

#if WITH_EDITOR
	if (bSaveToContent)
	{
		if (SaveCreatedAsset(AnimSequence))
		{
			UE_LOG(LogT2A, Log, TEXT("Built and saved AnimSequence asset: %s"), *AnimSequence->GetPathName());
		}
		else
		{
			UE_LOG(LogT2A, Warning, TEXT("Built AnimSequence asset but failed to save package: %s"), *AnimSequence->GetPathName());
		}
	}
	else
#endif
	{
		UE_LOG(LogT2A, Log, TEXT("Built transient AnimSequence: %s (%.2fs, %d tracks)"),
			*AnimData.AnimationName, AnimData.Duration, AnimData.BoneTracks.Num());
	}

	return AnimSequence;
}
