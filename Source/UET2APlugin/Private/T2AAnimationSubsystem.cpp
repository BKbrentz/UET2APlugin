// Copyright UET2A Team. All Rights Reserved.

#include "T2AAnimationSubsystem.h"
#include "UET2APlugin.h"
#include "HunyuanMotionAPI.h"
#include "FBXDownloader.h"
#include "RuntimeFBXImporter.h"
#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
#include "Misc/Paths.h"

// ==================== Lifecycle ====================

void UT2AAnimationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Create pipeline components
	API = NewObject<UHunyuanMotionAPI>(this);
	Downloader = NewObject<UFBXDownloader>(this);
	Importer = NewObject<URuntimeFBXImporter>(this);

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
	LastCompletionSummary.Empty();

	if (CurrentConfig.bSaveImportedAssetsToContent && CurrentConfig.OutputAssetFolder.IsEmpty())
	{
		CurrentConfig.OutputAssetFolder = TEXT("/Game/HunyuanMotion/Imported");
	}

	if (!CurrentConfig.LocalFBXFilePath.IsEmpty())
	{
		CurrentConfig.LocalFBXFilePath = FPaths::ConvertRelativePathToFull(CurrentConfig.LocalFBXFilePath);

		if (!FPaths::FileExists(CurrentConfig.LocalFBXFilePath))
		{
			FailPipeline(FString::Printf(TEXT("Local FBX file does not exist: %s"), *CurrentConfig.LocalFBXFilePath));
			return;
		}

		UE_LOG(LogT2A, Log, TEXT("Starting T2A pipeline from local FBX: %s"), *CurrentConfig.LocalFBXFilePath);
		StartImporting(CurrentConfig.LocalFBXFilePath);
		return;
	}

	if (Config.TextPrompt.IsEmpty())
	{
		FailPipeline(TEXT("Text prompt is empty"));
		return;
	}

	if (!API || API->GetAPIKey().IsEmpty())
	{
		FailPipeline(TEXT("API Key is not configured. Call SetAPIKey() first."));
		return;
	}

	UE_LOG(LogT2A, Log, TEXT("Starting T2A pipeline: \"%s\" (duration: %ds)"), *Config.TextPrompt, Config.Duration);
	StartSubmitting();
}

void UT2AAnimationSubsystem::CancelPipeline()
{
	if (API)
	{
		API->StopPolling();
	}

	if (Downloader)
	{
		Downloader->CancelDownload();
	}

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
	Request.bDisableDurationEstimate = true;

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

	if (Result.MotionEntries.Num() == 0 || Result.MotionEntries[0].Files.Num() == 0)
	{
		FailPipeline(TEXT("No FBX files in API response"));
		return;
	}

	if (Result.MotionEntries[0].Prompt.Len() > 0)
	{
		CurrentRewrittenPrompt = Result.MotionEntries[0].Prompt;
	}

	const FString FBXURL = Result.MotionEntries[0].Files[0];
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

	const bool bSaveToContent = CurrentConfig.bSaveImportedAssetsToContent && !CurrentConfig.OutputAssetFolder.IsEmpty();
	const FString ImportStatus = bSaveToContent
		? FString::Printf(TEXT("Importing animation from FBX to %s..."), *CurrentConfig.OutputAssetFolder)
		: TEXT("Importing animation from FBX...");

	OnPipelineProgress.Broadcast(CurrentStage, 0.65f, ImportStatus);
	OnPipelineProgressNative.Broadcast(CurrentStage, 0.65f, ImportStatus);

	if (CurrentConfig.TargetSkeletalMesh)
	{
		UE_LOG(LogT2A, Log, TEXT("Import will reuse target skeletal mesh: %s"), *CurrentConfig.TargetSkeletalMesh->GetPathName());
	}

	if (bSaveToContent)
	{
		Importer->ImportFBXAnimationAsyncToFolder(LocalFilePath, CurrentConfig.OutputAssetFolder, CurrentConfig.TargetSkeletalMesh);
		return;
	}

	Importer->ImportFBXAnimationAsync(LocalFilePath, CurrentConfig.TargetSkeletalMesh);
}

void UT2AAnimationSubsystem::OnImportComplete(UAnimSequence* Animation, USkeleton* Skeleton, const FString& ErrorMsg)
{
	static_cast<void>(Skeleton);

	if (!ErrorMsg.IsEmpty() || !Animation || !Skeleton)
	{
		FailPipeline(FString::Printf(TEXT("Import failed: %s"), *ErrorMsg));
		return;
	}

	UE_LOG(LogT2A, Log, TEXT("Animation imported successfully"));

	const bool bSavedToContent = Animation->GetPathName().StartsWith(TEXT("/Game/"));
	if (bSavedToContent)
	{
		LastCompletionSummary = CurrentConfig.LocalFBXFilePath.IsEmpty()
			? FString::Printf(TEXT("Done! Animation generated and imported to %s"), *Animation->GetPathName())
			: FString::Printf(TEXT("Done! Local FBX imported to %s"), *Animation->GetPathName());
	}
	else if (CurrentConfig.bSaveImportedAssetsToContent)
	{
		LastCompletionSummary = CurrentConfig.LocalFBXFilePath.IsEmpty()
			? TEXT("Done! Animation imported successfully, but asset save was unavailable so a transient object was returned.")
			: TEXT("Done! Local FBX imported successfully, but asset save was unavailable so a transient object was returned.");
	}
	else
	{
		LastCompletionSummary = CurrentConfig.LocalFBXFilePath.IsEmpty()
			? TEXT("Done! Animation generated, downloaded, and imported successfully.")
			: TEXT("Done! Local FBX imported successfully.");
	}

	const FString CompletionPrompt = CurrentRewrittenPrompt.IsEmpty() ? CurrentConfig.TextPrompt : CurrentRewrittenPrompt;
	FinishPipeline(Animation, CompletionPrompt);
}

void UT2AAnimationSubsystem::FinishPipeline(UAnimSequence* ImportedAnim, const FString& Prompt)
{
	CurrentStage = ET2APipelineStage::Completed;

	if (LastCompletionSummary.IsEmpty())
	{
		LastCompletionSummary = CurrentConfig.LocalFBXFilePath.IsEmpty()
			? TEXT("Done! Animation imported successfully.")
			: TEXT("Done! Local FBX imported successfully.");
	}

	OnPipelineProgress.Broadcast(CurrentStage, 1.0f, LastCompletionSummary);
	OnPipelineProgressNative.Broadcast(CurrentStage, 1.0f, LastCompletionSummary);
	OnPipelineCompleted.Broadcast(ImportedAnim, Prompt);
	OnPipelineCompletedNative.Broadcast(ImportedAnim, Prompt);

	UE_LOG(LogT2A, Log, TEXT("T2A Pipeline completed successfully! %s"), *LastCompletionSummary);
}

void UT2AAnimationSubsystem::FailPipeline(const FString& Error)
{
	const ET2APipelineStage FailedStage = CurrentStage;
	CurrentStage = ET2APipelineStage::Failed;

	UE_LOG(LogT2A, Error, TEXT("T2A Pipeline failed at stage %d: %s"), (int32)FailedStage, *Error);
	OnPipelineFailed.Broadcast(FailedStage, Error);
	OnPipelineFailedNative.Broadcast(FailedStage, Error);
}
