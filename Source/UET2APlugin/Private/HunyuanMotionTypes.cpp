// Copyright UET2A Team. All Rights Reserved.

#include "HunyuanMotionTypes.h"

FString FHunyuanAPIError::GetFriendlyMessage() const
{
	// HTTP-level errors
	switch (HttpStatusCode)
	{
	case 400: return TEXT("Bad Request: Invalid parameters.");
	case 401: return TEXT("Authentication failed: Invalid API Key.");
	case 422: return TEXT("Unprocessable entity: Check request format.");
	case 429: return TEXT("Rate limited: Too many requests. Please wait.");
	case 500: return TEXT("Server error: The API service is temporarily unavailable.");
	default: break;
	}

	// Business-level errors
	switch (BusinessErrorCode)
	{
	case -101: return TEXT("Text prompt is empty.");
	case -102: return TEXT("Text prompt is too long.");
	case -103: return TEXT("Text prompt contains invalid content.");
	case -201: return TEXT("Invalid duration (must be 1-12 seconds).");
	case -301: return TEXT("Invalid seed value.");
	case -302: return TEXT("Seed value out of range.");
	case -401: return TEXT("Unsupported output format.");
	case -501: return TEXT("Unsupported model.");
	case -502: return TEXT("Model is currently unavailable.");
	case -601: return TEXT("FBX generation failed on server side.");
	default: break;
	}

	if (!Message.IsEmpty())
	{
		return Message;
	}

	return FString::Printf(TEXT("Unknown error (HTTP: %d, Code: %d)"), HttpStatusCode, BusinessErrorCode);
}
