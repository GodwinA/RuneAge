// Copyright (c) 2006-2012 Audiokinetic Inc. / All Rights Reserved

#include "AkAudioModule.h"
#include "AkAudioDevice.h"
#include "AkAudioStyle.h"
#include "AkWaapiClient.h"
#include "Misc/CoreDelegates.h"

IMPLEMENT_MODULE( FAkAudioModule, AkAudio )
FAkAudioModule* FAkAudioModule::AkAudioModuleIntance = nullptr;

void FAkAudioModule::StartupModule()
{
	if (AkAudioModuleIntance)
		return;

	AkAudioModuleIntance = this;
	AkAudioDevice = new FAkAudioDevice;
	if (!AkAudioDevice)
		return;

	FAkWaapiClient::Initialize();
	if (!AkAudioDevice->Init())
	{
		delete AkAudioDevice;
		AkAudioDevice = nullptr;
		return;
	}

	OnTick = FTickerDelegate::CreateRaw(AkAudioDevice, &FAkAudioDevice::Update);
	TickDelegateHandle = FTicker::GetCoreTicker().AddTicker(OnTick);
}

void FAkAudioModule::ShutdownModule()
{
	FAkAudioStyle::Shutdown();

	FTicker::GetCoreTicker().RemoveTicker(TickDelegateHandle);

	if (AkAudioDevice) 
	{
		AkAudioDevice->Teardown();
		delete AkAudioDevice;
		AkAudioDevice = nullptr;
	}

	FAkWaapiClient::DeleteInstance();

	AkAudioModuleIntance = nullptr;
}

FAkAudioDevice* FAkAudioModule::GetAkAudioDevice()
{ 
	return AkAudioDevice;
}
