// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "AkPluginActivatorCommandlet.generated.h"

/**
 * 
 */
UCLASS()
class AUDIOKINETICTOOLS_API UAkPluginActivatorCommandlet : public UCommandlet
{
	GENERATED_BODY()
public:

	UAkPluginActivatorCommandlet();

	// UCommandlet interface
	virtual int32 Main(const FString& Params) override;

private:
	FString GetTargetFilePath(const FString& BaseDir, const FString& TargetName, const FString& Platform, const FString& Configuration);
	void PrintHelp() const;
};
