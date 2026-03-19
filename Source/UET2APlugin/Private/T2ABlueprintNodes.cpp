// Copyright UET2A Team. All Rights Reserved.

#include "T2ABlueprintNodes.h"
#include "UET2APlugin.h"
#include "T2AAnimationSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

// ==================== Async Blueprint Node ====================

UGenerateAndApplyMotionAsync* UGenerateAndApplyMotionAsync::GenerateMotionFromText(
	UObject* WorldContextObject,
	const FString& TextPrompt,
	int32 Duration,
	USkeletalMesh* TargetSkeletalMesh)
{
	UGenerateAndApplyMotionAsync* Node = NewObject<UGenerateAndApplyMotionAsync>();
	Node->WorldContext = WorldContextObject;

	Node->PipelineConfig.TextPrompt = TextPrompt;
	Node->PipelineConfig.Duration = FMath::Clamp(Duration, 1, 12);
	Node->PipelineConfig.TargetSkeletalMesh = TargetSkeletalMesh;

	Node->RegisterWithGameInstance(WorldContextObject);

	return Node;
}

void UGenerateAndApplyMotionAsync::Activate()
{
	if (!WorldContext.IsValid())
	{
		OnFailed.Broadcast(
			ET2APipelineStage::Failed,
			0.0f,
			TEXT("Invalid world context"),
			nullptr,
			TEXT(""),
			TEXT("Invalid world context"));
		return;
	}

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContext.Get(), EGetWorldErrorMode::ReturnNull);
	if (!World)
	{
		OnFailed.Broadcast(
			ET2APipelineStage::Failed,
			0.0f,
			TEXT("Cannot get world from context"),
			nullptr,
			TEXT(""),
			TEXT("Cannot get world from context"));
		return;
	}

	UGameInstance* GameInstance = World->GetGameInstance();
	if (!GameInstance)
	{
		OnFailed.Broadcast(
			ET2APipelineStage::Failed,
			0.0f,
			TEXT("No GameInstance available"),
			nullptr,
			TEXT(""),
			TEXT("No GameInstance available"));
		return;
	}

	UT2AAnimationSubsystem* Subsystem = GameInstance->GetSubsystem<UT2AAnimationSubsystem>();
	if (!Subsystem)
	{
		OnFailed.Broadcast(
			ET2APipelineStage::Failed,
			0.0f,
			TEXT("T2A Animation Subsystem not available"),
			nullptr,
			TEXT(""),
			TEXT("T2A Animation Subsystem not available"));
		return;
	}

	// Bind to subsystem delegates
	Subsystem->OnPipelineProgress.AddDynamic(this, &UGenerateAndApplyMotionAsync::HandleProgress);
	Subsystem->OnPipelineCompleted.AddDynamic(this, &UGenerateAndApplyMotionAsync::HandleCompleted);
	Subsystem->OnPipelineFailed.AddDynamic(this, &UGenerateAndApplyMotionAsync::HandleFailed);

	// Run the pipeline
	Subsystem->RunPipeline(PipelineConfig);
}

void UGenerateAndApplyMotionAsync::HandleProgress(ET2APipelineStage Stage, float Progress, const FString& StatusMessage)
{
	OnProgress.Broadcast(Stage, Progress, StatusMessage, nullptr, TEXT(""), TEXT(""));
}

void UGenerateAndApplyMotionAsync::HandleCompleted(UAnimSequence* ImportedAnimation, const FString& RewrittenPrompt)
{
	// Unbind from subsystem
	if (WorldContext.IsValid())
	{
		UWorld* World = GEngine->GetWorldFromContextObject(WorldContext.Get(), EGetWorldErrorMode::ReturnNull);
		if (World)
		{
			UGameInstance* GameInstance = World->GetGameInstance();
			if (GameInstance)
			{
				UT2AAnimationSubsystem* Subsystem = GameInstance->GetSubsystem<UT2AAnimationSubsystem>();
				if (Subsystem)
				{
					Subsystem->OnPipelineProgress.RemoveDynamic(this, &UGenerateAndApplyMotionAsync::HandleProgress);
					Subsystem->OnPipelineCompleted.RemoveDynamic(this, &UGenerateAndApplyMotionAsync::HandleCompleted);
					Subsystem->OnPipelineFailed.RemoveDynamic(this, &UGenerateAndApplyMotionAsync::HandleFailed);
				}
			}
		}
	}

	OnCompleted.Broadcast(
		ET2APipelineStage::Completed,
		1.0f,
		TEXT("Animation imported successfully"),
		ImportedAnimation,
		RewrittenPrompt,
		TEXT(""));
}

void UGenerateAndApplyMotionAsync::HandleFailed(ET2APipelineStage FailedStage, const FString& ErrorMessage)
{
	// Unbind from subsystem
	if (WorldContext.IsValid())
	{
		UWorld* World = GEngine->GetWorldFromContextObject(WorldContext.Get(), EGetWorldErrorMode::ReturnNull);
		if (World)
		{
			UGameInstance* GameInstance = World->GetGameInstance();
			if (GameInstance)
			{
				UT2AAnimationSubsystem* Subsystem = GameInstance->GetSubsystem<UT2AAnimationSubsystem>();
				if (Subsystem)
				{
					Subsystem->OnPipelineProgress.RemoveDynamic(this, &UGenerateAndApplyMotionAsync::HandleProgress);
					Subsystem->OnPipelineCompleted.RemoveDynamic(this, &UGenerateAndApplyMotionAsync::HandleCompleted);
					Subsystem->OnPipelineFailed.RemoveDynamic(this, &UGenerateAndApplyMotionAsync::HandleFailed);
				}
			}
		}
	}

	OnFailed.Broadcast(
		FailedStage,
		1.0f,
		ErrorMessage,
		nullptr,
		TEXT(""),
		ErrorMessage);
}

// ==================== Config Utility Nodes ====================

void UT2AConfigNode::SetT2AAPIKey(UObject* WorldContextObject, const FString& APIKey)
{
	UT2AAnimationSubsystem* Subsystem = GetT2ASubsystem(WorldContextObject);
	if (Subsystem)
	{
		Subsystem->SetAPIKey(APIKey);
	}
}

void UT2AConfigNode::SetT2ABaseURL(UObject* WorldContextObject, const FString& BaseURL)
{
	UT2AAnimationSubsystem* Subsystem = GetT2ASubsystem(WorldContextObject);
	if (Subsystem)
	{
		Subsystem->SetBaseURL(BaseURL);
	}
}

UT2AAnimationSubsystem* UT2AConfigNode::GetT2ASubsystem(UObject* WorldContextObject)
{
	if (!WorldContextObject) return nullptr;

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
	if (!World) return nullptr;

	UGameInstance* GameInstance = World->GetGameInstance();
	if (!GameInstance) return nullptr;

	return GameInstance->GetSubsystem<UT2AAnimationSubsystem>();
}
