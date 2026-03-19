// Copyright UET2A Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HunyuanMotionTypes.h"
#include "HunyuanMotionAPI.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMotionTaskSubmitted, const FString&, TaskId, const FString&, ErrorMsg);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMotionTaskCompleted, const FHunyuanMotionResult&, Result, const FString&, ErrorMsg);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMotionTaskProgress, EHunyuanTaskStatus, Status, const FString&, StatusMessage);

/**
 * Core API communication class for Hunyuan Motion API.
 * Fully runtime-compatible - no dependency on UnrealEd.
 * Supports async (submit + poll) and sync API modes.
 */
UCLASS(BlueprintType, Blueprintable)
class UET2APLUGIN_API UHunyuanMotionAPI : public UObject
{
	GENERATED_BODY()

public:
	UHunyuanMotionAPI();

	// ==================== Configuration ====================

	/** Set the API key for authentication */
	UFUNCTION(BlueprintCallable, Category = "T2A|API")
	void SetAPIKey(const FString& InAPIKey);

	/** Set the API base URL (default: http://api.taiji.woa.com) */
	UFUNCTION(BlueprintCallable, Category = "T2A|API")
	void SetBaseURL(const FString& InBaseURL);

	/** Get the configured API key */
	UFUNCTION(BlueprintPure, Category = "T2A|API")
	FString GetAPIKey() const { return APIKey; }

	// ==================== Async API (Recommended) ====================

	/** Submit an async motion generation task. Returns immediately with a task ID. */
	UFUNCTION(BlueprintCallable, Category = "T2A|API")
	void SubmitMotionTask(const FHunyuanMotionRequest& Request);

	/** Start polling for task completion. Call after SubmitMotionTask succeeds. */
	UFUNCTION(BlueprintCallable, Category = "T2A|API")
	void StartPollingTask(const FString& TaskId);

	/** Stop polling for a specific task */
	UFUNCTION(BlueprintCallable, Category = "T2A|API")
	void StopPolling();

	/** Query task status once (manual polling) */
	UFUNCTION(BlueprintCallable, Category = "T2A|API")
	void QueryTaskStatus(const FString& TaskId);

	// ==================== Sync API ====================

	/** Generate motion synchronously (blocking, long timeout - use async instead for runtime) */
	UFUNCTION(BlueprintCallable, Category = "T2A|API")
	void GenerateMotionSync(const FHunyuanMotionRequest& Request);

	// ==================== Delegates ====================

	/** Fired when async task submission completes (with TaskId or error) */
	UPROPERTY(BlueprintAssignable, Category = "T2A|API")
	FOnMotionTaskSubmitted OnTaskSubmitted;

	/** Fired when task generation completes (success or failure) */
	UPROPERTY(BlueprintAssignable, Category = "T2A|API")
	FOnMotionTaskCompleted OnTaskCompleted;

	/** Fired during polling with status updates */
	UPROPERTY(BlueprintAssignable, Category = "T2A|API")
	FOnMotionTaskProgress OnTaskProgress;

	// ==================== State ====================

	/** Check if currently polling */
	UFUNCTION(BlueprintPure, Category = "T2A|API")
	bool IsPolling() const { return bIsPolling; }

private:
	// HTTP helpers
	TSharedRef<class IHttpRequest, ESPMode::ThreadSafe> CreateBaseRequest(const FString& Endpoint, const FString& Verb = TEXT("POST"));
	TSharedPtr<FJsonObject> BuildRequestBody(const FHunyuanMotionRequest& Request) const;
	FHunyuanMotionResult ParseMotionResult(TSharedPtr<FJsonObject> JsonObj) const;
	FHunyuanAPIError ParseError(int32 HttpStatus, TSharedPtr<FJsonObject> JsonObj) const;

	// HTTP callbacks
	void OnSubmitResponse(TSharedPtr<class IHttpRequest, ESPMode::ThreadSafe> HttpRequest, TSharedPtr<class IHttpResponse, ESPMode::ThreadSafe> HttpResponse, bool bSucceeded);
	void OnQueryResponse(TSharedPtr<class IHttpRequest, ESPMode::ThreadSafe> HttpRequest, TSharedPtr<class IHttpResponse, ESPMode::ThreadSafe> HttpResponse, bool bSucceeded);
	void OnSyncResponse(TSharedPtr<class IHttpRequest, ESPMode::ThreadSafe> HttpRequest, TSharedPtr<class IHttpResponse, ESPMode::ThreadSafe> HttpResponse, bool bSucceeded);

	// Polling
	void PollTick();
	void ScheduleNextPoll(float DelaySeconds);
	float GetNextPollDelay() const;
	FTSTicker::FDelegateHandle PollingTickerHandle;

	// Config
	FString APIKey;
	FString BaseURL = TEXT("http://api.taiji.woa.com");
	
	// Polling state
	bool bIsPolling = false;
	FString CurrentPollingTaskId;
	int32 PollCount = 0;
	int32 SuccessWithoutFilesCount = 0;
	double PollingStartTime = 0.0;
	double SuccessWithoutFilesStartTime = 0.0;
	static constexpr int32 MaxPollCount = 500; // ~991s max with current polling schedule
	static constexpr int32 MaxSuccessWithoutFilesPolls = 30;
	static constexpr double MaxSuccessWithoutFilesWaitSeconds = 60.0;
	static constexpr float FirstPollDelay = 2.0f;
	static constexpr float FastPollWindowStart = 5.0f;
	static constexpr float FastPollWindowEnd = 15.0f;
	static constexpr float FastPollInterval = 1.0f;
	static constexpr float SlowPollInterval = 2.0f;
};
