#include "AkGeometryComponent.h"
#include "AkAudioDevice.h"
#include "AkAcousticTexture.h"
#include "AkRoomComponent.h"
#include "AkSettings.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Polys.h"
#include "Engine/StaticMesh.h"
#include "PhysXPublic.h"

#include <AK/SpatialAudio/Common/AkSpatialAudio.h>

#if WITH_EDITOR
#include "Editor.h"
#endif

#include "UObject/Object.h"
#include "Engine/GameEngine.h"
#include "PhysicsEngine/BodySetup.h"

static const float kVertexGridSnap = 0.001;
static const float kVertexNear = 0.001;

UAkGeometryComponent::UAkGeometryComponent(const class FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer)
{
	MeshType = AkMeshType::StaticMesh;
	LOD = 0;
	CollisionMeshSurfaceOverride.AcousticTexture = nullptr;
	CollisionMeshSurfaceOverride.bEnableOcclusionOverride = false;
	CollisionMeshSurfaceOverride.OcclusionValue = 1.f;

	bEnableDiffraction = 1;
	bEnableDiffractionOnBoundaryEdges = 0;
#if WITH_EDITOR
	PrimaryComponentTick.bCanEverTick = true;
	bTickInEditor = true;

	OnMeshTypeChangedHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddLambda([this](UObject* Object, FPropertyChangedEvent& PropertyChangedEvent)
	{
		if (GetAttachParent() && MeshType == AkMeshType::StaticMesh)
		{
			UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(GetAttachParent());
			if (StaticMeshComponent != nullptr &&
				StaticMeshComponent == Object &&
				PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UMeshComponent, OverrideMaterials))
			{
				UpdateStaticMeshOverride(StaticMeshComponent);
			}
		}

	});
#endif
}

void UAkGeometryComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	Super::OnComponentDestroyed(bDestroyingHierarchy);
	RemoveGeometry();
	StaticMeshSurfaceOverride.Empty();
#if WITH_EDITOR
	FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(OnMeshTypeChangedHandle);
	OnMeshTypeChangedHandle.Reset();
#endif
}

void UAkGeometryComponent::OnComponentCreated()
{
	Super::OnComponentCreated();

	UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(GetAttachParent());
	if (StaticMeshComponent)
	{
		int numMaterials = StaticMeshComponent->GetNumMaterials();
		for (int i = 0; i < numMaterials; i++)
		{
			UMaterialInterface* material = StaticMeshComponent->GetMaterial(i);
			if (!StaticMeshSurfaceOverride.Contains(material))
				StaticMeshSurfaceOverride.Add(material, FAkGeometrySurfaceOverride());
		}
	}
}

void UAkGeometryComponent::OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport)
{
	Super::OnUpdateTransform(UpdateTransformFlags, Teleport);

	UpdateGeometry();
}

inline bool UAkGeometryComponent::ShouldSendGeometry() const
{
	UWorld* CurrentWorld = GetWorld();
	if (CurrentWorld && !IsRunningCommandlet())
	{
		return CurrentWorld->WorldType == EWorldType::Game || CurrentWorld->WorldType == EWorldType::PIE;
	}
	return false;
}

void UAkGeometryComponent::BeginPlay()
{
	Super::BeginPlay();

#if WITH_EDITOR
	if (GeometryData.Triangles.Num() == 0)
		ConvertMesh();
#endif

	for (int PosIndex = 0; PosIndex < GeometryData.Surfaces.Num(); ++PosIndex)
	{
		// set geometry surface names and update textures
		GeometryData.Surfaces[PosIndex].Name = GetOwner()->GetName() + GetName() + FString::FromInt(PosIndex);

		UPhysicalMaterial* physMat = GeometryData.ToOverrideAcousticTexture[PosIndex];
		if (physMat)
		{
			UAkAcousticTexture* acousticTexture = nullptr;
			if (GetDefault<UAkSettings>()->GetAssociatedAcousticTexture(physMat, acousticTexture))
			{
				if (acousticTexture)
					GeometryData.Surfaces[PosIndex].Texture = FAkAudioDevice::Get()->GetIDFromString(acousticTexture->GetName());
			}
		}

		physMat = GeometryData.ToOverrideOcclusion[PosIndex];
		if (physMat)
		{
			float occlusionValue = 1.f;
			if (GetDefault<UAkSettings>()->GetAssociatedOcclusionValue(physMat, occlusionValue))
			{
				GeometryData.Surfaces[PosIndex].Occlusion = occlusionValue;
			}
		}
	}

	UpdateGeometry();
}

void AddVertsForEdge(const FPositionVertexBuffer& Positions, TArray<int32>& UniqueVerts, int32 P0UnrealIdx, int32 P0UniqueIdx, int32 P1UnrealIdx, int32 P1UniqueIdx, TArray< TPair<int32, float> > & VertsOnEdge)
{
	FVector p0 = Positions.VertexPosition(P0UnrealIdx).GridSnap(kVertexGridSnap);
	FVector p1 = Positions.VertexPosition(P1UnrealIdx).GridSnap(kVertexGridSnap);

	FVector dir;
	float length;
	(p1 - p0).ToDirectionAndLength(dir, length);

	for (int32 i = 0; i < UniqueVerts.Num(); i++)
	{
		int32 UnrealVertIdx = UniqueVerts[i];
		FVector p = Positions.VertexPosition(UnrealVertIdx).GridSnap(kVertexGridSnap);

		float dot = FVector::DotProduct(p - p0, dir);
		if (dot > kVertexNear && dot < length + kVertexNear)
		{
			FVector ptOnLine = p0 + dot * dir;
			FVector diff = ptOnLine - p;
			if (diff.GetAbsMax() < kVertexNear)
			{
				VertsOnEdge.Emplace(i, dot);
			}
		}
	}

	// VertsOnEdge should contain p1 but not p0
	check(VertsOnEdge.Num() > 0);

	VertsOnEdge.Sort([](const TPair<int32, float>& One, const TPair<int32, float>& Two)
	{
		return One.Value < Two.Value;
	});
}

void DetermineVertsToWeld(TArray<int32>& VertRemap, TArray<int32>& UniqueVerts, const FStaticMeshLODResources& RenderMesh)
{
	const int32 VertexCount = RenderMesh.VertexBuffers.PositionVertexBuffer.GetNumVertices();

	// Maps unreal verts to reduced list of verts
	VertRemap.Empty(VertexCount);
	VertRemap.AddUninitialized(VertexCount);

	// List of Unreal Verts to keep
	UniqueVerts.Empty(VertexCount);

	// Combine matching verts using hashed search to maintain good performance
	TMap<FVector, int32> HashedVerts;
	for (int32 a = 0; a < VertexCount; a++)
	{
		FVector PositionA = RenderMesh.VertexBuffers.PositionVertexBuffer.VertexPosition(a).GridSnap(kVertexGridSnap);
		const int32* FoundIndex = HashedVerts.Find(PositionA);
		if (!FoundIndex)
		{
			int32 NewIndex = UniqueVerts.Add(a);
			VertRemap[a] = NewIndex;
			HashedVerts.Add(PositionA, NewIndex);
		}
		else
		{
			VertRemap[a] = *FoundIndex;
		}
	}
}

void UAkGeometryComponent::ConvertMesh()
{
	UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(GetAttachParent());
	if (!StaticMeshComponent || !StaticMeshComponent->GetStaticMesh())
		return;

	const UAkSettings* AkSettings = GetDefault<UAkSettings>();

	switch (MeshType)
	{
		case AkMeshType::StaticMesh:
		{
			ConvertStaticMesh(StaticMeshComponent, AkSettings);
			break;
		}
		case AkMeshType::CollisionMesh:
		{
			ConvertCollisionMesh(StaticMeshComponent, AkSettings);
			break;
		}
	}
}

void UAkGeometryComponent::ConvertStaticMesh(UStaticMeshComponent* StaticMeshComponent, const UAkSettings* AkSettings)
{
	if (LOD > StaticMeshComponent->GetStaticMesh()->GetNumLODs() - 1)
		LOD = StaticMeshComponent->GetStaticMesh()->GetNumLODs() - 1;

	if (!StaticMeshComponent->GetStaticMesh()->RenderData)
		return;

	const FStaticMeshLODResources& RenderMesh = StaticMeshComponent->GetStaticMesh()->GetLODForExport(LOD);
	FIndexArrayView RawIndices = RenderMesh.IndexBuffer.GetArrayView();

	if (RawIndices.Num() == 0)
		return;

	GeometryData.Clear();

	TArray<int32> VertRemap;
	TArray<int32> UniqueVerts;

	DetermineVertsToWeld(VertRemap, UniqueVerts, RenderMesh);

	for (int PosIndex = 0; PosIndex < UniqueVerts.Num(); ++PosIndex)
	{
		int32 UnrealPosIndex = UniqueVerts[PosIndex];
		FVector VertexInActorSpace = RenderMesh.VertexBuffers.PositionVertexBuffer.VertexPosition(UnrealPosIndex);
		GeometryData.Vertices.Add(VertexInActorSpace);
	}

	UpdateStaticMeshOverride(StaticMeshComponent);

	const int32 PolygonsCount = RenderMesh.Sections.Num();
	for (int32 PolygonsIndex = 0; PolygonsIndex < PolygonsCount; ++PolygonsIndex)
	{
		const FStaticMeshSection& Polygons = RenderMesh.Sections[PolygonsIndex];

		FAkAcousticSurface Surface;
		UPhysicalMaterial* physMatTexture = nullptr;
		UPhysicalMaterial* physMatOcclusion = nullptr;
		FAkGeometrySurfaceOverride surfaceOverride;

		UMaterialInterface* Material = StaticMeshComponent->GetMaterial(Polygons.MaterialIndex);
		if (Material)
		{
			UPhysicalMaterial* physicalMaterial = Material->GetPhysicalMaterial();

			surfaceOverride = StaticMeshSurfaceOverride[Material];
			if (!surfaceOverride.AcousticTexture)
				physMatTexture = physicalMaterial;

			if (!surfaceOverride.bEnableOcclusionOverride)
				physMatOcclusion = physicalMaterial;
		}

		if (surfaceOverride.AcousticTexture)
			Surface.Texture = FAkAudioDevice::Get()->GetIDFromString(surfaceOverride.AcousticTexture->GetName());

		if (surfaceOverride.bEnableOcclusionOverride)
			Surface.Occlusion = surfaceOverride.OcclusionValue;

		GeometryData.Surfaces.Add(Surface);
		GeometryData.ToOverrideAcousticTexture.Add(physMatTexture);
		GeometryData.ToOverrideOcclusion.Add(physMatOcclusion);
		AkSurfIdx surfIdx = (AkSurfIdx)(GeometryData.Surfaces.Num() - 1);

		TArray< TPair<int32, float> > Edge0, Edge1, Edge2;
		const uint32 TriangleCount = Polygons.NumTriangles;
		for (uint32 TriangleIndex = 0; TriangleIndex < TriangleCount; ++TriangleIndex)
		{
			uint32 RawVertIndex0 = RawIndices[Polygons.FirstIndex + ((TriangleIndex * 3) + 0)];
			uint32 UniqueVertIndex0 = VertRemap[RawVertIndex0];

			uint32 RawVertIndex1 = RawIndices[Polygons.FirstIndex + ((TriangleIndex * 3) + 1)];
			uint32 UniqueVertIndex1 = VertRemap[RawVertIndex1];

			uint32 RawVertIndex2 = RawIndices[Polygons.FirstIndex + ((TriangleIndex * 3) + 2)];
			uint32 UniqueVertIndex2 = VertRemap[RawVertIndex2];

			Edge0.Empty(8);
			AddVertsForEdge(RenderMesh.VertexBuffers.PositionVertexBuffer, UniqueVerts, RawVertIndex0, UniqueVertIndex0, RawVertIndex1, UniqueVertIndex1, Edge0);

			Edge1.Empty(8);
			AddVertsForEdge(RenderMesh.VertexBuffers.PositionVertexBuffer, UniqueVerts, RawVertIndex1, UniqueVertIndex1, RawVertIndex2, UniqueVertIndex2, Edge1);

			Edge2.Empty(8);
			AddVertsForEdge(RenderMesh.VertexBuffers.PositionVertexBuffer, UniqueVerts, RawVertIndex2, UniqueVertIndex2, RawVertIndex0, UniqueVertIndex0, Edge2);

			FAkTriangle triangle;
			triangle.Surface = surfIdx;

			bool bDone = false;
			do
			{
				int32 v0, v1, v2;

				if (Edge0.Num() > 1)
				{
					v1 = Edge0.Pop().Key;
					v0 = Edge0.Last().Key;
					v2 = Edge1[0].Key;
				}
				else if (Edge1.Num() > 1)
				{
					v1 = Edge1.Pop().Key;
					v0 = Edge1.Last().Key;
					v2 = Edge2[0].Key;
				}
				else if (Edge2.Num() > 1)
				{
					v1 = Edge2.Pop().Key;
					v0 = Edge2.Last().Key;
					v2 = Edge0[0].Key;
				}
				else
				{
					v0 = Edge0[0].Key;
					v1 = Edge1[0].Key;
					v2 = Edge2[0].Key;
					bDone = true;
				}

				triangle.Point0 = (AkVertIdx)v0;
				triangle.Point1 = (AkVertIdx)v1;
				triangle.Point2 = (AkVertIdx)v2;

				if (triangle.Point0 != triangle.Point1 &&
					triangle.Point1 != triangle.Point2 &&
					triangle.Point2 != triangle.Point0)
					GeometryData.Triangles.Add(triangle);
			} while (!bDone);

		}
	}
}

void GetBasicBoxGeometryData(TArray<FVector>& Vertices, TArray<FAkTriangle>& Triangles)
{
	Vertices.Init(FVector(0, 0, 0), 8);
	Vertices[0] = FVector(-1, -1, -1);
	Vertices[1] = FVector(-1, -1, 1);
	Vertices[2] = FVector(-1, 1, -1);
	Vertices[3] = FVector(-1, 1, 1);
	Vertices[4] = FVector(1, -1, -1);
	Vertices[5] = FVector(1, -1, 1);
	Vertices[6] = FVector(1, 1, -1);
	Vertices[7] = FVector(1, 1, 1);

	Triangles.Init(FAkTriangle(), 12);
	Triangles[0] = {0, 1, 3, AK_INVALID_SURFACE};
	Triangles[1] = {0, 1, 5, AK_INVALID_SURFACE};
	Triangles[2] = {0, 2, 3, AK_INVALID_SURFACE};
	Triangles[3] = {0, 2, 6, AK_INVALID_SURFACE};
	Triangles[4] = {0, 4, 5, AK_INVALID_SURFACE};
	Triangles[5] = {0, 4, 6, AK_INVALID_SURFACE};
	Triangles[6] = {1, 3, 7, AK_INVALID_SURFACE};
	Triangles[7] = {1, 5, 7, AK_INVALID_SURFACE};
	Triangles[8] = {2, 3, 7, AK_INVALID_SURFACE};
	Triangles[9] = {2, 6, 7, AK_INVALID_SURFACE};
	Triangles[10] = {4, 5, 7, AK_INVALID_SURFACE};
	Triangles[11] = {4, 6, 7, AK_INVALID_SURFACE};
}

void ConvertBoxElemsToGeometryData(AkSurfIdx surfIdx, FVector center, FVector extent, FRotator rotation, FAkGeometryData* GeometryData)
{
	TArray<FVector> vertices;
	TArray<FAkTriangle> triangles;
	GetBasicBoxGeometryData(vertices, triangles);

	AkVertIdx initialVertIdx = GeometryData->Vertices.Num();

	// move vertices according to the center and extents
	for (AkVertIdx idx = 0; idx < vertices.Num(); idx++)
	{
		FVector v = rotation.RotateVector(vertices[idx] * extent + center);
		
		GeometryData->Vertices.Add(v);
	}

	for (AkTriIdx idx = 0; idx < triangles.Num(); idx++)
	{
		triangles[idx].Point0 += initialVertIdx;
		triangles[idx].Point1 += initialVertIdx;
		triangles[idx].Point2 += initialVertIdx;
		triangles[idx].Surface = surfIdx;

		GeometryData->Triangles.Add(triangles[idx]);
	}
}

struct VertexIndexByAngle
{
	AkVertIdx Index;
	float Angle;
};

bool operator<(const VertexIndexByAngle& lhs, const VertexIndexByAngle& rhs)
{
	return lhs.Angle < rhs.Angle;
}

void UAkGeometryComponent::ConvertCollisionMesh(UStaticMeshComponent* StaticMeshComponent, const UAkSettings* AkSettings)
{
	UBodySetup* bodySetup = StaticMeshComponent->GetStaticMesh()->BodySetup;
	if (!bodySetup)
		return;

	GeometryData.Clear();

	FAkAcousticSurface Surface;
	UPhysicalMaterial* physicalMaterial = bodySetup->GetPhysMaterial();
	UPhysicalMaterial* physMatTexture = nullptr;
	UPhysicalMaterial* physMatOcclusion = nullptr;
	FAkGeometrySurfaceOverride surfaceOverride = CollisionMeshSurfaceOverride;

	if (surfaceOverride.AcousticTexture)
		Surface.Texture = FAkAudioDevice::Get()->GetIDFromString(surfaceOverride.AcousticTexture->GetName());
	else
		physMatTexture = physicalMaterial;

	if (surfaceOverride.bEnableOcclusionOverride)
		Surface.Occlusion = surfaceOverride.OcclusionValue;
	else
		physMatOcclusion = physicalMaterial;
	
	GeometryData.ToOverrideAcousticTexture.Add(physMatTexture);
	GeometryData.ToOverrideOcclusion.Add(physMatOcclusion);

	GeometryData.Surfaces.Add(Surface);
	
	AkSurfIdx surfIdx = (AkSurfIdx)(GeometryData.Surfaces.Num() - 1);

	int32 numBoxes = bodySetup->AggGeom.BoxElems.Num();
	for (int32 i = 0; i < numBoxes; i++)
	{
		FKBoxElem box = bodySetup->AggGeom.BoxElems[i];

		FVector extent;
		extent.X = box.X / 2;
		extent.Y = box.Y / 2;
		extent.Z = box.Z / 2;

		ConvertBoxElemsToGeometryData(surfIdx, box.Center, extent, box.Rotation, &GeometryData);
	}

	int32 numSpheres = bodySetup->AggGeom.SphereElems.Num();
	for (int32 i = 0; i < numSpheres; i++)
	{
		FKSphereElem sphere = bodySetup->AggGeom.SphereElems[i];

		FVector extent;
		extent.X = sphere.Radius;
		extent.Y = sphere.Radius;
		extent.Z = sphere.Radius;

		ConvertBoxElemsToGeometryData(surfIdx, sphere.Center, extent, FRotator(), &GeometryData);
	}

	int32 numCapsules = bodySetup->AggGeom.SphylElems.Num();
	for (int32 i = 0; i < numCapsules; i++)
	{
		FKSphylElem capsule = bodySetup->AggGeom.SphylElems[i];

		FVector extent;
		extent.X = capsule.Radius;
		extent.Y = capsule.Radius;
		extent.Z = capsule.Radius + capsule.Length / 2;

		ConvertBoxElemsToGeometryData(surfIdx, capsule.Center, extent, capsule.Rotation, &GeometryData);
	}

	int32 numComvexHulls = bodySetup->AggGeom.ConvexElems.Num();
	for (int32 i = 0; i < numComvexHulls; i++)
	{
		FKConvexElem convexHull = bodySetup->AggGeom.ConvexElems[i];
		physx::PxConvexMesh* convexMesh = bodySetup->AggGeom.ConvexElems[i].GetConvexMesh();

		AkVertIdx initialVertIdx = GeometryData.Vertices.Num();
		if (convexMesh != nullptr)
		{
			const PxVec3 * vertices = convexMesh->getVertices();

			uint32 numVerts = (uint32)convexMesh->getNbVertices();
			for (uint32 vertIdx = 0; vertIdx < numVerts; ++vertIdx)
			{
				FVector akvtx;
				akvtx.X = vertices[vertIdx].x;
				akvtx.Y = vertices[vertIdx].y;
				akvtx.Z = vertices[vertIdx].z;
				GeometryData.Vertices.Add(akvtx);
			}

			const physx::PxU8* indexBuf = convexMesh->getIndexBuffer();

			uint32 numPolys = (uint32)convexMesh->getNbPolygons();
			for (uint32 polyIdx = 0; polyIdx < numPolys; polyIdx++)
			{
				PxHullPolygon polyData;
				convexMesh->getPolygonData(polyIdx, polyData);

				// order the vertices of the polygon
				uint32 numVertsInPoly = (uint32)polyData.mNbVerts;
				uint32 vertIdxOffset = (uint32)polyData.mIndexBase;

				TArray<VertexIndexByAngle> orderedIndexes;
				// first element is first vertex index
				AkVertIdx firstVertIdx = (AkVertIdx)indexBuf[vertIdxOffset];
				orderedIndexes.Add(VertexIndexByAngle{ firstVertIdx, 0 });

				// get the center of the polygon
				FVector center(0, 0, 0);
				for (uint32 polyVertIdx = 0; polyVertIdx < numVertsInPoly; ++polyVertIdx)
				{
					auto vertIdx = indexBuf[vertIdxOffset + polyVertIdx];
					center.X += vertices[vertIdx].x;
					center.Y += vertices[vertIdx].y;
					center.Z += vertices[vertIdx].z;
				}
				center.X /= numVertsInPoly;
				center.Y /= numVertsInPoly;
				center.Z /= numVertsInPoly;

				// get the vector from center to the first vertex
				FVector v0;
				v0.X = vertices[firstVertIdx].x - center.X;
				v0.Y = vertices[firstVertIdx].y - center.Y;
				v0.Z = vertices[firstVertIdx].z - center.Z;
				v0.Normalize();

				// get the normal of the plane
				FVector n;
				n.X = polyData.mPlane[0];
				n.Y = polyData.mPlane[1];
				n.Z = polyData.mPlane[2];
				n.Normalize();

				// find the angles between v0 and the other vertices of the polygon
				for (uint32 polyVertIdx = 1; polyVertIdx < numVertsInPoly; polyVertIdx++)
				{
					// get the vector from center to the current vertex
					AkVertIdx vertIdx = (AkVertIdx)indexBuf[vertIdxOffset + polyVertIdx];
					FVector v1;
					v1.X = vertices[vertIdx].x - center.X;
					v1.Y = vertices[vertIdx].y - center.Y;
					v1.Z = vertices[vertIdx].z - center.Z;
					v1.Normalize();

					// get the angle between v0 and v1
					// to do so, we need the dot product and the determinant respectively proportional to cos and sin of the angle.
					// atan2(sin, cos) will give us the angle
					float dot = FVector::DotProduct(v0, v1);
					// the determinant of two 3D vectors in the same plane can be found with the dot product of the normal with the result of
					// a cross product between the vectors
					float det = FVector::DotProduct(n, FVector::CrossProduct(v0, v1));
					float angle = (float)atan2(det, dot);

					orderedIndexes.Add(VertexIndexByAngle{ vertIdx, angle });
				}

				orderedIndexes.Sort();

				// fan triangulation
				for (uint32 vertIdx = 1; vertIdx < numVertsInPoly - 1; ++vertIdx)
				{
					FAkTriangle tri;
					tri.Point0 = (AkVertIdx)orderedIndexes[0].Index + initialVertIdx;
					tri.Point1 = (AkVertIdx)orderedIndexes[vertIdx].Index + initialVertIdx;
					tri.Point2 = (AkVertIdx)orderedIndexes[vertIdx + 1].Index + initialVertIdx;
					tri.Surface = surfIdx;

					GeometryData.Triangles.Add(tri);
				}
			}
		}
		else
		{
			// bounding box
			ConvertBoxElemsToGeometryData(surfIdx,
				convexHull.ElemBox.GetCenter(),
				convexHull.ElemBox.GetExtent(),
				convexHull.GetTransform().Rotator(), &GeometryData);
		}
	}
}

void UAkGeometryComponent::SendGeometry()
{
	FAkAudioDevice* AkAudioDevice = FAkAudioDevice::Get();

	if (AkAudioDevice && ShouldSendGeometry())
	{
		if (GeometryData.Triangles.Num() > 0 && GeometryData.Vertices.Num() > 0)
		{
			AkGeometryParams params;
			params.NumSurfaces = GeometryData.Surfaces.Num();
			params.NumTriangles = GeometryData.Triangles.Num();
			params.NumVertices = GeometryData.Vertices.Num();
			
			TUniquePtr<AkAcousticSurface[]> Surfaces; // temp surface buffer
			if (params.NumSurfaces) 
			{
				Surfaces = MakeUnique<AkAcousticSurface[]>(params.NumSurfaces);
				for (int i = 0; i < params.NumSurfaces; ++i)
				{
					Surfaces[i].occlusion = GeometryData.Surfaces[i].Occlusion;
					Surfaces[i].strName = TCHAR_TO_ANSI(*GeometryData.Surfaces[i].Name);
					Surfaces[i].textureID = GeometryData.Surfaces[i].Texture;
				}
			}
			params.Surfaces = Surfaces.Get();

			TUniquePtr<AkTriangle[]> Triangles = MakeUnique<AkTriangle[]>(params.NumTriangles);// temp triangle buffer
			for (int i = 0; i < params.NumTriangles; ++i)
			{
				Triangles[i].point0 = GeometryData.Triangles[i].Point0;
				Triangles[i].point1 = GeometryData.Triangles[i].Point1;
				Triangles[i].point2 = GeometryData.Triangles[i].Point2;
				Triangles[i].surface = GeometryData.Triangles[i].Surface;
			}
			params.Triangles = Triangles.Get();

			TUniquePtr<AkVertex[]> Vertices = MakeUnique<AkVertex[]>(params.NumVertices); // temp vertex buffer
			for (int i = 0; i < params.NumVertices; ++i)
			{
				Vertices[i].X = GeometryData.Vertices[i].X;
				Vertices[i].Y = GeometryData.Vertices[i].Y;
				Vertices[i].Z = GeometryData.Vertices[i].Z;
			}
			params.Vertices = Vertices.Get();
			
			params.EnableDiffraction = bEnableDiffraction;
			params.EnableDiffractionOnBoundaryEdges = bEnableDiffractionOnBoundaryEdges;

			if (AssociatedRoom)
			{
				UAkRoomComponent* room = Cast<UAkRoomComponent>(AssociatedRoom->GetComponentByClass(UAkRoomComponent::StaticClass()));

				if (room != nullptr)
					params.RoomID = room->GetRoomID();
			}

			if (AkAudioDevice->SetGeometry(AkGeometrySetID(this), params) == AK_Success)
				GeometryHasBeenSent = true;
		}
	}
}


void UAkGeometryComponent::RemoveGeometry()
{
	if (GeometryHasBeenSent)
	{
		FAkAudioDevice* AkAudioDevice = FAkAudioDevice::Get();
		check(AkAudioDevice != nullptr);
		if (AkAudioDevice->RemoveGeometrySet(AkGeometrySetID(this)) == AK_Success)
			GeometryHasBeenSent = false;
	}
}

void UAkGeometryComponent::UpdateGeometry()
{
	RemoveGeometry();
	UpdateGeometryTransform();
	SendGeometry();
}

void UAkGeometryComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	RemoveGeometry();
}

#if WITH_EDITOR
void UAkGeometryComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (AssociatedRoom && !Cast<UAkRoomComponent>(AssociatedRoom->GetComponentByClass(UAkRoomComponent::StaticClass())))
	{
		UE_LOG(LogAkAudio, Warning, TEXT("%s: The Surface Reflector Set's Associated Room is not of type UAkRoomComponent."), *GetOwner()->GetName());
	}

	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UAkGeometryComponent, MeshType) &&
		MeshType == AkMeshType::StaticMesh)
	{
		UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(GetAttachParent());
		UpdateStaticMeshOverride(StaticMeshComponent);
	}
}

void UAkGeometryComponent::PostEditUndo()
{
	OnMeshTypeChanged.ExecuteIfBound();
	Super::PostEditUndo();
}

void UAkGeometryComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (MeshType == AkMeshType::StaticMesh)
	{
		UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(GetAttachParent());
		if (StaticMeshSurfaceOverride.Num() != StaticMeshComponent->GetNumMaterials())
			UpdateStaticMeshOverride(StaticMeshComponent);
	}
}
#endif

void UAkGeometryComponent::UpdateStaticMeshOverride(UStaticMeshComponent* StaticMeshComponent)
{
	auto ToRemove = StaticMeshSurfaceOverride;

	int numMaterials = StaticMeshComponent->GetNumMaterials();
	for (int i = 0; i < numMaterials; i++)
	{
		UMaterialInterface* material = StaticMeshComponent->GetMaterial(i);
		if (StaticMeshSurfaceOverride.Contains(material))
			ToRemove.Remove(material);
		else
		{
			FAkGeometrySurfaceOverride surfaceOverride;
			if (PreviousStaticMeshSurfaceOverride.Contains(material))
				surfaceOverride = PreviousStaticMeshSurfaceOverride[material];

			StaticMeshSurfaceOverride.Add(material, surfaceOverride);
		}
	}

	for (auto& elemToRemove : ToRemove)
		StaticMeshSurfaceOverride.Remove(elemToRemove.Key);

	ToRemove.Empty();

	PreviousStaticMeshSurfaceOverride.Empty();
	PreviousStaticMeshSurfaceOverride = StaticMeshSurfaceOverride;
}

void UAkGeometryComponent::Serialize(FArchive& Ar)
{
#if WITH_EDITORONLY_DATA
	if (Ar.IsSaving())
		ConvertMesh();
#endif

	Super::Serialize(Ar);
}

void UAkGeometryComponent::UpdateGeometryTransform()
{
	FTransform OwnerToWorld = GetOwner()->ActorToWorld();

	for (auto& vertex : GeometryData.Vertices)
	{
		vertex = OwnerToWorld.TransformPosition(vertex).GridSnap(kVertexGridSnap);
	}
}
