// Copyright UET2A Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "HunyuanMotionTypes.h"
#include "T2AAnimationSubsystem.generated.h"

class UHunyuanMotionAPI;
class UFBXDownloader;
class URuntimeFBXImporter;
class UAnimSequence;
class USkeleton;

/** Pipeline stage enumeration */
UENUM(BlueprintType)
enum class ET2APipelineStage : uint8
{
	Idle			UMETA(DisplayName = "Idle"),
	Submitting		UMETA(DisplayName = "Submitting to API"),
	Polling			UMETA(DisplayName = "Waiting for Generation"),
	Downloading		UMETA(DisplayName = "Downloading FBX"),
	Importing		UMETA(DisplayName = "Importing Animation"),
	Completed		UMETA(DisplayName = "Completed"),
	Failed			UMETA(DisplayName = "Failed")
};

/** Pipeline task configuration */
USTRUCT(BlueprintType)
struct FT2APipelineConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "T2A")
	FString TextPrompt;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "T2A")
	int32 Duration = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "T2A")
	bool bMesh = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "T2A")
	bool bDisableRewrite = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "T2A")
	FString LocalFBXFilePath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "T2A")
	bool bSaveImportedAssetsToContent = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "T2A")
	FString OutputAssetFolder = TEXT("/Game/HunyuanMotion/Imported");
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnPipelineProgress, ET2APipelineStage, Stage, float, Progress, const FString&, StatusMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPipelineCompleted, UAnimSequence*, Animation, const FString&, RewrittenPrompt);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPipelineFailed, ET2APipelineStage, FailedStage, const FString&, ErrorMessage);

DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnPipelineProgressNative, ET2APipelineStage, float, const FString&);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPipelineCompletedNative, UAnimSequence*, const FString&);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPipelineFailedNative, ET2APipelineStage, const FString&);

/**
 * T2A Animation Pipeline Subsystem.
 * Manages the streamlined Text-to-Animation pipeline:
 * Text → API → Download → Import
 * 
 * Lives on the GameInstance, available throughout the game session.
 */
UCLASS()
class UET2APLUGIN_API UT2AAnimationSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// ==================== Lifecycle ====================
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ==================== Configuration ====================

	/** Set the API key (must be called before running pipeline) */
	UFUNCTION(BlueprintCallable, Category = "T2A|Pipeline")
	void SetAPIKey(const FString& InAPIKey);

	/** Set custom API base URL */
	UFUNCTION(BlueprintCallable, Category = "T2A|Pipeline")
	void SetBaseURL(const FString& InBaseURL);

	// ==================== Pipeline Control ====================

	/** Run the full Text-to-Animation pipeline */
	UFUNCTION(BlueprintCallable, Category = "T2A|Pipeline")
	void RunPipeline(const FT2APipelineConfig& Config);

	/** Cancel the current pipeline execution */
	UFUNCTION(BlueprintCallable, Category = "T2A|Pipeline")
	void CancelPipeline();

	/** Get current pipeline stage */
	UFUNCTION(BlueprintPure, Category = "T2A|Pipeline")
	ET2APipelineStage GetCurrentStage() const { return CurrentStage; }

	/** Check if pipeline is currently running */
	UFUNCTION(BlueprintPure, Category = "T2A|Pipeline")
	bool IsRunning() const { return CurrentStage != ET2APipelineStage::Idle && CurrentStage != ET2APipelineStage::Completed && CurrentStage != ET2APipelineStage::Failed; }

	/** Human-readable summary for the last completed run */
	UFUNCTION(BlueprintPure, Category = "T2A|Pipeline")
	FString GetLastCompletionSummary() const { return LastCompletionSummary; }

	// ==================== Component Access ====================

	/** Get the API handler */
	UFUNCTION(BlueprintPure, Category = "T2A|Pipeline")
	UHunyuanMotionAPI* GetAPI() const { return API; }

	/** Get the FBX importer */
	UFUNCTION(BlueprintPure, Category = "T2A|Pipeline")
	URuntimeFBXImporter* GetImporter() const { return Importer; }

	// ==================== Delegates ====================

	UPROPERTY(BlueprintAssignable, Category = "T2A|Pipeline")
	FOnPipelineProgress OnPipelineProgress;

	UPROPERTY(BlueprintAssignable, Category = "T2A|Pipeline")
	FOnPipelineCompleted OnPipelineCompleted;

	UPROPERTY(BlueprintAssignable, Category = "T2A|Pipeline")
	FOnPipelineFailed OnPipelineFailed;

	FOnPipelineProgressNative OnPipelineProgressNative;
	FOnPipelineCompletedNative OnPipelineCompletedNative;
	FOnPipelineFailedNative OnPipelineFailedNative;

private:
	// Pipeline stage handlers
	void StartSubmitting();
	void StartPolling(const FString& TaskId);
	void StartDownloading(const FString& FBXURL);
	void StartImporting(const FString& LocalFilePath);
	void FinishPipeline(UAnimSequence* ImportedAnim, const FString& Prompt);
	void FailPipeline(const FString& Error);

	// Internal callbacks
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

	// Components
	UPROPERTY()
	UHunyuanMotionAPI* API;
	UPROPERTY()
	UFBXDownloader* Downloader;
	UPROPERTY()
	URuntimeFBXImporter* Importer;

	// Pipeline state
	ET2APipelineStage CurrentStage = ET2APipelineStage::Idle;
	FT2APipelineConfig CurrentConfig;
	FString CurrentRewrittenPrompt;
	FString LastCompletionSummary;
};
