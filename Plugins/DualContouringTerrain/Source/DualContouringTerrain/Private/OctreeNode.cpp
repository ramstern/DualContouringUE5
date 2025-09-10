// Fill out your copyright notice in the Description page of Project Settings.


#include "OctreeNode.h"

OctreeNode::~OctreeNode()
{
	for (size_t i = 0; i < 8; i++)
	{
		delete children[i];
		children[i] = nullptr;
	}

	delete leaf_data;
}
