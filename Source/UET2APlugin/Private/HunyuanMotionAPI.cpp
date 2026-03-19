// Copyright UET2A Team. All Rights Reserved.

#include "HunyuanMotionAPI.h"
#include "UET2APlugin.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Containers/Ticker.h"

namespace
{
	void AddUniqueFileUrl(TArray<FString>& Files, const FString& FileUrl)
	{
		const FString TrimmedUrl = FileUrl.TrimStartAndEnd();
		if (!TrimmedUrl.IsEmpty())
		{
			Files.AddUnique(TrimmedUrl);
		}
	}

	void AppendDirectFileUrlFields(const TSharedPtr<FJsonObject>& JsonObj, TArray<FString>& Files)
	{
		if (!JsonObj.IsValid())
		{
			return;
		}

		static const TCHAR* CandidateFields[] =
		{
			TEXT("url"),
			TEXT("file_url"),
			TEXT("download_url"),
			TEXT("output_url"),
			TEXT("cos_url"),
			TEXT("fbx_url"),
			TEXT("retarget_fbx_url")
		};

		for (const TCHAR* FieldName : CandidateFields)
		{
			FString FileUrl;
			if (JsonObj->TryGetStringField(FieldName, FileUrl))
			{
				AddUniqueFileUrl(Files, FileUrl);
			}
		}
	}

	void AppendFileUrlsFromValue(const TSharedPtr<FJsonValue>& FileVal, TArray<FString>& Files)
	{
		if (!FileVal.IsValid())
		{
			return;
		}

		FString FileUrl;
		if (FileVal->TryGetString(FileUrl))
		{
			AddUniqueFileUrl(Files, FileUrl);
			return;
		}

		const TSharedPtr<FJsonObject>* FileObj;
		if (FileVal->TryGetObject(FileObj))
		{
			AppendDirectFileUrlFields(*FileObj, Files);
		}
	}

	void AppendFileUrlsFromArrayField(const TSharedPtr<FJsonObject>& JsonObj, const TCHAR* FieldName, TArray<FString>& Files)
	{
		if (!JsonObj.IsValid())
		{
			return;
		}

		const TArray<TSharedPtr<FJsonValue>>* FilesArray;
		if (JsonObj->TryGetArrayField(FieldName, FilesArray))
		{
			for (const TSharedPtr<FJsonValue>& FileVal : *FilesArray)
			{
				AppendFileUrlsFromValue(FileVal, Files);
			}
		}
	}

	int32 CountDownloadableFiles(const FHunyuanMotionResult& Result)
	{
		int32 TotalFiles = 0;
		for (const FHunyuanMotionResultEntry& Entry : Result.MotionEntries)
		{
			TotalFiles += Entry.Files.Num();
		}

		return TotalFiles;
	}

	FString BuildEntryFileSummary(const FHunyuanMotionResult& Result)
	{
		TArray<FString> Parts;
		for (int32 Index = 0; Index < Result.MotionEntries.Num(); ++Index)
		{
			Parts.Add(FString::Printf(TEXT("#%d:%d"), Index, Result.MotionEntries[Index].Files.Num()));
		}

		return Parts.Num() > 0 ? FString::Join(Parts, TEXT(", ")) : TEXT("none");
	}
}

UHunyuanMotionAPI::UHunyuanMotionAPI()
{
}

// ==================== Configuration ====================

void UHunyuanMotionAPI::SetAPIKey(const FString& InAPIKey)
{
	APIKey = InAPIKey;
}

void UHunyuanMotionAPI::SetBaseURL(const FString& InBaseURL)
{
	BaseURL = InBaseURL;
}

// ==================== HTTP Helpers ====================

TSharedRef<IHttpRequest, ESPMode::ThreadSafe> UHunyuanMotionAPI::CreateBaseRequest(const FString& Endpoint, const FString& Verb)
{
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(BaseURL + Endpoint);
	HttpRequest->SetVerb(Verb);
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	
	if (!APIKey.IsEmpty())
	{
		HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *APIKey));
	}
	
	return HttpRequest;
}

TSharedPtr<FJsonObject> UHunyuanMotionAPI::BuildRequestBody(const FHunyuanMotionRequest& Request) const
{
	TSharedPtr<FJsonObject> JsonObj = MakeShareable(new FJsonObject);

	JsonObj->SetStringField(TEXT("model"), Request.Model);
	JsonObj->SetStringField(TEXT("text_prompt"), Request.TextPrompt);
	JsonObj->SetNumberField(TEXT("duration"), Request.Duration);

	if (!Request.Seeds.IsEmpty())
	{
		JsonObj->SetStringField(TEXT("seeds"), Request.Seeds);
	}

	JsonObj->SetStringField(TEXT("type"), Request.OutputType);
	JsonObj->SetBoolField(TEXT("mesh"), Request.bMesh);

	if (Request.bDisableRewrite)
	{
		JsonObj->SetBoolField(TEXT("disable_rewrite"), true);
	}

	if (Request.bDisableDurationEstimate)
	{
		JsonObj->SetBoolField(TEXT("disable_duration_est"), true);
	}

	return JsonObj;
}

FHunyuanMotionResult UHunyuanMotionAPI::ParseMotionResult(TSharedPtr<FJsonObject> JsonObj) const
{
	FHunyuanMotionResult Result;

	if (!JsonObj.IsValid()) return Result;

	const TSharedPtr<FJsonObject>* DataObj;
	if (!JsonObj->TryGetObjectField(TEXT("data"), DataObj)) return Result;

	// Parse task_id
	(*DataObj)->TryGetStringField(TEXT("task_id"), Result.TaskId);

	// Parse status
	FString StatusStr;
	if ((*DataObj)->TryGetStringField(TEXT("status"), StatusStr))
	{
		if (StatusStr == TEXT("pending")) Result.Status = EHunyuanTaskStatus::Pending;
		else if (StatusStr == TEXT("processing")) Result.Status = EHunyuanTaskStatus::Processing;
		else if (StatusStr == TEXT("success")) Result.Status = EHunyuanTaskStatus::Success;
		else if (StatusStr == TEXT("failed")) Result.Status = EHunyuanTaskStatus::Failed;
	}

	// Parse result.motion.text array
	const TSharedPtr<FJsonObject>* ResultObj;
	if ((*DataObj)->TryGetObjectField(TEXT("result"), ResultObj))
	{
		const TSharedPtr<FJsonObject>* MotionObj;
		if ((*ResultObj)->TryGetObjectField(TEXT("motion"), MotionObj))
		{
			const TArray<TSharedPtr<FJsonValue>>* TextArray;
			if ((*MotionObj)->TryGetArrayField(TEXT("text"), TextArray))
			{
				for (const TSharedPtr<FJsonValue>& EntryVal : *TextArray)
				{
					const TSharedPtr<FJsonObject>* EntryObj;
					if (EntryVal->TryGetObject(EntryObj))
					{
						FHunyuanMotionResultEntry Entry;
						(*EntryObj)->TryGetNumberField(TEXT("duration"), Entry.Duration);
						(*EntryObj)->TryGetNumberField(TEXT("num"), Entry.Num);
						(*EntryObj)->TryGetStringField(TEXT("prompt"), Entry.Prompt);
						(*EntryObj)->TryGetStringField(TEXT("type"), Entry.Type);

						AppendFileUrlsFromArrayField(*EntryObj, TEXT("fbx_files"), Entry.Files);
						AppendFileUrlsFromArrayField(*EntryObj, TEXT("retarget_fbx_files"), Entry.Files);
						AppendFileUrlsFromArrayField(*EntryObj, TEXT("files"), Entry.Files);
						AppendDirectFileUrlFields(*EntryObj, Entry.Files);
						Result.MotionEntries.Add(Entry);
					}
				}
			}
		}
	}

	return Result;
}

FHunyuanAPIError UHunyuanMotionAPI::ParseError(int32 HttpStatus, TSharedPtr<FJsonObject> JsonObj) const
{
	FHunyuanAPIError Error;
	Error.HttpStatusCode = HttpStatus;

	if (JsonObj.IsValid())
	{
		JsonObj->TryGetNumberField(TEXT("error_code"), Error.BusinessErrorCode);
		JsonObj->TryGetStringField(TEXT("error_msg"), Error.Message);
		
		// Also try nested error object
		const TSharedPtr<FJsonObject>* ErrorObj;
		if (JsonObj->TryGetObjectField(TEXT("error"), ErrorObj))
		{
			(*ErrorObj)->TryGetNumberField(TEXT("code"), Error.BusinessErrorCode);
			(*ErrorObj)->TryGetStringField(TEXT("message"), Error.Message);
		}
	}

	return Error;
}

// ==================== Async API ====================

void UHunyuanMotionAPI::SubmitMotionTask(const FHunyuanMotionRequest& Request)
{
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = CreateBaseRequest(TEXT("/openapi/v1/mllm/motion/generations/submission"));

	TSharedPtr<FJsonObject> Body = BuildRequestBody(Request);
	FString BodyStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&BodyStr);
	FJsonSerializer::Serialize(Body.ToSharedRef(), Writer);
	HttpRequest->SetContentAsString(BodyStr);

	HttpRequest->OnProcessRequestComplete().BindUObject(this, &UHunyuanMotionAPI::OnSubmitResponse);
	HttpRequest->SetTimeout(10.0f);
	
	UE_LOG(LogT2A, Log, TEXT("Submitting motion task: %s"), *Request.TextPrompt);
	HttpRequest->ProcessRequest();
}

void UHunyuanMotionAPI::OnSubmitResponse(TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> HttpRequest, TSharedPtr<IHttpResponse, ESPMode::ThreadSafe> HttpResponse, bool bSucceeded)
{
	if (!bSucceeded || !HttpResponse.IsValid())
	{
		UE_LOG(LogT2A, Error, TEXT("Submit request failed - no response"));
		OnTaskSubmitted.Broadcast(TEXT(""), TEXT("Network request failed"));
		return;
	}

	int32 ResponseCode = HttpResponse->GetResponseCode();
	FString ResponseStr = HttpResponse->GetContentAsString();

	UE_LOG(LogT2A, Verbose, TEXT("Submit response [%d]: %s"), ResponseCode, *ResponseStr);

	TSharedPtr<FJsonObject> JsonObj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseStr);
	if (!FJsonSerializer::Deserialize(Reader, JsonObj) || !JsonObj.IsValid())
	{
		OnTaskSubmitted.Broadcast(TEXT(""), TEXT("Failed to parse API response"));
		return;
	}

	if (ResponseCode != 200)
	{
		FHunyuanAPIError Error = ParseError(ResponseCode, JsonObj);
		OnTaskSubmitted.Broadcast(TEXT(""), Error.GetFriendlyMessage());
		return;
	}

	// Extract task_id from data
	FString TaskId;
	const TSharedPtr<FJsonObject>* DataObj;
	if (JsonObj->TryGetObjectField(TEXT("data"), DataObj))
	{
		(*DataObj)->TryGetStringField(TEXT("task_id"), TaskId);
	}

	if (TaskId.IsEmpty())
	{
		OnTaskSubmitted.Broadcast(TEXT(""), TEXT("No task_id in response"));
		return;
	}

	UE_LOG(LogT2A, Log, TEXT("Task submitted successfully: %s"), *TaskId);
	OnTaskSubmitted.Broadcast(TaskId, TEXT(""));
}

// ==================== Polling ====================

void UHunyuanMotionAPI::StartPollingTask(const FString& TaskId)
{
	if (bIsPolling)
	{
		StopPolling();
	}

	CurrentPollingTaskId = TaskId;
	PollCount = 0;
	SuccessWithoutFilesCount = 0;
	PollingStartTime = FPlatformTime::Seconds();
	SuccessWithoutFilesStartTime = 0.0;
	bIsPolling = true;

	UE_LOG(LogT2A, Log, TEXT("Starting poll for task: %s"), *TaskId);
	ScheduleNextPoll(GetNextPollDelay());
}

void UHunyuanMotionAPI::ScheduleNextPoll(float DelaySeconds)
{
	if (!bIsPolling)
	{
		return;
	}

	if (PollingTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(PollingTickerHandle);
		PollingTickerHandle.Reset();
	}

	PollingTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateLambda([this](float DeltaTime) -> bool
		{
			PollingTickerHandle.Reset();
			if (!bIsPolling)
			{
				return false;
			}

			PollTick();
			return false;
		}),
		DelaySeconds
	);
}

float UHunyuanMotionAPI::GetNextPollDelay() const
{
	if (PollCount == 0)
	{
		return FirstPollDelay;
	}

	const float ElapsedSeconds = static_cast<float>(FPlatformTime::Seconds() - PollingStartTime);
	if (ElapsedSeconds < FastPollWindowStart)
	{
		return FMath::Max(0.1f, FastPollWindowStart - ElapsedSeconds);
	}

	if (ElapsedSeconds < FastPollWindowEnd)
	{
		return FastPollInterval;
	}

	return SlowPollInterval;
}

void UHunyuanMotionAPI::StopPolling()
{
	if (PollingTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(PollingTickerHandle);
		PollingTickerHandle.Reset();
	}
	bIsPolling = false;
	CurrentPollingTaskId.Empty();
	PollCount = 0;
	SuccessWithoutFilesCount = 0;
	PollingStartTime = 0.0;
	SuccessWithoutFilesStartTime = 0.0;
}

void UHunyuanMotionAPI::PollTick()
{
	if (!bIsPolling || CurrentPollingTaskId.IsEmpty()) return;

	PollCount++;
	if (PollCount > MaxPollCount)
	{
		UE_LOG(LogT2A, Error, TEXT("Polling timeout after %d attempts"), PollCount);
		StopPolling();

		FHunyuanMotionResult TimeoutResult;
		TimeoutResult.Status = EHunyuanTaskStatus::Failed;
		TimeoutResult.ErrorMessage = TEXT("Generation timed out");
		OnTaskCompleted.Broadcast(TimeoutResult, TEXT("Generation timed out"));
		return;
	}

	QueryTaskStatus(CurrentPollingTaskId);
	ScheduleNextPoll(GetNextPollDelay());
}

void UHunyuanMotionAPI::QueryTaskStatus(const FString& TaskId)
{
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = CreateBaseRequest(TEXT("/openapi/v1/mllm/motion/generations/task"));

	TSharedPtr<FJsonObject> Body = MakeShareable(new FJsonObject);
	Body->SetStringField(TEXT("task_id"), TaskId);

	FString BodyStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&BodyStr);
	FJsonSerializer::Serialize(Body.ToSharedRef(), Writer);
	HttpRequest->SetContentAsString(BodyStr);

	HttpRequest->OnProcessRequestComplete().BindUObject(this, &UHunyuanMotionAPI::OnQueryResponse);
	HttpRequest->SetTimeout(30.0f);

	HttpRequest->ProcessRequest();
}

void UHunyuanMotionAPI::OnQueryResponse(TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> HttpRequest, TSharedPtr<IHttpResponse, ESPMode::ThreadSafe> HttpResponse, bool bSucceeded)
{
	if (!bSucceeded || !HttpResponse.IsValid())
	{
		UE_LOG(LogT2A, Warning, TEXT("Query request failed - will retry"));
		OnTaskProgress.Broadcast(EHunyuanTaskStatus::Unknown, TEXT("Network error - retrying..."));
		return;
	}

	int32 ResponseCode = HttpResponse->GetResponseCode();
	FString ResponseStr = HttpResponse->GetContentAsString();

	UE_LOG(LogT2A, Verbose, TEXT("Query response [%d]: %s"), ResponseCode, *ResponseStr);

	TSharedPtr<FJsonObject> JsonObj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseStr);
	if (!FJsonSerializer::Deserialize(Reader, JsonObj) || !JsonObj.IsValid())
	{
		UE_LOG(LogT2A, Warning, TEXT("Failed to parse query response"));
		return;
	}

	if (ResponseCode != 200)
	{
		FHunyuanAPIError Error = ParseError(ResponseCode, JsonObj);
		
		// Rate limit: just keep polling
		if (Error.IsRateLimitError())
		{
			UE_LOG(LogT2A, Warning, TEXT("Rate limited, will retry"));
			OnTaskProgress.Broadcast(EHunyuanTaskStatus::Processing, TEXT("Rate limited - waiting..."));
			return;
		}

		// Other errors: stop and report
		StopPolling();
		FHunyuanMotionResult ErrorResult;
		ErrorResult.Status = EHunyuanTaskStatus::Failed;
		ErrorResult.ErrorMessage = Error.GetFriendlyMessage();
		ErrorResult.ErrorCode = Error.BusinessErrorCode;
		OnTaskCompleted.Broadcast(ErrorResult, Error.GetFriendlyMessage());
		return;
	}

	FHunyuanMotionResult Result = ParseMotionResult(JsonObj);

	switch (Result.Status)
	{
	case EHunyuanTaskStatus::Pending:
		SuccessWithoutFilesCount = 0;
		SuccessWithoutFilesStartTime = 0.0;
		OnTaskProgress.Broadcast(EHunyuanTaskStatus::Pending, TEXT("Queued - waiting for generation..."));
		break;

	case EHunyuanTaskStatus::Processing:
		SuccessWithoutFilesCount = 0;
		SuccessWithoutFilesStartTime = 0.0;
		OnTaskProgress.Broadcast(EHunyuanTaskStatus::Processing, TEXT("Generating animation..."));
		break;

	case EHunyuanTaskStatus::Success:
	{
		const int32 TotalFiles = CountDownloadableFiles(Result);
		if (TotalFiles == 0)
		{
			const double CurrentTime = FPlatformTime::Seconds();
			if (SuccessWithoutFilesStartTime <= 0.0)
			{
				SuccessWithoutFilesStartTime = CurrentTime;
			}

			SuccessWithoutFilesCount++;
			const double WaitedSeconds = CurrentTime - SuccessWithoutFilesStartTime;
			const bool bReachedAttemptLimit = SuccessWithoutFilesCount >= MaxSuccessWithoutFilesPolls;
			const bool bReachedTimeLimit = WaitedSeconds >= MaxSuccessWithoutFilesWaitSeconds;

			UE_LOG(LogT2A, Warning, TEXT("Task reported success but no downloadable FBX files are ready yet (attempt %d/%d, waited %.1fs/%.1fs, entries=%d, files per entry=%s)"),
				SuccessWithoutFilesCount,
				MaxSuccessWithoutFilesPolls,
				WaitedSeconds,
				MaxSuccessWithoutFilesWaitSeconds,
				Result.MotionEntries.Num(),
				*BuildEntryFileSummary(Result));
			UE_LOG(LogT2A, Verbose, TEXT("Success-without-files payload: %s"), *ResponseStr);

			if (bReachedAttemptLimit || bReachedTimeLimit)
			{
				UE_LOG(LogT2A, Warning, TEXT("Success-without-files final payload: %s"), *ResponseStr);
				StopPolling();
				FHunyuanMotionResult ErrorResult = Result;
				ErrorResult.Status = EHunyuanTaskStatus::Failed;
				ErrorResult.ErrorMessage = TEXT("Task succeeded but no FBX files became available in time.");
				OnTaskCompleted.Broadcast(ErrorResult, ErrorResult.ErrorMessage);
			}
			else
			{
				OnTaskProgress.Broadcast(EHunyuanTaskStatus::Processing, TEXT("Generation finished, waiting for FBX files..."));
			}
			break;
		}

		SuccessWithoutFilesCount = 0;
		SuccessWithoutFilesStartTime = 0.0;
		UE_LOG(LogT2A, Log, TEXT("Task completed successfully! %d motion entries, %d downloadable files"), Result.MotionEntries.Num(), TotalFiles);
		StopPolling();
		OnTaskCompleted.Broadcast(Result, TEXT(""));
		break;
	}

	case EHunyuanTaskStatus::Failed:
		UE_LOG(LogT2A, Error, TEXT("Task failed: %s"), *Result.ErrorMessage);
		StopPolling();
		OnTaskCompleted.Broadcast(Result, Result.ErrorMessage.IsEmpty() ? TEXT("Generation failed") : Result.ErrorMessage);
		break;

	default:
		OnTaskProgress.Broadcast(EHunyuanTaskStatus::Unknown, TEXT("Unknown status..."));
		break;
	}
}

// ==================== Sync API ====================

void UHunyuanMotionAPI::GenerateMotionSync(const FHunyuanMotionRequest& Request)
{
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = CreateBaseRequest(TEXT("/openapi/v1/mllm/motion/generations"));

	TSharedPtr<FJsonObject> Body = BuildRequestBody(Request);
	FString BodyStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&BodyStr);
	FJsonSerializer::Serialize(Body.ToSharedRef(), Writer);
	HttpRequest->SetContentAsString(BodyStr);

	HttpRequest->OnProcessRequestComplete().BindUObject(this, &UHunyuanMotionAPI::OnSyncResponse);
	HttpRequest->SetTimeout(1000.0f); // Sync API can take up to 1000s

	UE_LOG(LogT2A, Log, TEXT("Starting sync motion generation: %s"), *Request.TextPrompt);
	HttpRequest->ProcessRequest();
}

void UHunyuanMotionAPI::OnSyncResponse(TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> HttpRequest, TSharedPtr<IHttpResponse, ESPMode::ThreadSafe> HttpResponse, bool bSucceeded)
{
	if (!bSucceeded || !HttpResponse.IsValid())
	{
		FHunyuanMotionResult ErrorResult;
		ErrorResult.Status = EHunyuanTaskStatus::Failed;
		ErrorResult.ErrorMessage = TEXT("Network request failed");
		OnTaskCompleted.Broadcast(ErrorResult, TEXT("Network request failed"));
		return;
	}

	int32 ResponseCode = HttpResponse->GetResponseCode();
	FString ResponseStr = HttpResponse->GetContentAsString();

	TSharedPtr<FJsonObject> JsonObj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseStr);
	if (!FJsonSerializer::Deserialize(Reader, JsonObj) || !JsonObj.IsValid())
	{
		FHunyuanMotionResult ErrorResult;
		ErrorResult.Status = EHunyuanTaskStatus::Failed;
		ErrorResult.ErrorMessage = TEXT("Failed to parse response");
		OnTaskCompleted.Broadcast(ErrorResult, TEXT("Failed to parse response"));
		return;
	}

	if (ResponseCode != 200)
	{
		FHunyuanAPIError Error = ParseError(ResponseCode, JsonObj);
		FHunyuanMotionResult ErrorResult;
		ErrorResult.Status = EHunyuanTaskStatus::Failed;
		ErrorResult.ErrorMessage = Error.GetFriendlyMessage();
		ErrorResult.ErrorCode = Error.BusinessErrorCode;
		OnTaskCompleted.Broadcast(ErrorResult, Error.GetFriendlyMessage());
		return;
	}

	FHunyuanMotionResult Result = ParseMotionResult(JsonObj);
	Result.Status = EHunyuanTaskStatus::Success;
	
	UE_LOG(LogT2A, Log, TEXT("Sync generation completed! %d motion entries"), Result.MotionEntries.Num());
	OnTaskCompleted.Broadcast(Result, TEXT(""));
}
