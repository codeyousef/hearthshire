// Copyright Epic Games, Inc. All Rights Reserved.

#include "VoxelTypes.h"
#include "Materials/MaterialInterface.h"

UVoxelMaterialSet::UVoxelMaterialSet()
{
    DefaultMaterial = nullptr;
    
    // Initialize default material configurations
    Materials.Add(EVoxelMaterial::Grass, FVoxelMaterialConfig());
    Materials[EVoxelMaterial::Grass].BaseColor = FLinearColor(0.2f, 0.8f, 0.2f);
    Materials[EVoxelMaterial::Grass].Roughness = 0.8f;
    
    Materials.Add(EVoxelMaterial::Dirt, FVoxelMaterialConfig());
    Materials[EVoxelMaterial::Dirt].BaseColor = FLinearColor(0.4f, 0.3f, 0.2f);
    Materials[EVoxelMaterial::Dirt].Roughness = 0.9f;
    
    Materials.Add(EVoxelMaterial::Stone, FVoxelMaterialConfig());
    Materials[EVoxelMaterial::Stone].BaseColor = FLinearColor(0.5f, 0.5f, 0.5f);
    Materials[EVoxelMaterial::Stone].Roughness = 0.7f;
    
    Materials.Add(EVoxelMaterial::Wood, FVoxelMaterialConfig());
    Materials[EVoxelMaterial::Wood].BaseColor = FLinearColor(0.4f, 0.25f, 0.1f);
    Materials[EVoxelMaterial::Wood].Roughness = 0.6f;
    
    Materials.Add(EVoxelMaterial::Leaves, FVoxelMaterialConfig());
    Materials[EVoxelMaterial::Leaves].BaseColor = FLinearColor(0.1f, 0.6f, 0.1f);
    Materials[EVoxelMaterial::Leaves].Roughness = 0.5f;
    
    Materials.Add(EVoxelMaterial::Sand, FVoxelMaterialConfig());
    Materials[EVoxelMaterial::Sand].BaseColor = FLinearColor(0.9f, 0.8f, 0.6f);
    Materials[EVoxelMaterial::Sand].Roughness = 0.9f;
    
    Materials.Add(EVoxelMaterial::Water, FVoxelMaterialConfig());
    Materials[EVoxelMaterial::Water].BaseColor = FLinearColor(0.2f, 0.5f, 0.8f, 0.8f);
    Materials[EVoxelMaterial::Water].Roughness = 0.1f;
    
    Materials.Add(EVoxelMaterial::Snow, FVoxelMaterialConfig());
    Materials[EVoxelMaterial::Snow].BaseColor = FLinearColor(0.95f, 0.95f, 1.0f);
    Materials[EVoxelMaterial::Snow].Roughness = 0.3f;
    
    Materials.Add(EVoxelMaterial::Ice, FVoxelMaterialConfig());
    Materials[EVoxelMaterial::Ice].BaseColor = FLinearColor(0.8f, 0.9f, 1.0f, 0.9f);
    Materials[EVoxelMaterial::Ice].Roughness = 0.05f;
}

UMaterialInterface* UVoxelMaterialSet::GetMaterial(EVoxelMaterial VoxelMaterial) const
{
    if (const FVoxelMaterialConfig* Config = Materials.Find(VoxelMaterial))
    {
        return Config->Material ? Config->Material : DefaultMaterial;
    }
    
    return DefaultMaterial;
}

FLinearColor UVoxelMaterialSet::GetBaseColor(EVoxelMaterial VoxelMaterial) const
{
    if (const FVoxelMaterialConfig* Config = Materials.Find(VoxelMaterial))
    {
        return Config->BaseColor;
    }
    
    return FLinearColor::White;
}