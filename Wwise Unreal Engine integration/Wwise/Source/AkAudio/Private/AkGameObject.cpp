// Copyright (c) 2006-2012 Audiokinetic Inc. / All Rights Reserved

/*=============================================================================
	AkGameObject.cpp:
=============================================================================*/

#include "AkGameObject.h"
#include "AkAudioEvent.h"
#include "AkComponentCallbackManager.h"


UAkGameObject::UAkGameObject(const class FObjectInitializer& ObjectInitializer) :
Super(ObjectInitializer)
{
	bStarted = false;
}

int32 UAkGameObject::PostAssociatedAkEvent(int32 CallbackMask, const FOnAkPostEventCallback& PostEventCallback, const TArray<FAkExternalSourceInfo>& ExternalSources)
{
	return PostAkEvent(AkAudioEvent, CallbackMask, PostEventCallback, ExternalSources, EventName);
}

int32 UAkGameObject::PostAssociatedAkEvent(int32 CallbackMask, const FOnAkPostEventCallback& PostEventCallback)
{
	return PostAkEvent(AkAudioEvent, CallbackMask, PostEventCallback, TArray<FAkExternalSourceInfo>(), EventName);
}

int32 UAkGameObject::PostAkEvent(class UAkAudioEvent * AkEvent, int32 CallbackMask, const FOnAkPostEventCallback& PostEventCallback, const TArray<FAkExternalSourceInfo>& ExternalSources, const FString& in_EventName)
{
	return PostAkEventByNameWithDelegate(GET_AK_EVENT_NAME(AkEvent, in_EventName), CallbackMask, PostEventCallback, ExternalSources);
}

int32 UAkGameObject::PostAkEvent(class UAkAudioEvent * AkEvent, int32 CallbackMask, const FOnAkPostEventCallback& PostEventCallback, const FString& in_EventName)
{
	return PostAkEventByNameWithDelegate(GET_AK_EVENT_NAME(AkEvent, in_EventName), CallbackMask, PostEventCallback, TArray<FAkExternalSourceInfo>());
}

AkPlayingID UAkGameObject::PostAkEventByNameWithDelegate(const FString& in_EventName, int32 CallbackMask, const FOnAkPostEventCallback& PostEventCallback, const TArray<FAkExternalSourceInfo>& ExternalSources)
{
	AkPlayingID playingID = AK_INVALID_PLAYING_ID;

	auto AudioDevice = FAkAudioDevice::Get();
	if (AudioDevice)
	{
		if (ExternalSources.Num() > 0)
		{
			FAkSDKExtrernalSourceArray SDKExternalSrcInfo(ExternalSources);
			playingID = AudioDevice->PostEvent(in_EventName, this, PostEventCallback, CallbackMask, SDKExternalSrcInfo.ExternalSourceArray);
		}
		else
		{
			playingID = AudioDevice->PostEvent(in_EventName, this, PostEventCallback, CallbackMask);
		}
		if (playingID != AK_INVALID_PLAYING_ID)
			bStarted = true;
	}

	return playingID;
}

bool UAkGameObject::VerifyEventName(const FString& in_EventName) const
{
	const bool IsEventNameEmpty = in_EventName.IsEmpty();
	if (IsEventNameEmpty)
	{
		FString OwnerName = FString(TEXT(""));
		FString ObjectName = GetFName().ToString();

		const auto owner = GetOwner();
		if (owner)
			OwnerName = owner->GetName();

		UE_LOG(LogAkAudio, Warning, TEXT("[%s.%s] AkGameObject: Attempted to post an empty AkEvent name."), *OwnerName, *ObjectName);
	}

	return !IsEventNameEmpty;
}

bool UAkGameObject::AllowAudioPlayback() const
{
	UWorld* CurrentWorld = GetWorld();
	return (CurrentWorld && CurrentWorld->AllowAudioPlayback() && !IsBeingDestroyed());
}

AkGameObjectID UAkGameObject::GetAkGameObjectID() const
{
	return (AkGameObjectID)this;
}

void UAkGameObject::Stop()
{
	if (FAkAudioDevice::Get() && IsRegisteredWithWwise)
	{
		AK::SoundEngine::StopAll(GetAkGameObjectID());
	}
}

bool UAkGameObject::HasActiveEvents() const
{
	auto CallbackManager = FAkComponentCallbackManager::GetInstance();
	return (CallbackManager != nullptr) && CallbackManager->HasActiveEvents(GetAkGameObjectID());
}
