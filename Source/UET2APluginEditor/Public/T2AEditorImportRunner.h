// Copyright UET2A Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "T2AAnimationSubsystem.h"
#include "T2AEditorImportRunner.generated.h"

class UAnimSequence;
class UFBXDownloader;
class UHunyuanMotionAPI;
class URuntimeFBXImporter;
class USkeletalMesh;
class USkeleton;

DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnT2AEditorImportProgress, ET2APipelineStage, float, const FString&);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnT2AEditorImportCompleted, UAnimSequence*, const FString&);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnT2AEditorImportFailed, ET2APipelineStage, const FString&);

UCLASS()
class UET2APLUGINEDITOR_API UT2AEditorImportRunner : public UObject
{
	GENERATED_BODY()

public:
	void SetAPIKey(const FString& InAPIKey);
	FString GetAPIKey() const { return APIKey; }

	void RunImport(const FString& InTextPrompt, int32 InDuration, USkeletalMesh* InTargetSkeletalMesh, const FString& InOutputAssetFolder);
	void Cancel();

	bool IsRunning() const { return bIsRunning; }
	FString GetLastCompletionSummary() const { return LastCompletionSummary; }

	FOnT2AEditorImportProgress OnProgress;
	FOnT2AEditorImportCompleted OnCompleted;
	FOnT2AEditorImportFailed OnFailed;

protected:
	virtual void BeginDestroy() override;

private:
	void EnsureComponents();
	void ResetComponents();
	void StartSubmitting();
	void StartPolling(const FString& TaskId);
	void StartDownloading(const FString& FBXURL);
	void StartImporting(const FString& LocalFilePath);
	void FinishImport(UAnimSequence* ImportedAnim, const FString& Prompt);
	void FailImport(const FString& Error);
	void BroadcastProgress(ET2APipelineStage Stage, float Progress, const FString& StatusMessage);

	UFUNCTION()
	void OnTaskSubmitted(const FString& TaskId, const FString& ErrorMsg);

	UFUNCTION()
	void OnTaskCompleted(const FHunyuanMotionResult& Result, const FString& ErrorMsg);

	UFUNCTION()
	void OnTaskProgress(EHunyuanTaskStatus Status, const FString& StatusMessage);

	UFUNCTION()
	void OnDownloadComplete(const FString& LocalFilePath, const FString& ErrorMsg);

	UFUNCTION()
	void OnImportComplete(UAnimSequence* Animation, USkeleton* Skeleton, const FString& ErrorMsg);

	UPROPERTY(Transient)
	TObjectPtr<UHunyuanMotionAPI> API = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UFBXDownloader> Downloader = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URuntimeFBXImporter> Importer = nullptr;

	FString APIKey;
	FString TextPrompt;
	FString OutputAssetFolder = TEXT("/Game/HunyuanMotion/Imported");
	TWeakObjectPtr<USkeletalMesh> TargetSkeletalMesh;
	FString CurrentRewrittenPrompt;
	FString LastCompletionSummary;
	ET2APipelineStage CurrentStage = ET2APipelineStage::Idle;
	int32 Duration = 5;
	bool bIsRunning = false;
	bool bCancellationRequested = false;
};
