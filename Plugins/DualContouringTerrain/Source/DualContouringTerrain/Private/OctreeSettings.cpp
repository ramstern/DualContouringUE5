// Fill out your copyright notice in the Description page of Project Settings.


#include "DC_OctreeSettings.h"

UOctreeSettings::UOctreeSettings()
{
	CategoryName = "Game";
	SectionName = "Octree Settings";
}

OctreeSettingsMultithreadContext& OctreeSettingsMultithreadContext::operator=(const UOctreeSettings& settings)
{
	seed = settings.noise_seed;
	max_depth = settings.max_depth;
	iso_surface = settings.iso_surface;
	simplify = settings.simplify;
	simplify_threshold = settings.simplify_threshold;
	normal_fdm_offset = settings.normal_fdm_offset;
	stddev_pos = settings.stddev_pos;
	stddev_normal = settings.stddev_normal;

	return *this;
}
