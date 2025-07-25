// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VoxelTypes.h"
#include "ProceduralMeshComponent.h"

/**
 * Voxel mesh generation utilities
 */
class HEARTHSHIREVOXEL_API FVoxelMeshGenerator
{
public:
    // Configuration
    struct FGenerationConfig
    {
        float VoxelSize = 25.0f;
        bool bGenerateCollision = true;
        bool bSmoothNormals = false;
        bool bGenerateTangents = true;
        bool bOptimizeIndices = true;
        
        FGenerationConfig() = default;
    };
    
    // Generate basic mesh from voxel data (no optimization)
    static void GenerateBasicMesh(
        const FVoxelChunkData& ChunkData,
        FVoxelMeshData& OutMeshData,
        const FGenerationConfig& Config = FGenerationConfig()
    );
    
    // Generate optimized mesh with greedy meshing
    static void GenerateGreedyMesh(
        const FVoxelChunkData& ChunkData,
        FVoxelMeshData& OutMeshData,
        const FGenerationConfig& Config = FGenerationConfig()
    );
    
    // Generate LOD mesh
    static void GenerateLODMesh(
        const FVoxelChunkData& ChunkData,
        FVoxelMeshData& OutMeshData,
        int32 LODLevel,
        const FGenerationConfig& Config = FGenerationConfig()
    );
    
    // Apply mesh data to procedural mesh component
    static void ApplyMeshToComponent(
        UProceduralMeshComponent* Component,
        const FVoxelMeshData& MeshData,
        UVoxelMaterialSet* MaterialSet = nullptr
    );
    
    // Helper functions made public for FVoxelGreedyMesher access
    static void AddQuad(
        FVoxelMeshData& MeshData,
        const FVector& V0,
        const FVector& V1,
        const FVector& V2,
        const FVector& V3,
        const FVector& Normal,
        const FVector2D& UV0,
        const FVector2D& UV1,
        const FVector2D& UV2,
        const FVector2D& UV3,
        EVoxelMaterial Material
    );
    
    static FVector GetFaceNormal(EVoxelFace Face);
    static void GetFaceVertices(EVoxelFace Face, const FVector& Position, float Size, FVector OutVertices[4]);
    static void GetFaceUVs(EVoxelFace Face, FVector2D OutUVs[4]);
    static bool IsFaceVisible(
        const FVoxelChunkData& ChunkData,
        int32 X, int32 Y, int32 Z,
        EVoxelFace Face
    );
    
    // Material section management - made public for greedy mesher
    static int32 GetOrCreateMaterialSection(FVoxelMeshData& MeshData, EVoxelMaterial Material);
    
private:
    // Face generation helpers
    static void AddFace(
        FVoxelMeshData& MeshData,
        const FVector& Position,
        EVoxelFace Face,
        EVoxelMaterial Material,
        float VoxelSize
    );
    
    // Vertex helpers
    static int32 AddVertex(
        FVoxelMeshData& MeshData,
        const FVector& Position,
        const FVector& Normal,
        const FVector2D& UV,
        const FProcMeshTangent& Tangent,
        const FColor& Color = FColor::White
    );
    
    // Get neighbor voxel
    static FVoxel GetNeighborVoxel(
        const FVoxelChunkData& ChunkData,
        int32 X, int32 Y, int32 Z,
        EVoxelFace Face
    );
    
    // Face direction helpers
    static FIntVector GetFaceDirection(EVoxelFace Face);
    
    // Optimization helpers
    static void OptimizeMeshData(FVoxelMeshData& MeshData);
    static void CalculateTangents(FVoxelMeshData& MeshData);
};

