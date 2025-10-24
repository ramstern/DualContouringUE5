// Fill out your copyright notice in the Description page of Project Settings.


#include "DC_Chunk.h"

Chunk::~Chunk()
{
	//delete root;
}

Chunk::Chunk(Chunk&& other) noexcept : root(MoveTemp(other.root)), center(other.center), mesh(other.mesh)
{
	//other.root = nullptr;
	other.mesh = nullptr;
}

Chunk& Chunk::operator=(Chunk&& other) noexcept
{
	if(this != &other)
	{
		center = other.center;

		mesh = other.mesh;
		other.mesh = nullptr;

		root = MoveTemp(other.root);
	}
	
	return *this;
}