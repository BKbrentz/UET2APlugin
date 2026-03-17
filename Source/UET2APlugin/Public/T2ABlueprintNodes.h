// Copyright UET2A Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "T2AAnimationSubsystem.h"
#include "T2ABlueprintNodes.generated.h"

class UIKRetargeter;
class USkeletalMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnMotionGenProgress, ET2APipelineStage, Stage, float, Percent, const FString&, StatusMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMotionGenCompleted, UAnimSequence*, Animation, const FString&, RewrittenPrompt);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMotionGenFailed, const FString&, ErrorMessage);

/**
 * Blueprint async node: Generate and Apply Motion from Text
 * 
 * One node does the entire pipeline:
 * Text Prompt → Hunyuan API → Download FBX → Import → Retarget → Play
 * 
 * Usage in Blueprint:
 *   1. Connect text prompt and target character
 *   2. Wire up OnProgress / OnCompleted / OnFailed
 *   3. Done!
 */
UCLASS(meta=(BlueprintInternalUseOnly="true"))
class UET2APLUGIN_API UGenerateAndApplyMotionAsync : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	/**
	 * Generate animation from text and optionally apply to a character.
	 * 
	 * @param WorldContextObject  World context
	 * @param TextPrompt          Text description of the desired animation
	 * @param Duration            Animation duration in seconds (1-12)
	 * @param TargetCharacter     Character to apply animation to (optional)
	 * @param RetargetAsset       IKRetargeter asset for bone mapping (optional, auto-maps if null)
	 * @param bAutoPlay           Whether to automatically play on TargetCharacter
	 * @param bLooping            Whether the animation should loop
	 */
	UFUNCTION(BlueprintCallable, meta=(BlueprintInternalUseOnly="true", WorldContext="WorldContextObject", DisplayName="Generate Motion from Text"), Category="T2A")
	static UGenerateAndApplyMotionAsync* GenerateMotionFromText(
		UObject* WorldContextObject,
		const FString& TextPrompt,
		int32 Duration = 5,
		USkeletalMeshComponent* TargetCharacter = nullptr,
		UIKRetargeter* RetargetAsset = nullptr,
		bool bAutoPlay = true,
		bool bLooping = false);

	// ==================== Output Pins ====================

	/** Fired during pipeline execution with progress updates */
	UPROPERTY(BlueprintAssignable)
	FOnMotionGenProgress OnProgress;

	/** Fired when animation generation and retargeting completes */
	UPROPERTY(BlueprintAssignable)
	FOnMotionGenCompleted OnCompleted;

	/** Fired if any stage of the pipeline fails */
	UPROPERTY(BlueprintAssignable)
	FOnMotionGenFailed OnFailed;

	// UBlueprintAsyncActionBase
	virtual void Activate() override;

private:
	UFUNCTION()
	void HandleProgress(ET2APipelineStage Stage, float Progress, const FString& StatusMessage);
	UFUNCTION()
	void HandleCompleted(UAnimSequence* Animation, const FString& RewrittenPrompt);
	UFUNCTION()
	void HandleFailed(ET2APipelineStage FailedStage, const FString& ErrorMessage);

	FT2APipelineConfig PipelineConfig;
	TWeakObjectPtr<UObject> WorldContext;
};

/**
 * Simple blueprint node to configure the T2A API key.
 */
UCLASS()
class UET2APLUGIN_API UT2AConfigNode : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Set the Hunyuan Motion API key for the T2A subsystem */
	UFUNCTION(BlueprintCallable, meta=(WorldContext="WorldContextObject"), Category="T2A|Config")
	static void SetT2AAPIKey(UObject* WorldContextObject, const FString& APIKey);

	/** Set custom API base URL */
	UFUNCTION(BlueprintCallable, meta=(WorldContext="WorldContextObject"), Category="T2A|Config")
	static void SetT2ABaseURL(UObject* WorldContextObject, const FString& BaseURL);

	/** Get the T2A Animation Subsystem */
	UFUNCTION(BlueprintPure, meta=(WorldContext="WorldContextObject"), Category="T2A")
	static UT2AAnimationSubsystem* GetT2ASubsystem(UObject* WorldContextObject);
};
