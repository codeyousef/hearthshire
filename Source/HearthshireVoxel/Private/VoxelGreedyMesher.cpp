// Copyright Epic Games, Inc. All Rights Reserved.

#include "VoxelGreedyMesher.h"
#include "VoxelMeshGenerator.h"
#include "VoxelPerformanceStats.h"
#include "HearthshireVoxelModule.h"

void FVoxelGreedyMesher::GenerateGreedyMesh(
    const FVoxelChunkData& ChunkData,
    TArray<FGreedyQuad>& OutQuads)
{
#if VOXEL_ENABLE_STATS
    SCOPE_CYCLE_COUNTER(STAT_GreedyMeshing);
#endif
    
    OutQuads.Empty();
    
    // Process each of the 6 face directions
    for (int32 FaceIndex = 0; FaceIndex < 6; FaceIndex++)
    {
        EVoxelFace Face = static_cast<EVoxelFace>(FaceIndex);
        ProcessFaceDirection(ChunkData, Face, OutQuads);
    }
}

void FVoxelGreedyMesher::ProcessFaceDirection(
    const FVoxelChunkData& ChunkData,
    EVoxelFace Face,
    TArray<FGreedyQuad>& OutQuads)
{
    int32 PrimaryAxis, UAxis, VAxis;
    GetFaceAxes(Face, PrimaryAxis, UAxis, VAxis);
    
    const FIntVector ChunkSize = ChunkData.ChunkSize.ToIntVector();
    const int32 SliceCount = ChunkSize[PrimaryAxis];
    
    // Process each slice perpendicular to the face normal
    for (int32 Slice = 0; Slice < SliceCount; Slice++)
    {
        TArray<FFaceMask> Mask;
        FIntVector MaskDimensions;
        
        // Create visibility mask for this slice
        CreateFaceMask(ChunkData, Face, Slice, Mask, MaskDimensions);
        
        // Extract greedy quads from the mask
        ExtractQuadsFromMask(Mask, MaskDimensions, Face, Slice, OutQuads);
    }
}

void FVoxelGreedyMesher::CreateFaceMask(
    const FVoxelChunkData& ChunkData,
    EVoxelFace Face,
    int32 SliceIndex,
    TArray<FFaceMask>& OutMask,
    FIntVector& OutMaskDimensions)
{
    int32 PrimaryAxis, UAxis, VAxis;
    GetFaceAxes(Face, PrimaryAxis, UAxis, VAxis);
    
    const FIntVector ChunkSize = ChunkData.ChunkSize.ToIntVector();
    OutMaskDimensions.X = ChunkSize[UAxis];
    OutMaskDimensions.Y = ChunkSize[VAxis];
    OutMaskDimensions.Z = 1;
    
    const int32 MaskSize = OutMaskDimensions.X * OutMaskDimensions.Y;
    OutMask.SetNum(MaskSize);
    
    // Fill the mask with face visibility data
    for (int32 V = 0; V < OutMaskDimensions.Y; V++)
    {
        for (int32 U = 0; U < OutMaskDimensions.X; U++)
        {
            // Convert UV coordinates to 3D position
            FIntVector VoxelPos = MaskToVoxelPosition(U, V, SliceIndex, Face);
            
            const FVoxel CurrentVoxel = ChunkData.GetVoxel(VoxelPos.X, VoxelPos.Y, VoxelPos.Z);
            const int32 MaskIndex = GetMaskIndex(U, V, OutMaskDimensions);
            
            if (CurrentVoxel.IsAir())
            {
                OutMask[MaskIndex] = FFaceMask(EVoxelMaterial::Air, false);
            }
            else
            {
                bool bFaceVisible = IsFaceVisible(ChunkData, VoxelPos.X, VoxelPos.Y, VoxelPos.Z, Face);
                OutMask[MaskIndex] = FFaceMask(CurrentVoxel.Material, bFaceVisible);
            }
        }
    }
}

void FVoxelGreedyMesher::ExtractQuadsFromMask(
    TArray<FFaceMask>& Mask,
    const FIntVector& MaskDimensions,
    EVoxelFace Face,
    int32 SliceIndex,
    TArray<FGreedyQuad>& OutQuads)
{
    // Iterate through the mask and find unprocessed visible faces
    for (int32 V = 0; V < MaskDimensions.Y; V++)
    {
        for (int32 U = 0; U < MaskDimensions.X; U++)
        {
            const int32 MaskIndex = GetMaskIndex(U, V, MaskDimensions);
            FFaceMask& CurrentMask = Mask[MaskIndex];
            
            // Skip if not visible or already processed
            if (!CurrentMask.bVisible)
            {
                continue;
            }
            
            // Found a visible face - extend it as far as possible
            const EVoxelMaterial Material = CurrentMask.Material;
            const FIntVector StartPos(U, V, 0);
            
            // Extend the quad to find optimal size
            FIntVector QuadSize = ExtendQuad(Mask, MaskDimensions, StartPos, Material);
            
            // Create the greedy quad
            FIntVector VoxelPos = MaskToVoxelPosition(U, V, SliceIndex, Face);
            FGreedyQuad Quad(VoxelPos, QuadSize, Face, Material);
            OutQuads.Add(Quad);
            
            // Mark the area as processed
            MarkQuadProcessed(Mask, MaskDimensions, StartPos, QuadSize);
        }
    }
}

FIntVector FVoxelGreedyMesher::ExtendQuad(
    const TArray<FFaceMask>& Mask,
    const FIntVector& MaskDimensions,
    const FIntVector& StartPos,
    EVoxelMaterial Material)
{
    FIntVector QuadSize(1, 1, 1);
    
    // First, extend along U axis
    int32 MaxU = StartPos.X + 1;
    while (MaxU < MaskDimensions.X)
    {
        const int32 TestIndex = GetMaskIndex(MaxU, StartPos.Y, MaskDimensions);
        const FFaceMask& TestMask = Mask[TestIndex];
        
        if (!TestMask.bVisible || TestMask.Material != Material)
        {
            break;
        }
        
        MaxU++;
    }
    QuadSize.X = MaxU - StartPos.X;
    
    // Then, extend along V axis
    int32 MaxV = StartPos.Y + 1;
    while (MaxV < MaskDimensions.Y)
    {
        // Check if entire row can be added
        bool bCanExtend = true;
        for (int32 U = StartPos.X; U < StartPos.X + QuadSize.X; U++)
        {
            const int32 TestIndex = GetMaskIndex(U, MaxV, MaskDimensions);
            const FFaceMask& TestMask = Mask[TestIndex];
            
            if (!TestMask.bVisible || TestMask.Material != Material)
            {
                bCanExtend = false;
                break;
            }
        }
        
        if (!bCanExtend)
        {
            break;
        }
        
        MaxV++;
    }
    QuadSize.Y = MaxV - StartPos.Y;
    
    return QuadSize;
}

void FVoxelGreedyMesher::MarkQuadProcessed(
    TArray<FFaceMask>& Mask,
    const FIntVector& MaskDimensions,
    const FIntVector& StartPos,
    const FIntVector& QuadSize)
{
    for (int32 V = StartPos.Y; V < StartPos.Y + QuadSize.Y; V++)
    {
        for (int32 U = StartPos.X; U < StartPos.X + QuadSize.X; U++)
        {
            const int32 MaskIndex = GetMaskIndex(U, V, MaskDimensions);
            Mask[MaskIndex].bVisible = false;
        }
    }
}

void FVoxelGreedyMesher::GetFaceAxes(EVoxelFace Face, int32& PrimaryAxis, int32& UAxis, int32& VAxis)
{
    switch (Face)
    {
        case EVoxelFace::Front:  // +Y
        case EVoxelFace::Back:   // -Y
            PrimaryAxis = 1; // Y
            UAxis = 0;       // X
            VAxis = 2;       // Z
            break;
            
        case EVoxelFace::Right:  // +X
        case EVoxelFace::Left:   // -X
            PrimaryAxis = 0; // X
            UAxis = 1;       // Y
            VAxis = 2;       // Z
            break;
            
        case EVoxelFace::Top:    // +Z
        case EVoxelFace::Bottom: // -Z
            PrimaryAxis = 2; // Z
            UAxis = 0;       // X
            VAxis = 1;       // Y
            break;
    }
}

FIntVector FVoxelGreedyMesher::MaskToVoxelPosition(
    int32 U, int32 V, int32 SliceIndex,
    EVoxelFace Face)
{
    FIntVector Position;
    
    switch (Face)
    {
        case EVoxelFace::Front:  // +Y
        case EVoxelFace::Back:   // -Y
            Position.X = U;
            Position.Y = SliceIndex;
            Position.Z = V;
            break;
            
        case EVoxelFace::Right:  // +X
        case EVoxelFace::Left:   // -X
            Position.X = SliceIndex;
            Position.Y = U;
            Position.Z = V;
            break;
            
        case EVoxelFace::Top:    // +Z
        case EVoxelFace::Bottom: // -Z
            Position.X = U;
            Position.Y = V;
            Position.Z = SliceIndex;
            break;
    }
    
    return Position;
}

bool FVoxelGreedyMesher::IsFaceVisible(
    const FVoxelChunkData& ChunkData,
    int32 X, int32 Y, int32 Z,
    EVoxelFace Face)
{
    const FVoxel CurrentVoxel = ChunkData.GetVoxel(X, Y, Z);
    if (CurrentVoxel.IsAir())
    {
        return false;
    }
    
    const FVoxel NeighborVoxel = GetNeighborVoxel(ChunkData, X, Y, Z, Face);
    
    // Face is visible if neighbor is air or transparent with different material
    return NeighborVoxel.IsAir() || 
           (NeighborVoxel.IsTransparent() && CurrentVoxel.Material != NeighborVoxel.Material);
}

FVoxel FVoxelGreedyMesher::GetNeighborVoxel(
    const FVoxelChunkData& ChunkData,
    int32 X, int32 Y, int32 Z,
    EVoxelFace Face)
{
    FIntVector Offset;
    
    switch (Face)
    {
        case EVoxelFace::Front:  Offset = FIntVector(0, 1, 0); break;
        case EVoxelFace::Back:   Offset = FIntVector(0, -1, 0); break;
        case EVoxelFace::Right:  Offset = FIntVector(1, 0, 0); break;
        case EVoxelFace::Left:   Offset = FIntVector(-1, 0, 0); break;
        case EVoxelFace::Top:    Offset = FIntVector(0, 0, 1); break;
        case EVoxelFace::Bottom: Offset = FIntVector(0, 0, -1); break;
        default:
            // Should never happen with valid EVoxelFace values
            Offset = FIntVector::ZeroValue;
            break;
    }
    
    return ChunkData.GetVoxel(X + Offset.X, Y + Offset.Y, Z + Offset.Z);
}

void FVoxelGreedyMesher::ConvertQuadsToMesh(
    const TArray<FGreedyQuad>& Quads,
    FVoxelMeshData& OutMeshData,
    float VoxelSize)
{
    OutMeshData.Clear();
    
    // Reserve space for all quads
    const int32 VerticesPerQuad = 4;
    const int32 IndicesPerQuad = 6;
    OutMeshData.Reserve(Quads.Num() * VerticesPerQuad, Quads.Num() * IndicesPerQuad);
    
    for (const FGreedyQuad& Quad : Quads)
    {
        // Calculate base position and size in world units
        const FVector BasePos = FVector(Quad.Position) * VoxelSize;
        const FVector QuadWorldSize = FVector(Quad.Size) * VoxelSize;
        
        // Get face axes
        int32 PrimaryAxis, UAxis, VAxis;
        GetFaceAxes(Quad.Face, PrimaryAxis, UAxis, VAxis);
        
        // Calculate vertices based on face orientation and quad size
        FVector Vertices[4];
        FVector Normal = FVoxelMeshGenerator::GetFaceNormal(Quad.Face);
        
        // Adjust vertex positions based on quad size
        switch (Quad.Face)
        {
            case EVoxelFace::Front: // +Y
                Vertices[0] = BasePos + FVector(0, VoxelSize, 0);
                Vertices[1] = BasePos + FVector(QuadWorldSize.X, VoxelSize, 0);
                Vertices[2] = BasePos + FVector(QuadWorldSize.X, VoxelSize, QuadWorldSize.Y);
                Vertices[3] = BasePos + FVector(0, VoxelSize, QuadWorldSize.Y);
                break;
                
            case EVoxelFace::Back: // -Y
                Vertices[0] = BasePos + FVector(QuadWorldSize.X, 0, 0);
                Vertices[1] = BasePos + FVector(0, 0, 0);
                Vertices[2] = BasePos + FVector(0, 0, QuadWorldSize.Y);
                Vertices[3] = BasePos + FVector(QuadWorldSize.X, 0, QuadWorldSize.Y);
                break;
                
            case EVoxelFace::Right: // +X
                Vertices[0] = BasePos + FVector(VoxelSize, 0, 0);
                Vertices[1] = BasePos + FVector(VoxelSize, QuadWorldSize.X, 0);
                Vertices[2] = BasePos + FVector(VoxelSize, QuadWorldSize.X, QuadWorldSize.Y);
                Vertices[3] = BasePos + FVector(VoxelSize, 0, QuadWorldSize.Y);
                break;
                
            case EVoxelFace::Left: // -X
                Vertices[0] = BasePos + FVector(0, QuadWorldSize.X, 0);
                Vertices[1] = BasePos + FVector(0, 0, 0);
                Vertices[2] = BasePos + FVector(0, 0, QuadWorldSize.Y);
                Vertices[3] = BasePos + FVector(0, QuadWorldSize.X, QuadWorldSize.Y);
                break;
                
            case EVoxelFace::Top: // +Z
                Vertices[0] = BasePos + FVector(0, 0, VoxelSize);
                Vertices[1] = BasePos + FVector(QuadWorldSize.X, 0, VoxelSize);
                Vertices[2] = BasePos + FVector(QuadWorldSize.X, QuadWorldSize.Y, VoxelSize);
                Vertices[3] = BasePos + FVector(0, QuadWorldSize.Y, VoxelSize);
                break;
                
            case EVoxelFace::Bottom: // -Z
                Vertices[0] = BasePos + FVector(0, QuadWorldSize.Y, 0);
                Vertices[1] = BasePos + FVector(QuadWorldSize.X, QuadWorldSize.Y, 0);
                Vertices[2] = BasePos + FVector(QuadWorldSize.X, 0, 0);
                Vertices[3] = BasePos + FVector(0, 0, 0);
                break;
        }
        
        // Calculate UVs based on quad size
        FVector2D UVs[4];
        UVs[0] = FVector2D(0, 0);
        UVs[1] = FVector2D(Quad.Size[UAxis], 0);
        UVs[2] = FVector2D(Quad.Size[UAxis], Quad.Size[VAxis]);
        UVs[3] = FVector2D(0, Quad.Size[VAxis]);
        
        // Add the quad to the mesh
        FVoxelMeshGenerator::AddQuad(
            OutMeshData,
            Vertices[0], Vertices[1], Vertices[2], Vertices[3],
            Normal,
            UVs[0], UVs[1], UVs[2], UVs[3],
            Quad.Material
        );
    }
    
    // Update mesh statistics
    OutMeshData.TriangleCount = OutMeshData.Triangles.Num() / 3;
    OutMeshData.VertexCount = OutMeshData.Vertices.Num();
}

float FVoxelGreedyMesher::CalculateReductionPercent(
    int32 OriginalFaceCount,
    int32 OptimizedQuadCount)
{
    if (OriginalFaceCount == 0)
    {
        return 0.0f;
    }
    
    return (1.0f - (float)OptimizedQuadCount / (float)OriginalFaceCount) * 100.0f;
}