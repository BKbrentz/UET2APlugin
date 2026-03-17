// Copyright UET2A Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FBXDownloader.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDownloadProgress, float, Progress, int32, BytesReceived);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDownloadCompleted, const FString&, LocalFilePath, const FString&, ErrorMsg);

/**
 * Async FBX file downloader.
 * Downloads FBX files from COS URLs to local temp directory.
 * Fully runtime-compatible.
 */
UCLASS(BlueprintType)
class UET2APLUGIN_API UFBXDownloader : public UObject
{
	GENERATED_BODY()

public:
	/** Download an FBX file from URL to local temp directory */
	UFUNCTION(BlueprintCallable, Category = "T2A|Download")
	void DownloadFBX(const FString& URL, const FString& OptionalFileName = TEXT(""));

	/** Cancel current download */
	UFUNCTION(BlueprintCallable, Category = "T2A|Download")
	void CancelDownload();

	/** Check if download is in progress */
	UFUNCTION(BlueprintPure, Category = "T2A|Download")
	bool IsDownloading() const { return bIsDownloading; }

	/** Fired during download with progress */
	UPROPERTY(BlueprintAssignable, Category = "T2A|Download")
	FOnDownloadProgress OnDownloadProgress;

	/** Fired when download completes (or fails) */
	UPROPERTY(BlueprintAssignable, Category = "T2A|Download")
	FOnDownloadCompleted OnDownloadCompleted;

	/** Get the default download directory */
	UFUNCTION(BlueprintPure, Category = "T2A|Download")
	static FString GetDownloadDirectory();

	/** Clean up all downloaded temp files */
	UFUNCTION(BlueprintCallable, Category = "T2A|Download")
	static void CleanupTempFiles();

private:
	void OnHttpProgress(TSharedPtr<class IHttpRequest, ESPMode::ThreadSafe> HttpRequest, uint64 BytesSent, uint64 BytesReceived);
	void OnHttpComplete(TSharedPtr<class IHttpRequest, ESPMode::ThreadSafe> HttpRequest, TSharedPtr<class IHttpResponse, ESPMode::ThreadSafe> HttpResponse, bool bSucceeded);

	TSharedPtr<class IHttpRequest, ESPMode::ThreadSafe> CurrentRequest;
	FString TargetFilePath;
	bool bIsDownloading = false;
};
