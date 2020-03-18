// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#include "AkGeometryComponentDetailsCustomization.h"
#include "AkGeometryComponent.h"
#include "DetailLayoutBuilder.h"

#define LOCTEXT_NAMESPACE "AudiokineticTools"


//////////////////////////////////////////////////////////////////////////
// FAkGeometryDetailsCustomization

FAkGeometryComponentDetailsCustomization::FAkGeometryComponentDetailsCustomization()
{
	ComponentBeingCustomized = nullptr;
}

FAkGeometryComponentDetailsCustomization::~FAkGeometryComponentDetailsCustomization()
{
	if (ComponentBeingCustomized && ComponentBeingCustomized->IsValidLowLevelFast() && ComponentBeingCustomized->GetOnMeshTypeChanged())
	{
		if (ComponentBeingCustomized->GetOnMeshTypeChanged()->IsBoundToObject(this))
		{
			ComponentBeingCustomized->ClearOnMeshTypeChanged();
		}
		ComponentBeingCustomized = nullptr;
	}
}

TSharedRef<IDetailCustomization> FAkGeometryComponentDetailsCustomization::MakeInstance()
{
	return MakeShared<FAkGeometryComponentDetailsCustomization>();
}

void FAkGeometryComponentDetailsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	MyDetailLayout = &DetailLayout;
	TArray<TWeakObjectPtr<UObject>> ObjectsBeingCustomized;
	DetailLayout.GetObjectsBeingCustomized(ObjectsBeingCustomized);
	if (ObjectsBeingCustomized.Num() != 1)
	{
		return;
	}

	ComponentBeingCustomized = Cast<UAkGeometryComponent>(ObjectsBeingCustomized[0].Get());
	if (ComponentBeingCustomized)
	{
		auto meshTypeChangedHandle = MyDetailLayout->GetProperty("MeshType");
		meshTypeChangedHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FAkGeometryComponentDetailsCustomization::RefreshDetails));

		FOnMeshTypeChanged meshTypeChanged = FOnMeshTypeChanged::CreateRaw(this, &FAkGeometryComponentDetailsCustomization::RefreshDetails);
		ComponentBeingCustomized->SetOnMeshTypeChanged(meshTypeChanged);

		if (ComponentBeingCustomized->MeshType == AkMeshType::StaticMesh)
		{
			DetailLayout.HideProperty("CollisionMeshSurfaceOverride");
		}
		else if (ComponentBeingCustomized->MeshType == AkMeshType::CollisionMesh)
		{
			DetailLayout.HideProperty("LOD");
			DetailLayout.HideProperty("StaticMeshSurfaceOverride");
		}
	}
}

void FAkGeometryComponentDetailsCustomization::RefreshDetails()
{
	if (ComponentBeingCustomized)
		ComponentBeingCustomized->ClearOnMeshTypeChanged();
	if (MyDetailLayout)
		MyDetailLayout->ForceRefreshDetails();
}

//////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE