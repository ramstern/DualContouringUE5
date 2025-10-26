// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/EngineSubsystem.h"
#include "DC_OctreeSettings.h"
#include "DC_OctreeNode.h"
#include "Interface/Core/RealtimeMeshKeys.h"
#include "Interface/Core/RealtimeMeshDataStream.h"
#include "DC_SDFOps.h"
#include "DC_OctreeCode.generated.h"

class UNoiseDataGenerator;

//clamps minimizers to voxels. big if condition.
#define CLAMP_MINIMIZERS 0

namespace RealtimeMesh
{
	template <typename IndexType, typename TangentElementType, typename TexCoordElementType, int32, typename PolyGroupIndexType>
	struct TRealtimeMeshBuilderLocal;
}

using MeshBuilder = RealtimeMesh::TRealtimeMeshBuilderLocal<uint32, FPackedNormal, FVector2DHalf, 1, uint16>;

UCLASS()
class DUALCONTOURINGTERRAIN_API UOctreeCode : public UEngineSubsystem
{
	GENERATED_BODY()
	
public:
	// USubsystem implementation Begin
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	
	// builds an octree and returns it
	static OctreeNode* BuildOctree(FVector3f center, float size, const OctreeSettingsMultithreadContext& settings_context, const TArray<float>& noise);
	static OctreeNode* RebuildOctree(FVector3f center, float size, const OctreeSettingsMultithreadContext& settings_context, const TArray<float>& noise, const TArray<SDFOp>& sdf_ops);
	
	//get octree node from position p inside starting (parent) node, at depth depth.
	TUniquePtr<OctreeNode>* GetNodeFromPositionDepth(OctreeNode* start, FVector3f p, int8 depth) const;

	//input: specific ordering of the main node and all its neighbor nodes
	static RealtimeMesh::FRealtimeMeshStreamSet PolygonizeOctree(const TArray<OctreeNode*, TInlineAllocator<8>>& nodes, bool negative_delta);
	static RealtimeMesh::FRealtimeMeshStreamSet PolygonizeOctree(const TArray<OctreeNode*, TInlineAllocator<8>>& nodes, const TArray<OctreeNode*, TInlineAllocator<8>>& ec_nodes, bool negative_delta);

	static FORCEINLINE int32 GetDim(int32 depth) { return 1 << depth;};
	static FORCEINLINE int32 Get1DIndexFrom3D(int32 x, int32 y, int32 z, int32 dim)
	{
		return z + (y * dim) + (x * dim * dim);
	}

	//debug draw octree node
	void DebugDrawOctree(UWorld* world, OctreeNode* node, int32 current_depth, bool draw_leaves, bool draw_simple_leaves, int32 how_deep);
private:

	static void ConstructLeafNode(OctreeNode* node, const FVector3f& node_p, const float* corner_densities, uint8 corners, const OctreeSettingsMultithreadContext& settings_context);
	static void ConstructLeafNode_Edit(OctreeNode* node, const FVector3f& node_p, const float* corner_densities, uint8 corners, const OctreeSettingsMultithreadContext& settings_context, const TArray<SDFOp>& sdf_ops);

	static StitchOctreeNode* ConstructSeamOctree(const TArray<OctreeNode*, TInlineAllocator<8>>& seam_nodes, bool negative_delta, MeshBuilder& builder);

	// get node size from depth, could be tableized
	FORCEINLINE float SizeFromNodeDepth(uint8 depth) { return 0.f / std::exp2f(static_cast<float>(depth)); };

	// simplify the octree with residual error
	static bool SimplifyOctree(OctreeNode* node, float simplify_threshold);

	// Build vertex buffer and assign indices to leaf data
	static void BuildMeshData(OctreeNode* node, MeshBuilder& builder);

	void BuildStitchMeshData(OctreeNode* node, OctreeNode* parent, MeshBuilder& builder);

	// DC polygonization methods
	static void DC_ProcessCell(OctreeNode* node, MeshBuilder& builder);
	static void DC_ProcessFace(OctreeNode* node_1, OctreeNode* node_2, unsigned char direction, MeshBuilder& builder);
	static void DC_ProcessEdge(OctreeNode* node_1, OctreeNode* node_2, OctreeNode* node_3, OctreeNode* node_4, unsigned char direction, MeshBuilder& builder);

	// DC polygonization methods (seam)
	static void DC_ProcessCell(StitchOctreeNode* node, MeshBuilder& builder);
	static void DC_ProcessFace(StitchOctreeNode* node_1, StitchOctreeNode* node_2, unsigned char direction, MeshBuilder& builder);
	static void DC_ProcessEdge(StitchOctreeNode* node_1, StitchOctreeNode* node_2, StitchOctreeNode* node_3, StitchOctreeNode* node_4, unsigned char direction, MeshBuilder& builder);

	//returns the root stitch node copy of start_node
	//StitchOctreeNode* ConstructSeamOctree(OctreeNode* start_node, uint8 node_idx, OctreeNode* parent_node, MeshBuilder& builder);


	
	//debug draw dc data
	void DebugDrawDCData(OctreeNode* node);
	void DebugDrawNode(UWorld* world, OctreeNode* node, float size, FColor color);
	void DebugDrawNodeMinimizer(OctreeNode* node);

	// get normal via fdm 
	static FVector3f FDMGetNormal(const FVector3f& at_point, float h, int32 seed);
	static FVector3f FDMGetNormal_SDF(const FVector3f& at_point, float h, int32 seed, const TArray<SDFOp>& sdf_ops);

	// get child index containing p from node position
	static FORCEINLINE uint8 GetChildNodeFromPosition(const FVector3f& p, const FVector3f& node_center)
	{
		const uint8 x = p.X > node_center.X;
		const uint8 y = p.Y > node_center.Y;
		const uint8 z = p.Z > node_center.Z;

		//x is 0b100, y 0b010, z 0b001

		return (y << 2) | (z << 1) | x;
	}



	const UOctreeSettings* octree_settings = nullptr;
	UNoiseDataGenerator* noise_gen = nullptr;
};
