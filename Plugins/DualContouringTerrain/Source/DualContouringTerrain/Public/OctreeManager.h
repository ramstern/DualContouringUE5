// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "OctreeSettings.h"
#include "OctreeNode.h"
#include "RealtimeMeshComponent/Public/Interface/Core/RealtimeMeshKeys.h"
#include "OctreeManager.generated.h"

class UNoiseDataGenerator;
class ADC_OctreeRenderActor;
class URealtimeMeshSimple;

//clamps minimizers to voxels. big if condition.
#define CLAMP_MINIMIZERS 1

namespace RealtimeMesh
{
	template <typename IndexType, typename TangentElementType, typename TexCoordElementType, int32, typename PolyGroupIndexType>
	struct TRealtimeMeshBuilderLocal;
}

using MeshBuilder = RealtimeMesh::TRealtimeMeshBuilderLocal<uint32, FPackedNormal, FVector2DHalf, 1, uint16>;

UCLASS()
class DUALCONTOURINGTERRAIN_API UOctreeManager : public UTickableWorldSubsystem
{
	GENERATED_BODY()
	
public:
	// USubsystem implementation Begin
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	
	void PostWorldInit(UWorld* World, const UWorld::InitializationValues IVS);

private:

	// returns root node
	OctreeNode* SetupOctree();

	// recursive construction of child nodes
	OctreeNode* ConstructChildNodes(OctreeNode*& node, float node_size);
	OctreeNode* ConstructChildNodes(OctreeNode*& node);

	// creation and data collection of leaf nodes
	OctreeNode* ConstructLeafNode(OctreeNode*& node);

	// samples density values at a nodes 8 corners
	TArray<float> SampleOctreeNodeDensities(OctreeNode* node);

	// get node size from depth, could be tableized
	FORCEINLINE float SizeFromNodeDepth(uint8 depth) { return octree_settings->initial_size / std::exp2f(static_cast<float>(depth)); };

	// rebuilds the entire octree
	void RebuildOctree();

	// simplify the octree with residual error
	bool SimplifyOctree(OctreeNode* node);

	// Build vertex buffer and assign indices to leaf data
	void BuildMeshData(OctreeNode* node, MeshBuilder& builder);

	void BuildStitchMeshData(OctreeNode* node, OctreeNode* parent, MeshBuilder& builder);

	// DC polygonization methods
	void DC_ProcessCell(OctreeNode* node, MeshBuilder& builder);
	void DC_ProcessFace(OctreeNode* node_1, OctreeNode* node_2, unsigned char direction, MeshBuilder& builder);
	void DC_ProcessEdge(OctreeNode* node_1, OctreeNode* node_2, OctreeNode* node_3, OctreeNode* node_4, unsigned char direction, MeshBuilder& builder);

	// DC polygonization methods (seam)
	void DC_ProcessCell(StitchOctreeNode* node, MeshBuilder& builder);
	void DC_ProcessFace(StitchOctreeNode* node_1, StitchOctreeNode* node_2, unsigned char direction, MeshBuilder& builder);
	void DC_ProcessEdge(StitchOctreeNode* node_1, StitchOctreeNode* node_2, StitchOctreeNode* node_3, StitchOctreeNode* node_4, unsigned char direction, MeshBuilder& builder);

	void PolygonizeAtDepth(OctreeNode* start_node, uint8 node_idx, OctreeNode* parent, int8 depth);

	//returns the root stitch node copy of start_node
	StitchOctreeNode* ConstructSeamOctree(OctreeNode* start_node, uint8 node_idx, OctreeNode* parent_node, MeshBuilder& builder);

	//debug draw octree nodes
	void DebugDrawOctree(OctreeNode* node, int32 current_depth);
	//debug draw dc data
	void DebugDrawDCData();
	void DebugDrawNode(OctreeNode* node, float size, FColor color);
	void DebugDrawNodeMinimizer(OctreeNode* node);

	// get normal via fdm 
	FVector3f FDMGetNormal(FVector3f at_point);

	// try to get current render camera
	FVector GetActiveCameraLocation();

	// get child index containing p from node position
	uint8 GetChildNodeFromPosition(FVector3f p, FVector3f node_center);

	OctreeNode* GetNodeFromPositionDepth(OctreeNode* start, FVector3f p, int8 depth);

	const UOctreeSettings* octree_settings = nullptr;
	UNoiseDataGenerator* noise_gen = nullptr;
	OctreeNode* root_node = nullptr;

	FVector camera_pos = FVector();
	uint8 last_visited_child_idx = 255ui8;

#if UE_BUILD_DEBUG
	struct dbg_edge
	{
		FVector start;
		FVector end;
	};
	TArray<dbg_edge> debug_edges; 
#endif

	// actor for rendering the octree mesh
	ADC_OctreeRenderActor* render_actor = nullptr;
	URealtimeMeshSimple* octree_mesh = nullptr;
	TArray<FRealtimeMeshSectionGroupKey> mesh_group_keys;

	//debug cam proxy actor
	ADC_OctreeRenderActor* cam_proxy_actor = nullptr;

	virtual void Tick(float DeltaTime) override;
	TStatId GetStatId() const override;

	virtual bool DoesSupportWorldType(EWorldType::Type type) const override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual bool IsTickable() const override;
	virtual bool IsTickableInEditor() const override;
};
