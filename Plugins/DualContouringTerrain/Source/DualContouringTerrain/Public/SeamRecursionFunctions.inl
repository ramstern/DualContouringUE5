#pragma once


constexpr unsigned char z_backside_lookup[4] = { 0,4,2,6 };
constexpr unsigned char z_frontside_lookup[4] = { 1,5,3,7 };
constexpr unsigned char y_topside_lookup[4] = { 2,3,6,7 };
constexpr unsigned char y_bottomside_lookup[4] = { 0,1,4,5 };

StitchOctreeNode* LeftRecurse(OctreeNode* node, StitchOctreeNode* existing, MeshBuilder& builder)
{
	if (!node) return nullptr;

	bool did_exist = true;
	if (!existing)
	{
		existing = new StitchOctreeNode();
		existing->corners = node->corners;
		existing->depth = node->depth;
		existing->type = node->type;
		did_exist = false;
	}

	if (node->type == NODE_INTERNAL)
	{
		for (uint8 i = 0; i < 4; i++)
		{
			existing->children[i] = LeftRecurse(node->children[i].Get(), existing->children[i], builder);
		}
	}
	else if (!did_exist)
	{
		const auto& vertex = builder.AddVertex(node->leaf_data.minimizer * inv_scale_factor).SetNormal(node->leaf_data.normal);
		existing->tri_index = vertex.GetIndex();
	}

	return existing;
}

StitchOctreeNode* RightRecurse(OctreeNode* node, StitchOctreeNode* existing, MeshBuilder& builder)
{
	if (!node) return nullptr;

	bool did_exist = true;
	if (!existing)
	{
		existing = new StitchOctreeNode();
		existing->corners = node->corners;
		existing->depth = node->depth;
		existing->type = node->type;
		did_exist = false;
	}

	if (node->type == NODE_INTERNAL)
	{
		for (uint8 i = 4; i < 8; i++)
		{
			existing->children[i] = RightRecurse(node->children[i].Get(), existing->children[i], builder);
		}
	}
	else if (!did_exist)
	{
		const auto& vertex = builder.AddVertex(node->leaf_data.minimizer * inv_scale_factor).SetNormal(node->leaf_data.normal);
		existing->tri_index = vertex.GetIndex();
	}

	return existing;
}

StitchOctreeNode* BackRecurse(OctreeNode* node, StitchOctreeNode* existing, MeshBuilder& builder)
{
	if (!node) return nullptr;

	bool did_exist = true;
	if (!existing)
	{
		existing = new StitchOctreeNode();
		existing->corners = node->corners;
		existing->depth = node->depth;
		existing->type = node->type;
		did_exist = false;
	}

	if (node->type == NODE_INTERNAL)
	{
		for (uint8 i = 0; i < 4; i++)
		{
			unsigned char idx = z_backside_lookup[i];

			existing->children[idx] = BackRecurse(node->children[idx].Get(), existing->children[idx], builder);
		}
	}
	else if (!did_exist)
	{
		const auto& vertex = builder.AddVertex(node->leaf_data.minimizer * inv_scale_factor).SetNormal(node->leaf_data.normal);
		existing->tri_index = vertex.GetIndex();
	}

	return existing;
}

StitchOctreeNode* FrontRecurse(OctreeNode* node, StitchOctreeNode* existing, MeshBuilder& builder)
{
	if (!node) return nullptr;

	bool did_exist = true;
	if (!existing)
	{
		existing = new StitchOctreeNode();
		existing->corners = node->corners;
		existing->depth = node->depth;
		existing->type = node->type;
		did_exist = false;
	}

	if (node->type == NODE_INTERNAL)
	{
		for (uint8 i = 0; i < 4; i++)
		{
			unsigned char idx = z_frontside_lookup[i];

			existing->children[idx] = FrontRecurse(node->children[idx].Get(), existing->children[idx], builder);
		}
	}
	else if (!did_exist)
	{
		const auto& vertex = builder.AddVertex(node->leaf_data.minimizer * inv_scale_factor).SetNormal(node->leaf_data.normal);
		existing->tri_index = vertex.GetIndex();
	}

	return existing;
}

StitchOctreeNode* TopRecurse(OctreeNode* node, StitchOctreeNode* existing, MeshBuilder& builder)
{
	if (!node) return nullptr;

	bool did_exist = true;
	if (!existing)
	{
		existing = new StitchOctreeNode();
		existing->corners = node->corners;
		existing->depth = node->depth;
		existing->type = node->type;
		did_exist = false;
	}

	if (node->type == NODE_INTERNAL)
	{
		for (uint8 i = 0; i < 4; i++)
		{
			unsigned char idx = y_topside_lookup[i];

			existing->children[idx] = TopRecurse(node->children[idx].Get(), existing->children[idx], builder);
		}
	}
	else if (!did_exist)
	{
		const auto& vertex = builder.AddVertex(node->leaf_data.minimizer * inv_scale_factor).SetNormal(node->leaf_data.normal);
		existing->tri_index = vertex.GetIndex();
	}

	return existing;
}

StitchOctreeNode* BottomRecurse(OctreeNode* node, StitchOctreeNode* existing, MeshBuilder& builder)
{
	if (!node) return nullptr;

	bool did_exist = true;
	if (!existing)
	{
		existing = new StitchOctreeNode();
		existing->corners = node->corners;
		existing->depth = node->depth;
		existing->type = node->type;
		did_exist = false;
	}

	if (node->type == NODE_INTERNAL)
	{
		for (uint8 i = 0; i < 4; i++)
		{
			unsigned char idx = y_bottomside_lookup[i];

			existing->children[idx] = BottomRecurse(node->children[idx].Get(), existing->children[idx], builder);
		}
	}
	else if (!did_exist)
	{
		const auto& vertex = builder.AddVertex(node->leaf_data.minimizer * inv_scale_factor).SetNormal(node->leaf_data.normal);
		existing->tri_index = vertex.GetIndex();
	}

	return existing;
}

StitchOctreeNode* CornerBarRecurseTL(OctreeNode* node, StitchOctreeNode* existing, MeshBuilder& builder)
{
	if (!node) return nullptr;

	bool did_exist = true;
	if (!existing)
	{
		existing = new StitchOctreeNode();
		existing->corners = node->corners;
		existing->depth = node->depth;
		existing->type = node->type;
		did_exist = false;
	}

	if (node->type == NODE_INTERNAL)
	{
		existing->children[3] = CornerBarRecurseTL(node->children[3].Get(), existing->children[3], builder);
		existing->children[2] = CornerBarRecurseTL(node->children[2].Get(), existing->children[2], builder);
	}
	else if (!did_exist)
	{
		const auto& vertex = builder.AddVertex(node->leaf_data.minimizer * inv_scale_factor).SetNormal(node->leaf_data.normal);
		existing->tri_index = vertex.GetIndex();
	}

	return existing;
}
StitchOctreeNode* CornerBarRecurseTR(OctreeNode* node, StitchOctreeNode* existing, MeshBuilder& builder)
{
	if (!node) return nullptr;

	bool did_exist = true;
	if (!existing)
	{
		existing = new StitchOctreeNode();
		existing->corners = node->corners;
		existing->depth = node->depth;
		existing->type = node->type;
		did_exist = false;
	}

	if (node->type == NODE_INTERNAL)
	{
		existing->children[6] = CornerBarRecurseTR(node->children[6].Get(), existing->children[6], builder);
		existing->children[7] = CornerBarRecurseTR(node->children[7].Get(), existing->children[7], builder);
	}
	else if (!did_exist)
	{
		const auto& vertex = builder.AddVertex(node->leaf_data.minimizer * inv_scale_factor).SetNormal(node->leaf_data.normal);
		existing->tri_index = vertex.GetIndex();
	}

	return existing;
}
StitchOctreeNode* CornerBarRecurseTF(OctreeNode* node, StitchOctreeNode* existing, MeshBuilder& builder)
{
	if (!node) return nullptr;

	bool did_exist = true;
	if (!existing)
	{
		existing = new StitchOctreeNode();
		existing->corners = node->corners;
		existing->depth = node->depth;
		existing->type = node->type;
		did_exist = false;
	}

	if (node->type == NODE_INTERNAL)
	{
		existing->children[3] = CornerBarRecurseTF(node->children[3].Get(), existing->children[3], builder);
		existing->children[7] = CornerBarRecurseTF(node->children[7].Get(), existing->children[7], builder);
	}
	else if (!did_exist)
	{
		const auto& vertex = builder.AddVertex(node->leaf_data.minimizer * inv_scale_factor).SetNormal(node->leaf_data.normal);
		existing->tri_index = vertex.GetIndex();
	}

	return existing;
}
StitchOctreeNode* CornerBarRecurseTB(OctreeNode* node, StitchOctreeNode* existing, MeshBuilder& builder)
{
	if (!node) return nullptr;

	bool did_exist = true;
	if (!existing)
	{
		existing = new StitchOctreeNode();
		existing->corners = node->corners;
		existing->depth = node->depth;
		existing->type = node->type;
		did_exist = false;
	}

	if (node->type == NODE_INTERNAL)
	{
		existing->children[2] = CornerBarRecurseTB(node->children[2].Get(), existing->children[2], builder);
		existing->children[6] = CornerBarRecurseTB(node->children[6].Get(), existing->children[6], builder);
	}
	else if (!did_exist)
	{
		const auto& vertex = builder.AddVertex(node->leaf_data.minimizer * inv_scale_factor).SetNormal(node->leaf_data.normal);
		existing->tri_index = vertex.GetIndex();
	}

	return existing;
}

StitchOctreeNode* CornerBarRecurseBL(OctreeNode* node, StitchOctreeNode* existing, MeshBuilder& builder)
{
	if (!node) return nullptr;

	bool did_exist = true;
	if (!existing)
	{
		existing = new StitchOctreeNode();
		existing->corners = node->corners;
		existing->depth = node->depth;
		existing->type = node->type;
		did_exist = false;
	}

	if (node->type == NODE_INTERNAL)
	{
		existing->children[0] = CornerBarRecurseBL(node->children[0].Get(), existing->children[0], builder);
		existing->children[1] = CornerBarRecurseBL(node->children[1].Get(), existing->children[1], builder);
	}
	else if (!did_exist)
	{
		const auto& vertex = builder.AddVertex(node->leaf_data.minimizer * inv_scale_factor).SetNormal(node->leaf_data.normal);
		existing->tri_index = vertex.GetIndex();
	}

	return existing;
}
StitchOctreeNode* CornerBarRecurseBR(OctreeNode* node, StitchOctreeNode* existing, MeshBuilder& builder)
{
	if (!node) return nullptr;

	bool did_exist = true;
	if (!existing)
	{
		existing = new StitchOctreeNode();
		existing->corners = node->corners;
		existing->depth = node->depth;
		existing->type = node->type;
		did_exist = false;
	}

	if (node->type == NODE_INTERNAL)
	{
		existing->children[4] = CornerBarRecurseBR(node->children[4].Get(), existing->children[4], builder);
		existing->children[5] = CornerBarRecurseBR(node->children[5].Get(), existing->children[5], builder);
	}
	else if (!did_exist)
	{
		const auto& vertex = builder.AddVertex(node->leaf_data.minimizer * inv_scale_factor).SetNormal(node->leaf_data.normal);
		existing->tri_index = vertex.GetIndex();
	}

	return existing;
}
StitchOctreeNode* CornerBarRecurseBF(OctreeNode* node, StitchOctreeNode* existing, MeshBuilder& builder)
{
	if (!node) return nullptr;

	bool did_exist = true;
	if (!existing)
	{
		existing = new StitchOctreeNode();
		existing->corners = node->corners;
		existing->depth = node->depth;
		existing->type = node->type;
		did_exist = false;
	}

	if (node->type == NODE_INTERNAL)
	{
		existing->children[1] = CornerBarRecurseBF(node->children[1].Get(), existing->children[1], builder);
		existing->children[5] = CornerBarRecurseBF(node->children[5].Get(), existing->children[5], builder);
	}
	else if (!did_exist)
	{
		const auto& vertex = builder.AddVertex(node->leaf_data.minimizer * inv_scale_factor).SetNormal(node->leaf_data.normal);
		existing->tri_index = vertex.GetIndex();
	}

	return existing;
}
StitchOctreeNode* CornerBarRecurseBB(OctreeNode* node, StitchOctreeNode* existing, MeshBuilder& builder)
{
	if (!node) return nullptr;

	bool did_exist = true;
	if (!existing)
	{
		existing = new StitchOctreeNode();
		existing->corners = node->corners;
		existing->depth = node->depth;
		existing->type = node->type;
		did_exist = false;
	}

	if (node->type == NODE_INTERNAL)
	{
		existing->children[0] = CornerBarRecurseBB(node->children[0].Get(), existing->children[0], builder);
		existing->children[4] = CornerBarRecurseBB(node->children[4].Get(), existing->children[4], builder);
	}
	else if (!did_exist)
	{
		const auto& vertex = builder.AddVertex(node->leaf_data.minimizer * inv_scale_factor).SetNormal(node->leaf_data.normal);
		existing->tri_index = vertex.GetIndex();
	}

	return existing;
}

StitchOctreeNode* CornerBarRecurseVLB(OctreeNode* node, StitchOctreeNode* existing, MeshBuilder& builder)
{
	if (!node) return nullptr;

	bool did_exist = true;
	if (!existing)
	{
		existing = new StitchOctreeNode();
		existing->corners = node->corners;
		existing->depth = node->depth;
		existing->type = node->type;
		did_exist = false;
	}

	if (node->type == NODE_INTERNAL)
	{
		existing->children[0] = CornerBarRecurseVLB(node->children[0].Get(), existing->children[0], builder);
		existing->children[2] = CornerBarRecurseVLB(node->children[2].Get(), existing->children[2], builder);
	}
	else if (!did_exist)
	{
		const auto& vertex = builder.AddVertex(node->leaf_data.minimizer * inv_scale_factor).SetNormal(node->leaf_data.normal);
		existing->tri_index = vertex.GetIndex();
	}

	return existing;
}
StitchOctreeNode* CornerBarRecurseVRB(OctreeNode* node, StitchOctreeNode* existing, MeshBuilder& builder)
{
	if (!node) return nullptr;

	bool did_exist = true;
	if (!existing)
	{
		existing = new StitchOctreeNode();
		existing->corners = node->corners;
		existing->depth = node->depth;
		existing->type = node->type;
		did_exist = false;
	}

	if (node->type == NODE_INTERNAL)
	{
		existing->children[4] = CornerBarRecurseVRB(node->children[4].Get(), existing->children[4], builder);
		existing->children[6] = CornerBarRecurseVRB(node->children[6].Get(), existing->children[6], builder);
	}
	else if (!did_exist)
	{
		const auto& vertex = builder.AddVertex(node->leaf_data.minimizer * inv_scale_factor).SetNormal(node->leaf_data.normal);
		existing->tri_index = vertex.GetIndex();
	}

	return existing;
}
StitchOctreeNode* CornerBarRecurseVLF(OctreeNode* node, StitchOctreeNode* existing, MeshBuilder& builder)
{
	if (!node) return nullptr;

	bool did_exist = true;
	if (!existing)
	{
		existing = new StitchOctreeNode();
		existing->corners = node->corners;
		existing->depth = node->depth;
		existing->type = node->type;
		did_exist = false;
	}

	if (node->type == NODE_INTERNAL)
	{
		existing->children[1] = CornerBarRecurseVLF(node->children[1].Get(), existing->children[1], builder);
		existing->children[3] = CornerBarRecurseVLF(node->children[3].Get(), existing->children[3], builder);
	}
	else if (!did_exist)
	{
		const auto& vertex = builder.AddVertex(node->leaf_data.minimizer * inv_scale_factor).SetNormal(node->leaf_data.normal);
		existing->tri_index = vertex.GetIndex();
	}

	return existing;
}
StitchOctreeNode* CornerBarRecurseVRF(OctreeNode* node, StitchOctreeNode* existing, MeshBuilder& builder)
{
	if (!node) return nullptr;

	bool did_exist = true;
	if (!existing)
	{
		existing = new StitchOctreeNode();
		existing->corners = node->corners;
		existing->depth = node->depth;
		existing->type = node->type;
		did_exist = false;
	}

	if (node->type == NODE_INTERNAL)
	{
		existing->children[7] = CornerBarRecurseVRF(node->children[7].Get(), existing->children[7], builder);
		existing->children[5] = CornerBarRecurseVRF(node->children[5].Get(), existing->children[5], builder);
	}
	else if (!did_exist)
	{
		const auto& vertex = builder.AddVertex(node->leaf_data.minimizer * inv_scale_factor).SetNormal(node->leaf_data.normal);
		existing->tri_index = vertex.GetIndex();
	}

	return existing;
}

StitchOctreeNode* CornerMiniRecurse_0(OctreeNode* node, StitchOctreeNode* existing, MeshBuilder& builder)
{
	if (!node) return nullptr;

	bool did_exist = true;
	if (!existing)
	{
		existing = new StitchOctreeNode();
		existing->corners = node->corners;
		existing->depth = node->depth;
		existing->type = node->type;
		did_exist = false;
	}

	if (node->type == NODE_INTERNAL)
	{
		existing->children[0] = CornerMiniRecurse_0(node->children[0].Get(), existing->children[0], builder);
	}
	else if (!did_exist)
	{
		const auto& vertex = builder.AddVertex(node->leaf_data.minimizer * inv_scale_factor).SetNormal(node->leaf_data.normal);
		existing->tri_index = vertex.GetIndex();
	}

	return existing;
}
StitchOctreeNode* CornerMiniRecurse_1(OctreeNode* node, StitchOctreeNode* existing, MeshBuilder& builder)
{
	if (!node) return nullptr;

	bool did_exist = true;
	if (!existing)
	{
		existing = new StitchOctreeNode();
		existing->corners = node->corners;
		existing->depth = node->depth;
		existing->type = node->type;
		did_exist = false;
	}

	if (node->type == NODE_INTERNAL)
	{
		existing->children[1] = CornerMiniRecurse_1(node->children[1].Get(), existing->children[1], builder);
	}
	else if (!did_exist)
	{
		const auto& vertex = builder.AddVertex(node->leaf_data.minimizer * inv_scale_factor).SetNormal(node->leaf_data.normal);
		existing->tri_index = vertex.GetIndex();
	}

	return existing;
}
StitchOctreeNode* CornerMiniRecurse_2(OctreeNode* node, StitchOctreeNode* existing, MeshBuilder& builder)
{
	if (!node) return nullptr;

	bool did_exist = true;
	if (!existing)
	{
		existing = new StitchOctreeNode();
		existing->corners = node->corners;
		existing->depth = node->depth;
		existing->type = node->type;
		did_exist = false;
	}

	if (node->type == NODE_INTERNAL)
	{
		existing->children[2] = CornerMiniRecurse_2(node->children[2].Get(), existing->children[2], builder);
	}
	else if (!did_exist)
	{
		const auto& vertex = builder.AddVertex(node->leaf_data.minimizer * inv_scale_factor).SetNormal(node->leaf_data.normal);
		existing->tri_index = vertex.GetIndex();
	}

	return existing;
}
StitchOctreeNode* CornerMiniRecurse_3(OctreeNode* node, StitchOctreeNode* existing, MeshBuilder& builder)
{
	if (!node) return nullptr;

	bool did_exist = true;
	if (!existing)
	{
		existing = new StitchOctreeNode();
		existing->corners = node->corners;
		existing->depth = node->depth;
		existing->type = node->type;
		did_exist = false;
	}

	if (node->type == NODE_INTERNAL)
	{
		existing->children[3] = CornerMiniRecurse_3(node->children[3].Get(), existing->children[3], builder);
	}
	else if (!did_exist)
	{
		const auto& vertex = builder.AddVertex(node->leaf_data.minimizer * inv_scale_factor).SetNormal(node->leaf_data.normal);
		existing->tri_index = vertex.GetIndex();
	}

	return existing;
}
StitchOctreeNode* CornerMiniRecurse_4(OctreeNode* node, StitchOctreeNode* existing, MeshBuilder& builder)
{
	if (!node) return nullptr;

	bool did_exist = true;
	if (!existing)
	{
		existing = new StitchOctreeNode();
		existing->corners = node->corners;
		existing->depth = node->depth;
		existing->type = node->type;
		did_exist = false;
	}

	if (node->type == NODE_INTERNAL)
	{
		existing->children[4] = CornerMiniRecurse_4(node->children[4].Get(), existing->children[4], builder);
	}
	else if (!did_exist)
	{
		const auto& vertex = builder.AddVertex(node->leaf_data.minimizer * inv_scale_factor).SetNormal(node->leaf_data.normal);
		existing->tri_index = vertex.GetIndex();
	}

	return existing;
}
StitchOctreeNode* CornerMiniRecurse_5(OctreeNode* node, StitchOctreeNode* existing, MeshBuilder& builder)
{
	if (!node) return nullptr;

	bool did_exist = true;
	if (!existing)
	{
		existing = new StitchOctreeNode();
		existing->corners = node->corners;
		existing->depth = node->depth;
		existing->type = node->type;
		did_exist = false;
	}

	if (node->type == NODE_INTERNAL)
	{
		existing->children[5] = CornerMiniRecurse_5(node->children[5].Get(), existing->children[5], builder);
	}
	else if (!did_exist)
	{
		const auto& vertex = builder.AddVertex(node->leaf_data.minimizer * inv_scale_factor).SetNormal(node->leaf_data.normal);
		existing->tri_index = vertex.GetIndex();
	}

	return existing;
}
StitchOctreeNode* CornerMiniRecurse_6(OctreeNode* node, StitchOctreeNode* existing, MeshBuilder& builder)
{
	if (!node) return nullptr;

	bool did_exist = true;
	if (!existing)
	{
		existing = new StitchOctreeNode();
		existing->corners = node->corners;
		existing->depth = node->depth;
		existing->type = node->type;
		did_exist = false;
	}

	if (node->type == NODE_INTERNAL)
	{
		existing->children[6] = CornerMiniRecurse_6(node->children[6].Get(), existing->children[6], builder);
	}
	else if (!did_exist)
	{
		const auto& vertex = builder.AddVertex(node->leaf_data.minimizer * inv_scale_factor).SetNormal(node->leaf_data.normal);
		existing->tri_index = vertex.GetIndex();
	}

	return existing;
}
StitchOctreeNode* CornerMiniRecurse_7(OctreeNode* node, StitchOctreeNode* existing, MeshBuilder& builder)
{
	if (!node) return nullptr;

	bool did_exist = true;
	if (!existing)
	{
		existing = new StitchOctreeNode();
		existing->corners = node->corners;
		existing->depth = node->depth;
		existing->type = node->type;
		did_exist = false;
	}

	if (node->type == NODE_INTERNAL)
	{
		existing->children[7] = CornerMiniRecurse_7(node->children[7].Get(), existing->children[7], builder);
	}
	else if (!did_exist)
	{
		const auto& vertex = builder.AddVertex(node->leaf_data.minimizer * inv_scale_factor).SetNormal(node->leaf_data.normal);
		existing->tri_index = vertex.GetIndex();
	}

	return existing;
}