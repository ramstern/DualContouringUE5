// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "OctreeSettings.h"
#include "OctreeNode.h"
#include "Interface/Core/RealtimeMeshKeys.h"
#include "Interface/Core/RealtimeMeshDataStream.h"
#include "OctreeManager.generated.h"

class UNoiseDataGenerator;
class ADC_OctreeRenderActor;
class URealtimeMeshSimple;

//clamps minimizers to voxels. big if condition.
#define CLAMP_MINIMIZERS 0

namespace RealtimeMesh
{
	template <typename IndexType, typename TangentElementType, typename TexCoordElementType, int32, typename PolyGroupIndexType>
	struct TRealtimeMeshBuilderLocal;
}

using MeshBuilder = RealtimeMesh::TRealtimeMeshBuilderLocal<uint32, FPackedNormal, FVector2DHalf, 1, uint16>;

UCLASS()
class DUALCONTOURINGTERRAIN_API UOctreeManager : public UWorldSubsystem
{
	GENERATED_BODY()
	
public:
	// USubsystem implementation Begin
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	
	void PostWorldInit(UWorld* World, const UWorld::InitializationValues IVS);

	// builds an octree and returns it
	OctreeNode* BuildOctree(FVector3f center, float size, const OctreeSettingsMultithreadContext& settings_context);

	//input: specific ordering of the main node and all its neighbor nodes
	RealtimeMesh::FRealtimeMeshStreamSet PolygonizeOctree(const TArray<OctreeNode*, TInlineAllocator<8>>& nodes, bool negative_delta, int32 chunk_idx, bool has_group_key);

	void UpdateSection(const RealtimeMesh::FRealtimeMeshStreamSet& stream_set, FRealtimeMeshSectionGroupKey key);
	void CreateSection(const RealtimeMesh::FRealtimeMeshStreamSet& stream_set, FRealtimeMeshSectionGroupKey key);

	//debug draw octree node
	void DebugDrawOctree(OctreeNode* node, int32 current_depth, bool draw_leaves, bool draw_simple_leaves, int32 how_deep);
private:

	void ConstructLeafNode_V2(OctreeNode* node, const FVector3f& node_p, const float* corner_densities, uint8 corners, const OctreeSettingsMultithreadContext& settings_context);

	StitchOctreeNode* ConstructSeamOctree(const TArray<OctreeNode*, TInlineAllocator<8>>& seam_nodes, bool negative_delta, MeshBuilder& builder);

	// get node size from depth, could be tableized
	FORCEINLINE float SizeFromNodeDepth(uint8 depth) { return 0.f / std::exp2f(static_cast<float>(depth)); };

	
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

	//returns the root stitch node copy of start_node
	//StitchOctreeNode* ConstructSeamOctree(OctreeNode* start_node, uint8 node_idx, OctreeNode* parent_node, MeshBuilder& builder);

	FORCEINLINE int32 GetDim(int32 depth) { return 1 << depth;};

	
	//debug draw dc data
	void DebugDrawDCData(OctreeNode* node);
	void DebugDrawNode(OctreeNode* node, float size, FColor color);
	void DebugDrawNodeMinimizer(OctreeNode* node);

	// get normal via fdm 
	FVector3f FDMGetNormal(const FVector3f& at_point);

	// get child index containing p from node position
	FORCEINLINE uint8 GetChildNodeFromPosition(const FVector3f& p, const FVector3f& node_center)
	{
		const uint8 x = p.X > node_center.X;
		const uint8 y = p.Y > node_center.Y;
		const uint8 z = p.Z > node_center.Z;

		//x is 0b100, y 0b010, z 0b001

		return (y << 2) | (z << 1) | x;
	}

	OctreeNode* GetNodeFromPositionDepth(OctreeNode* start, FVector3f p, int8 depth);

	FORCEINLINE int32 Get1DIndexFrom3D(int32 x, int32 y, int32 z, int32 dim) const
	{
		return z + (y * dim) + (x * dim * dim);
	}

	const UOctreeSettings* octree_settings = nullptr;
	UNoiseDataGenerator* noise_gen = nullptr;

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

	virtual bool DoesSupportWorldType(EWorldType::Type type) const override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
};
