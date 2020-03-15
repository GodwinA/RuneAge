// Copyright (c) 2006-2012 Audiokinetic Inc. / All Rights Reserved

/*=============================================================================
	AkSpotReflector.h:
=============================================================================*/
#pragma once

#include "AkAudioDevice.h"
#include "AkAcousticTexture.h"
#include "GameFramework/Actor.h"
#include "AkSpotReflector.generated.h"

UCLASS(config = Engine)
class AKAUDIO_API AAkSpotReflector : public AActor
{
	GENERATED_UCLASS_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AkSpotReflector)
	class UAkAuxBus * EarlyReflectionAuxBus;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = AkSpotReflector)
	FString EarlyReflectionAuxBusName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AkSpotReflector)
	UAkAcousticTexture* AcousticTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AkSpotReflector, meta = (ClampMin = "0.0"))
	float DistanceScalingFactor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AkSpotReflector, meta = (ClampMin = "0.0", ClampMax = "4.0"))
	float Level;

	AkImageSourceID GetImageSourceID() const;
	AkAuxBusID GetAuxBusID() const;

	virtual void PostInitializeComponents() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Call to set all spot reflectors in the world for a single this ak component.
	static void SetSpotReflectors(UAkComponent* AkComponent);

private:
	void SetImageSource(UAkComponent* AkComponent);
	void AddToWorld();
	void RemoveFromWorld();

#if WITH_EDITORONLY_DATA
	/** Editor only component used to display the sprite so as to be able to see the location of the Spot Reflector  */
	class UBillboardComponent* SpriteComponent;
#endif

	typedef TSet<AAkSpotReflector*> SpotReflectorSet;
	typedef TMap<UWorld*, SpotReflectorSet> WorldToSpotReflectorsMap;
	static WorldToSpotReflectorsMap sWorldToSpotReflectors;
};
