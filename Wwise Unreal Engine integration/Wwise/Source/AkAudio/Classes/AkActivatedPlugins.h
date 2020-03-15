#pragma once

#include "Engine/EngineTypes.h"
#include "AkInclude.h"
#include "AkActivatedPlugins.generated.h"

USTRUCT()
struct FAkPluginList
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = "Plug-in Activation")
	TArray<FString> PluginNames;
};

UCLASS(config = Game, defaultconfig)
class AKAUDIO_API UAkActivatedPlugins : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	UPROPERTY(Config, EditAnywhere, Category = "Plug-in Activation")
	TMap<FString, FAkPluginList> Platforms;

	static const FString& GetPackageName();
	static const FString& GetFilePath();
};
