// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "probabilistic-quadrics.hh"
#include "DC_Mat3x3.h"


using uemath = pq::math<float, FVector3f, FVector3f, FMatrix3x3>;

//typedef quadrid type
using quadric3 = pq::quadric<uemath>;

constexpr unsigned char NODE_INTERNAL = 0;
constexpr unsigned char NODE_LEAF = 1;
constexpr unsigned char NODE_COLLAPSED_LEAF = 2;

struct DC_LeafData
{
	FVector3f normal;
	FVector3f minimizer;
	quadric3 qef;
	uint32 index;
};

struct DUALCONTOURINGTERRAIN_API OctreeNode
{
public:
	OctreeNode() = default;
	~OctreeNode();

	OctreeNode* children[8] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
	FVector3f center = FVector3f::ZeroVector;
	uint8 depth = 0;
	unsigned char type = NODE_INTERNAL;
	unsigned char child_mask = 0;
	unsigned char corners = 0;
	DC_LeafData* leaf_data = nullptr;
	float size;
};

constexpr uint32 INDEX_NOEXIST = MAX_uint32;

struct DUALCONTOURINGTERRAIN_API OctreeNode_smol
{
public:
	uint32 first_child;
	uint8 depth = 0;
	uint8 child_mask = 0;
	uint8 type = NODE_INTERNAL;
	uint8 corners = 0;
	uint32 leaf_data_idx;

	//get the index into the node storing array
	FORCEINLINE uint32 GetChildIndex(unsigned char idx) const
	{
		//we need to count the bits before idx in the mask
		return ChildExists(idx) ? first_child + FMath::CountBits(((1u << idx) - 1u) & child_mask) : INDEX_NOEXIST;
	}
	
	//doesn't check for existence of the child
	FORCEINLINE uint32 GetChildIndex_Unchecked(unsigned char idx) const
	{
		//we need to count the bits before idx in the mask
		return first_child + FMath::CountBits(((1u << idx) - 1u) & child_mask);
	}

	FORCEINLINE bool ChildExists(unsigned char idx) const
	{
		return (child_mask >> idx) & 1u;
	}
};
