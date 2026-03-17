// Copyright UET2A Team. All Rights Reserved.

#include "FBXDownloader.h"
#include "UET2APlugin.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Guid.h"

FString UFBXDownloader::GetDownloadDirectory()
{
	FString Dir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("HunyuanMotion"), TEXT("Downloads"));
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*Dir))
	{
		PlatformFile.CreateDirectoryTree(*Dir);
	}
	return Dir;
}

void UFBXDownloader::CleanupTempFiles()
{
	FString Dir = GetDownloadDirectory();
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (PlatformFile.DirectoryExists(*Dir))
	{
		PlatformFile.DeleteDirectoryRecursively(*Dir);
		PlatformFile.CreateDirectoryTree(*Dir);
	}
	UE_LOG(LogT2A, Log, TEXT("Cleaned up temp download directory: %s"), *Dir);
}

void UFBXDownloader::DownloadFBX(const FString& URL, const FString& OptionalFileName)
{
	if (bIsDownloading)
	{
		CancelDownload();
	}

	// Generate local file path
	FString FileName = OptionalFileName;
	if (FileName.IsEmpty())
	{
		FileName = FString::Printf(TEXT("motion_%s.fbx"), *FGuid::NewGuid().ToString(EGuidFormats::Short));
	}
	if (!FileName.EndsWith(TEXT(".fbx"), ESearchCase::IgnoreCase))
	{
		FileName += TEXT(".fbx");
	}
	
	TargetFilePath = FPaths::Combine(GetDownloadDirectory(), FileName);
	bIsDownloading = true;

	UE_LOG(LogT2A, Log, TEXT("Starting FBX download: %s -> %s"), *URL, *TargetFilePath);

	CurrentRequest = FHttpModule::Get().CreateRequest();
	CurrentRequest->SetURL(URL);
	CurrentRequest->SetVerb(TEXT("GET"));
	CurrentRequest->OnRequestProgress64().BindUObject(this, &UFBXDownloader::OnHttpProgress);
	CurrentRequest->OnProcessRequestComplete().BindUObject(this, &UFBXDownloader::OnHttpComplete);
	CurrentRequest->SetTimeout(60.0f);
	CurrentRequest->ProcessRequest();
}

void UFBXDownloader::CancelDownload()
{
	if (CurrentRequest.IsValid())
	{
		CurrentRequest->CancelRequest();
		CurrentRequest.Reset();
	}
	bIsDownloading = false;
	TargetFilePath.Empty();
}

void UFBXDownloader::OnHttpProgress(TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> HttpRequest, uint64 BytesSent, uint64 BytesReceived)
{
	static_cast<void>(BytesSent);

	// Estimate progress from content length header if available
	float Progress = 0.0f;
	if (TSharedPtr<IHttpResponse> Response = HttpRequest->GetResponse())
	{
		int32 ContentLength = Response->GetContentLength();
		if (ContentLength > 0)
		{
			Progress = static_cast<float>(static_cast<double>(BytesReceived) / static_cast<double>(ContentLength));
		}
	}

	const int32 SafeBytesReceived = BytesReceived > static_cast<uint64>(MAX_int32)
		? MAX_int32
		: static_cast<int32>(BytesReceived);

	OnDownloadProgress.Broadcast(Progress, SafeBytesReceived);
}

void UFBXDownloader::OnHttpComplete(TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> HttpRequest, TSharedPtr<IHttpResponse, ESPMode::ThreadSafe> HttpResponse, bool bSucceeded)
{
	bIsDownloading = false;
	CurrentRequest.Reset();

	if (!bSucceeded || !HttpResponse.IsValid())
	{
		UE_LOG(LogT2A, Error, TEXT("FBX download failed - no response"));
		OnDownloadCompleted.Broadcast(TEXT(""), TEXT("Download failed: no response"));
		return;
	}

	int32 ResponseCode = HttpResponse->GetResponseCode();
	if (ResponseCode != 200)
	{
		UE_LOG(LogT2A, Error, TEXT("FBX download failed with HTTP %d"), ResponseCode);
		OnDownloadCompleted.Broadcast(TEXT(""), FString::Printf(TEXT("Download failed: HTTP %d"), ResponseCode));
		return;
	}

	const TArray<uint8>& Content = HttpResponse->GetContent();
	if (Content.Num() == 0)
	{
		UE_LOG(LogT2A, Error, TEXT("FBX download returned empty content"));
		OnDownloadCompleted.Broadcast(TEXT(""), TEXT("Download failed: empty response"));
		return;
	}

	// Save to disk
	if (FFileHelper::SaveArrayToFile(Content, *TargetFilePath))
	{
		UE_LOG(LogT2A, Log, TEXT("FBX downloaded successfully: %s (%d bytes)"), *TargetFilePath, Content.Num());
		OnDownloadCompleted.Broadcast(TargetFilePath, TEXT(""));
	}
	else
	{
		UE_LOG(LogT2A, Error, TEXT("Failed to save FBX to: %s"), *TargetFilePath);
		OnDownloadCompleted.Broadcast(TEXT(""), FString::Printf(TEXT("Failed to save file: %s"), *TargetFilePath));
	}
}
