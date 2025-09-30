// Fill out your copyright notice in the Description page of Project Settings.


#include "Chunk.h"

Chunk::~Chunk()
{
	delete root;
}

Chunk::Chunk(Chunk&& other) noexcept : root(other.root), coordinates(other.coordinates), center(other.center)
{
	other.root = nullptr;
}

Chunk& Chunk::operator=(Chunk&& other) noexcept
{
	if(this != &other)
	{
		coordinates = other.coordinates;
		center = other.center;

		delete root;
		root = other.root;
		other.root = nullptr;
	}
	
	return *this;
}
