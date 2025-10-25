// Fill out your copyright notice in the Description page of Project Settings.


#include "DC_OctreeNode.h"

OctreeNode::~OctreeNode()
{
	leaf_data.normal = FVector3f();
	/*for (uint8 i = 0; i < 8; i++)
	{
		delete children[i];
		children[i] = nullptr;
	}*/
}

StitchOctreeNode::~StitchOctreeNode()
{
	for (uint8 i = 0; i < 8; i++)
	{
		delete children[i];
	}
}
