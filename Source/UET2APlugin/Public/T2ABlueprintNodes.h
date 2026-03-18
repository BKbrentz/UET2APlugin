// Copyright UET2A Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "T2AAnimationSubsystem.h"
#include "T2ABlueprintNodes.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_SixParams(FOnMotionGenEvent, ET2APipelineStage, Stage, float, Percent, const FString&, StatusMessage, UAnimSequence*, ImportedAnimation, const FString&, RewrittenPrompt, const FString&, ErrorMessage);

/**
 * Blueprint async node: Generate Motion from Text.
 *
 * One node does the streamlined pipeline:
 * Text Prompt → Hunyuan API → Download FBX → Import
 *
 * The underlying class name is kept for backward compatibility, but the node no
 * longer performs auto-retargeting or auto-play.
 */
UCLASS(meta=(BlueprintInternalUseOnly="true"))
class UET2APLUGIN_API UGenerateAndApplyMotionAsync : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	/**
	 * Generate animation from text and return the imported FBX animation.
	 *
	 * @param WorldContextObject  World context
	 * @param TextPrompt          Text description of the desired animation
	 * @param Duration            Animation duration in seconds (1-12)
	 */
	UFUNCTION(BlueprintCallable, meta=(BlueprintInternalUseOnly="true", WorldContext="WorldContextObject", DisplayName="Generate Motion from Text"), Category="T2A")
	static UGenerateAndApplyMotionAsync* GenerateMotionFromText(
		UObject* WorldContextObject,
		const FString& TextPrompt,
		int32 Duration = 5);

	// ==================== Output Pins ====================

	/** All async outputs share one signature so Blueprint pins stay consistent across events */
	UPROPERTY(BlueprintAssignable)
	FOnMotionGenEvent OnProgress;

	/** Fired when animation generation and import completes */
	UPROPERTY(BlueprintAssignable)
	FOnMotionGenEvent OnCompleted;

	/** Fired if any stage of the pipeline fails */
	UPROPERTY(BlueprintAssignable)
	FOnMotionGenEvent OnFailed;

	// UBlueprintAsyncActionBase
	virtual void Activate() override;

private:
	UFUNCTION()
	void HandleProgress(ET2APipelineStage Stage, float Progress, const FString& StatusMessage);
	UFUNCTION()
	void HandleCompleted(UAnimSequence* ImportedAnimation, const FString& RewrittenPrompt);
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
