// Copyright UET2A Team. All Rights Reserved.

#include "T2AEditorImportRunner.h"
#include "UET2APlugin.h"
#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
#include "FBXDownloader.h"
#include "HunyuanMotionAPI.h"
#include "RuntimeFBXImporter.h"

namespace
{
const TCHAR* DefaultImportFolder = TEXT("/Game/HunyuanMotion/Imported");
}

void UT2AEditorImportRunner::SetAPIKey(const FString& InAPIKey)
{
	APIKey = InAPIKey.TrimStartAndEnd();
	EnsureComponents();
	API->SetAPIKey(APIKey);
}

void UT2AEditorImportRunner::RunImport(const FString& InTextPrompt, int32 InDuration, USkeletalMesh* InTargetSkeletalMesh, const FString& InOutputAssetFolder)
{
	Cancel();
	ResetComponents();
	EnsureComponents();

	TextPrompt = InTextPrompt.TrimStartAndEnd();
	Duration = FMath::Clamp(InDuration, 1, 12);
	OutputAssetFolder = InOutputAssetFolder.TrimStartAndEnd();
	if (OutputAssetFolder.IsEmpty())
	{
		OutputAssetFolder = DefaultImportFolder;
	}

	TargetSkeletalMesh = InTargetSkeletalMesh;
	CurrentRewrittenPrompt.Empty();
	LastCompletionSummary.Empty();
	bCancellationRequested = false;
	bIsRunning = true;
	CurrentStage = ET2APipelineStage::Idle;

	API->SetAPIKey(APIKey);

	if (APIKey.IsEmpty())
	{
		FailImport(TEXT("API Key is not configured."));
		return;
	}

	if (TextPrompt.IsEmpty())
	{
		FailImport(TEXT("Animation prompt is empty."));
		return;
	}

	UE_LOG(LogT2A, Log, TEXT("Starting editor T2A import: \"%s\" -> %s"), *TextPrompt, *OutputAssetFolder);
	StartSubmitting();
}

void UT2AEditorImportRunner::Cancel()
{
	bCancellationRequested = true;
	bIsRunning = false;
	CurrentStage = ET2APipelineStage::Idle;

	if (API)
	{
		API->StopPolling();
	}

	if (Downloader)
	{
		Downloader->CancelDownload();
	}
}

void UT2AEditorImportRunner::BeginDestroy()
{
	Cancel();
	ResetComponents();
	Super::BeginDestroy();
}

void UT2AEditorImportRunner::EnsureComponents()
{
	if (!API)
	{
		API = NewObject<UHunyuanMotionAPI>(this);
		API->OnTaskSubmitted.AddDynamic(this, &UT2AEditorImportRunner::OnTaskSubmitted);
		API->OnTaskCompleted.AddDynamic(this, &UT2AEditorImportRunner::OnTaskCompleted);
		API->OnTaskProgress.AddDynamic(this, &UT2AEditorImportRunner::OnTaskProgress);
	}

	if (!Downloader)
	{
		Downloader = NewObject<UFBXDownloader>(this);
		Downloader->OnDownloadCompleted.AddDynamic(this, &UT2AEditorImportRunner::OnDownloadComplete);
	}

	if (!Importer)
	{
		Importer = NewObject<URuntimeFBXImporter>(this);
		Importer->OnImportCompleted.AddDynamic(this, &UT2AEditorImportRunner::OnImportComplete);
	}
}

void UT2AEditorImportRunner::ResetComponents()
{
	if (API)
	{
		API->StopPolling();
		API->OnTaskSubmitted.RemoveDynamic(this, &UT2AEditorImportRunner::OnTaskSubmitted);
		API->OnTaskCompleted.RemoveDynamic(this, &UT2AEditorImportRunner::OnTaskCompleted);
		API->OnTaskProgress.RemoveDynamic(this, &UT2AEditorImportRunner::OnTaskProgress);
		API = nullptr;
	}

	if (Downloader)
	{
		Downloader->CancelDownload();
		Downloader->OnDownloadCompleted.RemoveDynamic(this, &UT2AEditorImportRunner::OnDownloadComplete);
		Downloader = nullptr;
	}

	if (Importer)
	{
		Importer->OnImportCompleted.RemoveDynamic(this, &UT2AEditorImportRunner::OnImportComplete);
		Importer = nullptr;
	}
}

void UT2AEditorImportRunner::StartSubmitting()
{
	CurrentStage = ET2APipelineStage::Submitting;
	BroadcastProgress(CurrentStage, 0.0f, TEXT("Submitting to Hunyuan API..."));

	FHunyuanMotionRequest Request;
	Request.TextPrompt = TextPrompt;
	Request.Duration = Duration;
	Request.bDisableDurationEstimate = true;

	API->SubmitMotionTask(Request);
}

void UT2AEditorImportRunner::StartPolling(const FString& TaskId)
{
	CurrentStage = ET2APipelineStage::Polling;
	BroadcastProgress(CurrentStage, 0.1f, TEXT("Waiting for animation generation..."));
	API->StartPollingTask(TaskId);
}

void UT2AEditorImportRunner::StartDownloading(const FString& FBXURL)
{
	CurrentStage = ET2APipelineStage::Downloading;
	BroadcastProgress(CurrentStage, 0.5f, TEXT("Downloading FBX file..."));
	Downloader->DownloadFBX(FBXURL);
}

void UT2AEditorImportRunner::StartImporting(const FString& LocalFilePath)
{
	CurrentStage = ET2APipelineStage::Importing;

	const FString ImportStatus = FString::Printf(TEXT("Importing animation asset to %s..."), *OutputAssetFolder);
	BroadcastProgress(CurrentStage, 0.65f, ImportStatus);

	if (USkeletalMesh* ResolvedTargetMesh = TargetSkeletalMesh.Get())
	{
		UE_LOG(LogT2A, Log, TEXT("Editor import will reuse target skeletal mesh: %s"), *ResolvedTargetMesh->GetPathName());
	}

	Importer->ImportFBXAnimationAsyncToFolder(LocalFilePath, OutputAssetFolder, TargetSkeletalMesh.Get());
}

void UT2AEditorImportRunner::FinishImport(UAnimSequence* ImportedAnim, const FString& Prompt)
{
	bIsRunning = false;
	CurrentStage = ET2APipelineStage::Completed;

	if (LastCompletionSummary.IsEmpty())
	{
		LastCompletionSummary = TEXT("Done! Animation asset imported successfully.");
	}

	BroadcastProgress(CurrentStage, 1.0f, LastCompletionSummary);
	OnCompleted.Broadcast(ImportedAnim, Prompt);
	UE_LOG(LogT2A, Log, TEXT("Editor T2A import completed successfully. %s"), *LastCompletionSummary);
}

void UT2AEditorImportRunner::FailImport(const FString& Error)
{
	if (bCancellationRequested)
	{
		return;
	}

	bIsRunning = false;
	const ET2APipelineStage FailedStage = CurrentStage == ET2APipelineStage::Idle ? ET2APipelineStage::Failed : CurrentStage;
	CurrentStage = ET2APipelineStage::Failed;

	UE_LOG(LogT2A, Error, TEXT("Editor T2A import failed at stage %d: %s"), static_cast<int32>(FailedStage), *Error);
	OnFailed.Broadcast(FailedStage, Error);
}

void UT2AEditorImportRunner::BroadcastProgress(ET2APipelineStage Stage, float Progress, const FString& StatusMessage)
{
	OnProgress.Broadcast(Stage, Progress, StatusMessage);
}

void UT2AEditorImportRunner::OnTaskSubmitted(const FString& TaskId, const FString& ErrorMsg)
{
	if (!bIsRunning || bCancellationRequested)
	{
		return;
	}

	if (!ErrorMsg.IsEmpty())
	{
		FailImport(FString::Printf(TEXT("Task submission failed: %s"), *ErrorMsg));
		return;
	}

	UE_LOG(LogT2A, Log, TEXT("Editor import task submitted: %s"), *TaskId);
	StartPolling(TaskId);
}

void UT2AEditorImportRunner::OnTaskCompleted(const FHunyuanMotionResult& Result, const FString& ErrorMsg)
{
	if (!bIsRunning || bCancellationRequested)
	{
		return;
	}

	if (!ErrorMsg.IsEmpty() || Result.Status == EHunyuanTaskStatus::Failed)
	{
		FailImport(FString::Printf(TEXT("Generation failed: %s"), *(!ErrorMsg.IsEmpty() ? ErrorMsg : Result.ErrorMessage)));
		return;
	}

	if (Result.MotionEntries.Num() == 0 || Result.MotionEntries[0].Files.Num() == 0)
	{
		FailImport(TEXT("No FBX files in API response."));
		return;
	}

	if (!Result.MotionEntries[0].Prompt.IsEmpty())
	{
		CurrentRewrittenPrompt = Result.MotionEntries[0].Prompt;
	}

	const FString FBXURL = Result.MotionEntries[0].Files[0];
	UE_LOG(LogT2A, Log, TEXT("Editor import got FBX URL: %s"), *FBXURL);
	StartDownloading(FBXURL);
}

void UT2AEditorImportRunner::OnTaskProgress(EHunyuanTaskStatus Status, const FString& StatusMessage)
{
	if (!bIsRunning || bCancellationRequested)
	{
		return;
	}

	float Progress = 0.1f;
	switch (Status)
	{
	case EHunyuanTaskStatus::Pending:
		Progress = 0.15f;
		break;
	case EHunyuanTaskStatus::Processing:
		Progress = 0.3f;
		break;
	default:
		break;
	}

	BroadcastProgress(CurrentStage, Progress, StatusMessage);
}

void UT2AEditorImportRunner::OnDownloadComplete(const FString& LocalFilePath, const FString& ErrorMsg)
{
	if (!bIsRunning || bCancellationRequested)
	{
		return;
	}

	if (!ErrorMsg.IsEmpty() || LocalFilePath.IsEmpty())
	{
		FailImport(FString::Printf(TEXT("Download failed: %s"), *ErrorMsg));
		return;
	}

	UE_LOG(LogT2A, Log, TEXT("Editor import downloaded FBX to: %s"), *LocalFilePath);
	StartImporting(LocalFilePath);
}

void UT2AEditorImportRunner::OnImportComplete(UAnimSequence* Animation, USkeleton* Skeleton, const FString& ErrorMsg)
{
	if (!bIsRunning || bCancellationRequested)
	{
		return;
	}

	if (!ErrorMsg.IsEmpty() || !Animation || !Skeleton)
	{
		FailImport(FString::Printf(TEXT("Import failed: %s"), *ErrorMsg));
		return;
	}

	const bool bSavedToContent = Animation->GetPathName().StartsWith(TEXT("/Game/"));
	if (bSavedToContent)
	{
		LastCompletionSummary = FString::Printf(TEXT("Done! Animation generated and imported to %s"), *Animation->GetPathName());
	}
	else
	{
		LastCompletionSummary = TEXT("Done! Animation imported successfully, but asset save was unavailable so a transient object was returned.");
	}

	const FString CompletionPrompt = CurrentRewrittenPrompt.IsEmpty() ? TextPrompt : CurrentRewrittenPrompt;
	FinishImport(Animation, CompletionPrompt);
}
