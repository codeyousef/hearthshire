// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VoxelTypes.h"

/**
 * High-performance greedy meshing implementation
 * Achieves 70-90% triangle reduction through face merging
 */
class HEARTHSHIREVOXEL_API FVoxelGreedyMesher
{
public:
    struct FGreedyQuad
    {
        FIntVector Position;    // Base position in voxel coordinates
        FIntVector Size;        // Size in voxel units (width, height on face plane)
        EVoxelFace Face;        // Which face this quad represents
        EVoxelMaterial Material; // Material type
        
        FGreedyQuad()
        {
            Position = FIntVector::ZeroValue;
            Size = FIntVector(1, 1, 1);
            Face = EVoxelFace::Front;
            Material = EVoxelMaterial::Air;
        }
        
        FGreedyQuad(const FIntVector& InPos, const FIntVector& InSize, EVoxelFace InFace, EVoxelMaterial InMat)
            : Position(InPos), Size(InSize), Face(InFace), Material(InMat)
        {}
    };
    
    // Main greedy meshing function - generates optimized quads
    static void GenerateGreedyMesh(
        const FVoxelChunkData& ChunkData,
        TArray<FGreedyQuad>& OutQuads
    );
    
    // Convert greedy quads to renderable mesh data
    static void ConvertQuadsToMesh(
        const TArray<FGreedyQuad>& Quads,
        FVoxelMeshData& OutMeshData,
        float VoxelSize = 25.0f
    );
    
    // Generate greedy mesh from raw voxel array
    static void GenerateGreedyMeshFromData(
        const TArray<EVoxelMaterial>& VoxelData,
        const FVoxelChunkSize& ChunkSize,
        float VoxelSize,
        FVoxelMeshData& OutMeshData
    );
    
    // Get mesh reduction statistics
    static float CalculateReductionPercent(
        int32 OriginalFaceCount,
        int32 OptimizedQuadCount
    );
    
private:
    // Face mask for tracking which voxel faces need processing
    struct FFaceMask
    {
        EVoxelMaterial Material;
        bool bVisible;
        
        FFaceMask() : Material(EVoxelMaterial::Air), bVisible(false) {}
        FFaceMask(EVoxelMaterial InMat, bool InVisible) : Material(InMat), bVisible(InVisible) {}
        
        bool CanMergeWith(const FFaceMask& Other) const
        {
            return bVisible && Other.bVisible && Material == Other.Material;
        }
    };
    
    // Process a single face direction with greedy algorithm
    static void ProcessFaceDirection(
        const FVoxelChunkData& ChunkData,
        EVoxelFace Face,
        TArray<FGreedyQuad>& OutQuads
    );
    
    // Create face visibility mask for a slice
    static void CreateFaceMask(
        const FVoxelChunkData& ChunkData,
        EVoxelFace Face,
        int32 SliceIndex,
        TArray<FFaceMask>& OutMask,
        FIntVector& OutMaskDimensions
    );
    
    // Extract greedy quads from face mask
    static void ExtractQuadsFromMask(
        TArray<FFaceMask>& Mask,
        const FIntVector& MaskDimensions,
        EVoxelFace Face,
        int32 SliceIndex,
        TArray<FGreedyQuad>& OutQuads
    );
    
    // Try to extend a quad in the given direction
    static FIntVector ExtendQuad(
        const TArray<FFaceMask>& Mask,
        const FIntVector& MaskDimensions,
        const FIntVector& StartPos,
        EVoxelMaterial Material
    );
    
    // Mark quad area as processed in mask
    static void MarkQuadProcessed(
        TArray<FFaceMask>& Mask,
        const FIntVector& MaskDimensions,
        const FIntVector& StartPos,
        const FIntVector& QuadSize
    );
    
    // Helper to convert 2D mask coordinates to 1D array index
    static FORCEINLINE int32 GetMaskIndex(int32 U, int32 V, const FIntVector& MaskDimensions)
    {
        return U + V * MaskDimensions.X;
    }
    
    // Get the axis indices for a given face (returns primary axis, U axis, V axis)
    static void GetFaceAxes(EVoxelFace Face, int32& PrimaryAxis, int32& UAxis, int32& VAxis);
    
    // Convert mask UV coordinates to 3D voxel position
    static FIntVector MaskToVoxelPosition(
        int32 U, int32 V, int32 SliceIndex,
        EVoxelFace Face
    );
    
    // Check if a voxel face is visible (needs to be rendered)
    static bool IsFaceVisible(
        const FVoxelChunkData& ChunkData,
        int32 X, int32 Y, int32 Z,
        EVoxelFace Face
    );
    
    // Get neighbor voxel in the direction of the face
    static FVoxel GetNeighborVoxel(
        const FVoxelChunkData& ChunkData,
        int32 X, int32 Y, int32 Z,
        EVoxelFace Face
    );
};