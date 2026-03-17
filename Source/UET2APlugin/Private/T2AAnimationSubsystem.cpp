// Copyright UET2A Team. All Rights Reserved.

#include "T2AAnimationSubsystem.h"
#include "UET2APlugin.h"
#include "HunyuanMotionAPI.h"
#include "FBXDownloader.h"
#include "RuntimeFBXImporter.h"
#include "RuntimeIKRetargeter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "Retargeter/IKRetargeter.h"

// ==================== Lifecycle ====================

void UT2AAnimationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Create pipeline components
	API = NewObject<UHunyuanMotionAPI>(this);
	Downloader = NewObject<UFBXDownloader>(this);
	Importer = NewObject<URuntimeFBXImporter>(this);
	Retargeter = NewObject<URuntimeIKRetargeter>(this);

	// Bind API callbacks
	API->OnTaskSubmitted.AddDynamic(this, &UT2AAnimationSubsystem::OnTaskSubmitted);
	API->OnTaskCompleted.AddDynamic(this, &UT2AAnimationSubsystem::OnTaskCompleted);
	API->OnTaskProgress.AddDynamic(this, &UT2AAnimationSubsystem::OnTaskProgress);

	// Bind downloader callback
	Downloader->OnDownloadCompleted.AddDynamic(this, &UT2AAnimationSubsystem::OnDownloadComplete);

	// Bind importer callback
	Importer->OnImportCompleted.AddDynamic(this, &UT2AAnimationSubsystem::OnImportComplete);

	UE_LOG(LogT2A, Log, TEXT("T2A Animation Subsystem initialized"));
}

void UT2AAnimationSubsystem::Deinitialize()
{
	CancelPipeline();
	Super::Deinitialize();
}

// ==================== Configuration ====================

void UT2AAnimationSubsystem::SetAPIKey(const FString& InAPIKey)
{
	if (API)
	{
		API->SetAPIKey(InAPIKey);
	}
}

void UT2AAnimationSubsystem::SetBaseURL(const FString& InBaseURL)
{
	if (API)
	{
		API->SetBaseURL(InBaseURL);
	}
}

// ==================== Pipeline Control ====================

void UT2AAnimationSubsystem::RunPipeline(const FT2APipelineConfig& Config)
{
	if (IsRunning())
	{
		UE_LOG(LogT2A, Warning, TEXT("Pipeline already running, cancelling current and starting new"));
		CancelPipeline();
	}

	CurrentConfig = Config;
	CurrentRewrittenPrompt.Empty();

	if (Config.TextPrompt.IsEmpty())
	{
		FailPipeline(TEXT("Text prompt is empty"));
		return;
	}

	if (API->GetAPIKey().IsEmpty())
	{
		FailPipeline(TEXT("API Key is not configured. Call SetAPIKey() first."));
		return;
	}

	UE_LOG(LogT2A, Log, TEXT("Starting T2A pipeline: \"%s\" (duration: %ds)"), *Config.TextPrompt, Config.Duration);
	StartSubmitting();
}

void UT2AAnimationSubsystem::CancelPipeline()
{
	if (API) API->StopPolling();
	if (Downloader) Downloader->CancelDownload();
	
	CurrentStage = ET2APipelineStage::Idle;
	UE_LOG(LogT2A, Log, TEXT("Pipeline cancelled"));
}

// ==================== Pipeline Stages ====================

void UT2AAnimationSubsystem::StartSubmitting()
{
	CurrentStage = ET2APipelineStage::Submitting;
	OnPipelineProgress.Broadcast(CurrentStage, 0.0f, TEXT("Submitting to Hunyuan API..."));
	OnPipelineProgressNative.Broadcast(CurrentStage, 0.0f, TEXT("Submitting to Hunyuan API..."));

	FHunyuanMotionRequest Request;
	Request.TextPrompt = CurrentConfig.TextPrompt;
	Request.Duration = CurrentConfig.Duration;
	Request.bMesh = CurrentConfig.bMesh;
	Request.bDisableRewrite = CurrentConfig.bDisableRewrite;

	API->SubmitMotionTask(Request);
}

void UT2AAnimationSubsystem::OnTaskSubmitted(const FString& TaskId, const FString& ErrorMsg)
{
	if (!ErrorMsg.IsEmpty())
	{
		FailPipeline(FString::Printf(TEXT("Task submission failed: %s"), *ErrorMsg));
		return;
	}

	UE_LOG(LogT2A, Log, TEXT("Task submitted: %s"), *TaskId);
	StartPolling(TaskId);
}

void UT2AAnimationSubsystem::StartPolling(const FString& TaskId)
{
	CurrentStage = ET2APipelineStage::Polling;
	OnPipelineProgress.Broadcast(CurrentStage, 0.1f, TEXT("Waiting for animation generation..."));
	OnPipelineProgressNative.Broadcast(CurrentStage, 0.1f, TEXT("Waiting for animation generation..."));

	API->StartPollingTask(TaskId);
}

void UT2AAnimationSubsystem::OnTaskProgress(EHunyuanTaskStatus Status, const FString& StatusMessage)
{
	float Progress = 0.1f;
	switch (Status)
	{
	case EHunyuanTaskStatus::Pending: Progress = 0.15f; break;
	case EHunyuanTaskStatus::Processing: Progress = 0.3f; break;
	default: break;
	}

	OnPipelineProgress.Broadcast(CurrentStage, Progress, StatusMessage);
	OnPipelineProgressNative.Broadcast(CurrentStage, Progress, StatusMessage);
}

void UT2AAnimationSubsystem::OnTaskCompleted(const FHunyuanMotionResult& Result, const FString& ErrorMsg)
{
	if (!ErrorMsg.IsEmpty() || Result.Status == EHunyuanTaskStatus::Failed)
	{
		FailPipeline(FString::Printf(TEXT("Generation failed: %s"), *(!ErrorMsg.IsEmpty() ? ErrorMsg : Result.ErrorMessage)));
		return;
	}

	// Get the first FBX URL from results
	if (Result.MotionEntries.Num() == 0 || Result.MotionEntries[0].Files.Num() == 0)
	{
		FailPipeline(TEXT("No FBX files in API response"));
		return;
	}

	// Save rewritten prompt if available
	if (Result.MotionEntries[0].Prompt.Len() > 0)
	{
		CurrentRewrittenPrompt = Result.MotionEntries[0].Prompt;
	}

	FString FBXURL = Result.MotionEntries[0].Files[0];
	UE_LOG(LogT2A, Log, TEXT("Got FBX URL: %s"), *FBXURL);

	StartDownloading(FBXURL);
}

void UT2AAnimationSubsystem::StartDownloading(const FString& FBXURL)
{
	CurrentStage = ET2APipelineStage::Downloading;
	OnPipelineProgress.Broadcast(CurrentStage, 0.5f, TEXT("Downloading FBX file..."));
	OnPipelineProgressNative.Broadcast(CurrentStage, 0.5f, TEXT("Downloading FBX file..."));

	Downloader->DownloadFBX(FBXURL);
}

void UT2AAnimationSubsystem::OnDownloadComplete(const FString& LocalFilePath, const FString& ErrorMsg)
{
	if (!ErrorMsg.IsEmpty() || LocalFilePath.IsEmpty())
	{
		FailPipeline(FString::Printf(TEXT("Download failed: %s"), *ErrorMsg));
		return;
	}

	UE_LOG(LogT2A, Log, TEXT("FBX downloaded to: %s"), *LocalFilePath);
	StartImporting(LocalFilePath);
}

void UT2AAnimationSubsystem::StartImporting(const FString& LocalFilePath)
{
	CurrentStage = ET2APipelineStage::Importing;
	OnPipelineProgress.Broadcast(CurrentStage, 0.65f, TEXT("Importing animation from FBX..."));
	OnPipelineProgressNative.Broadcast(CurrentStage, 0.65f, TEXT("Importing animation from FBX..."));

	Importer->ImportFBXAnimationAsync(LocalFilePath);
}

void UT2AAnimationSubsystem::OnImportComplete(UAnimSequence* Animation, USkeleton* Skeleton, const FString& ErrorMsg)
{
	if (!ErrorMsg.IsEmpty() || !Animation)
	{
		FailPipeline(FString::Printf(TEXT("Import failed: %s"), *ErrorMsg));
		return;
	}

	UE_LOG(LogT2A, Log, TEXT("Animation imported successfully"));
	StartRetargeting(Animation, Skeleton);
}

void UT2AAnimationSubsystem::StartRetargeting(UAnimSequence* SourceAnim, USkeleton* SourceSkeleton)
{
	// Check if retargeting is needed
	if (!CurrentConfig.TargetCharacter)
	{
		// No target character specified - return source animation as-is
		UE_LOG(LogT2A, Log, TEXT("No target character, skipping retarget"));
		FinishPipeline(SourceAnim, CurrentRewrittenPrompt);
		return;
	}

	USkeletalMesh* TargetMesh = CurrentConfig.TargetCharacter->GetSkeletalMeshAsset();
	if (!TargetMesh)
	{
		UE_LOG(LogT2A, Warning, TEXT("Target character has no skeletal mesh, skipping retarget"));
		FinishPipeline(SourceAnim, CurrentRewrittenPrompt);
		return;
	}

	USkeleton* TargetSkeleton = TargetMesh->GetSkeleton();
	if (!TargetSkeleton)
	{
		UE_LOG(LogT2A, Warning, TEXT("Target mesh has no skeleton, skipping retarget"));
		FinishPipeline(SourceAnim, CurrentRewrittenPrompt);
		return;
	}

	// Check compatibility
	if (URuntimeIKRetargeter::AreSkeletonsCompatible(SourceSkeleton, TargetSkeleton))
	{
		UE_LOG(LogT2A, Log, TEXT("Skeletons compatible, skipping retarget"));
		FinishPipeline(SourceAnim, CurrentRewrittenPrompt);
		return;
	}

	CurrentStage = ET2APipelineStage::Retargeting;
	OnPipelineProgress.Broadcast(CurrentStage, 0.8f, TEXT("Retargeting animation..."));
	OnPipelineProgressNative.Broadcast(CurrentStage, 0.8f, TEXT("Retargeting animation..."));

	UAnimSequence* RetargetedAnim = Retargeter->RetargetAnimation(
		SourceAnim,
		SourceSkeleton,
		TargetMesh,
		CurrentConfig.RetargetAsset);

	if (!RetargetedAnim)
	{
		UE_LOG(LogT2A, Warning, TEXT("Retargeting failed, using source animation"));
		FinishPipeline(SourceAnim, CurrentRewrittenPrompt);
		return;
	}

	UE_LOG(LogT2A, Log, TEXT("Animation retargeted successfully"));
	FinishPipeline(RetargetedAnim, CurrentRewrittenPrompt);
}

void UT2AAnimationSubsystem::ApplyAnimation(UAnimSequence* FinalAnim)
{
	if (!CurrentConfig.TargetCharacter || !FinalAnim) return;

	CurrentConfig.TargetCharacter->PlayAnimation(FinalAnim, CurrentConfig.bLooping);
	UE_LOG(LogT2A, Log, TEXT("Animation applied to target character (looping: %s)"), 
		CurrentConfig.bLooping ? TEXT("true") : TEXT("false"));
}

void UT2AAnimationSubsystem::FinishPipeline(UAnimSequence* FinalAnim, const FString& Prompt)
{
	CurrentStage = ET2APipelineStage::Completed;
	
	// Auto-apply if configured
	if (CurrentConfig.bAutoPlay && CurrentConfig.TargetCharacter && FinalAnim)
	{
		CurrentStage = ET2APipelineStage::Applying;
		OnPipelineProgress.Broadcast(CurrentStage, 0.95f, TEXT("Applying animation to character..."));
		OnPipelineProgressNative.Broadcast(CurrentStage, 0.95f, TEXT("Applying animation to character..."));
		ApplyAnimation(FinalAnim);
	}

	CurrentStage = ET2APipelineStage::Completed;
	OnPipelineProgress.Broadcast(CurrentStage, 1.0f, TEXT("Done!"));
	OnPipelineProgressNative.Broadcast(CurrentStage, 1.0f, TEXT("Done!"));
	OnPipelineCompleted.Broadcast(FinalAnim, Prompt);
	OnPipelineCompletedNative.Broadcast(FinalAnim, Prompt);

	UE_LOG(LogT2A, Log, TEXT("T2A Pipeline completed successfully!"));
}

void UT2AAnimationSubsystem::FailPipeline(const FString& Error)
{
	ET2APipelineStage FailedStage = CurrentStage;
	CurrentStage = ET2APipelineStage::Failed;

	UE_LOG(LogT2A, Error, TEXT("T2A Pipeline failed at stage %d: %s"), (int32)FailedStage, *Error);
	OnPipelineFailed.Broadcast(FailedStage, Error);
	OnPipelineFailedNative.Broadcast(FailedStage, Error);
}
