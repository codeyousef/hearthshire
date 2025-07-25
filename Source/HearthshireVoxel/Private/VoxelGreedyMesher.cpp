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
    
    
    // Count visible faces before optimization
    int32 VisibleFaceCount = 0;
    
    // Process each of the 6 face directions
    for (int32 FaceIndex = 0; FaceIndex < 6; FaceIndex++)
    {
        EVoxelFace Face = static_cast<EVoxelFace>(FaceIndex);
        int32 QuadsBefore = OutQuads.Num();
        ProcessFaceDirection(ChunkData, Face, OutQuads);
        int32 QuadsAdded = OutQuads.Num() - QuadsBefore;
        
        UE_LOG(LogHearthshireVoxel, VeryVerbose, TEXT("  Face %d: Generated %d quads"), FaceIndex, QuadsAdded);
    }
    
    // Count faces by type for debugging
    int32 FaceCounts[6] = {0};
    for (const FGreedyQuad& Quad : OutQuads)
    {
        FaceCounts[(int32)Quad.Face]++;
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
            
            // Debug logging for large quads
            if (QuadSize.X > 1 || QuadSize.Y > 1)
            {
                UE_LOG(LogHearthshireVoxel, VeryVerbose, TEXT("    Created greedy quad: Pos=%s, Size=%s, Face=%d, Material=%d"),
                    *VoxelPos.ToString(), *QuadSize.ToString(), (int32)Face, (int32)Material);
            }
            
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
    
    // Get the direction offset for this face
    FIntVector Offset;
    switch (Face)
    {
        case EVoxelFace::Front:  Offset = FIntVector(0, 1, 0); break;   // +Y
        case EVoxelFace::Back:   Offset = FIntVector(0, -1, 0); break;  // -Y
        case EVoxelFace::Right:  Offset = FIntVector(1, 0, 0); break;   // +X
        case EVoxelFace::Left:   Offset = FIntVector(-1, 0, 0); break;  // -X
        case EVoxelFace::Top:    Offset = FIntVector(0, 0, 1); break;   // +Z
        case EVoxelFace::Bottom: Offset = FIntVector(0, 0, -1); break;  // -Z
        default: Offset = FIntVector::ZeroValue; break;
    }
    
    int32 NeighborX = X + Offset.X;
    int32 NeighborY = Y + Offset.Y;
    int32 NeighborZ = Z + Offset.Z;
    
    // Check if neighbor is out of bounds - if so, face is visible
    if (NeighborX < 0 || NeighborX >= ChunkData.ChunkSize.X ||
        NeighborY < 0 || NeighborY >= ChunkData.ChunkSize.Y ||
        NeighborZ < 0 || NeighborZ >= ChunkData.ChunkSize.Z)
    {
        return true; // Face is at chunk boundary, so it's visible
    }
    
    const FVoxel NeighborVoxel = ChunkData.GetVoxel(NeighborX, NeighborY, NeighborZ);
    
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

// Vertex key for spatial hashing - handles floating point precision
struct FVertexKey
{
    int32 X, Y, Z;
    
    FVertexKey(const FVector& Pos, float Scale = 100.0f)
    {
        X = FMath::RoundToInt(Pos.X * Scale);
        Y = FMath::RoundToInt(Pos.Y * Scale);
        Z = FMath::RoundToInt(Pos.Z * Scale);
    }
    
    bool operator==(const FVertexKey& Other) const
    {
        return X == Other.X && Y == Other.Y && Z == Other.Z;
    }
    
    friend uint32 GetTypeHash(const FVertexKey& Key)
    {
        return HashCombine(HashCombine(GetTypeHash(Key.X), GetTypeHash(Key.Y)), GetTypeHash(Key.Z));
    }
};

void FVoxelGreedyMesher::ConvertQuadsToMesh(
    const TArray<FGreedyQuad>& Quads,
    FVoxelMeshData& OutMeshData,
    float VoxelSize)
{
    OutMeshData.Clear();
    
    // Vertex deduplication map: Position -> Vertex Index
    TMap<FVertexKey, int32> VertexMap;
    
    
    // Reserve approximate space (assuming ~25% vertex sharing)
    const int32 EstimatedVertices = FMath::Max(Quads.Num(), 100);
    const int32 EstimatedTriangles = Quads.Num() * 6;
    OutMeshData.Reserve(EstimatedVertices, EstimatedTriangles);
    
    int32 DuplicateVerticesSaved = 0;
    int32 QuadIndex = 0;
    
    for (const FGreedyQuad& Quad : Quads)
    {
        
        // Calculate base position in world units
        const FVector BasePos = FVector(Quad.Position) * VoxelSize;
        
        // Get face axes
        int32 PrimaryAxis, UAxis, VAxis;
        GetFaceAxes(Quad.Face, PrimaryAxis, UAxis, VAxis);
        
        // Calculate size in world units for U and V axes
        // IMPORTANT: Quad.Size.X is always U dimension, Quad.Size.Y is always V dimension
        // regardless of which world axes they map to
        FVector SizeVector = FVector::ZeroVector;
        SizeVector[UAxis] = Quad.Size.X * VoxelSize;  // U dimension size
        SizeVector[VAxis] = Quad.Size.Y * VoxelSize;  // V dimension size
        
        // Calculate vertices based on face orientation
        FVector Vertices[4];
        FVector Normal = FVoxelMeshGenerator::GetFaceNormal(Quad.Face);
        
        // Position vertices correctly based on face direction
        switch (Quad.Face)
        {
            case EVoxelFace::Front: // +Y (facing positive Y)
                Vertices[0] = BasePos + FVector(0, VoxelSize, 0);
                Vertices[1] = BasePos + FVector(SizeVector.X, VoxelSize, 0);
                Vertices[2] = BasePos + FVector(SizeVector.X, VoxelSize, SizeVector.Z);
                Vertices[3] = BasePos + FVector(0, VoxelSize, SizeVector.Z);
                break;
                
            case EVoxelFace::Back: // -Y (facing negative Y)
                Vertices[0] = BasePos + FVector(SizeVector.X, 0, 0);
                Vertices[1] = BasePos + FVector(0, 0, 0);
                Vertices[2] = BasePos + FVector(0, 0, SizeVector.Z);
                Vertices[3] = BasePos + FVector(SizeVector.X, 0, SizeVector.Z);
                break;
                
            case EVoxelFace::Right: // +X (facing positive X)
                Vertices[0] = BasePos + FVector(VoxelSize, SizeVector.Y, 0);
                Vertices[1] = BasePos + FVector(VoxelSize, 0, 0);
                Vertices[2] = BasePos + FVector(VoxelSize, 0, SizeVector.Z);
                Vertices[3] = BasePos + FVector(VoxelSize, SizeVector.Y, SizeVector.Z);
                break;
                
            case EVoxelFace::Left: // -X (facing negative X)
                Vertices[0] = BasePos + FVector(0, 0, 0);
                Vertices[1] = BasePos + FVector(0, SizeVector.Y, 0);
                Vertices[2] = BasePos + FVector(0, SizeVector.Y, SizeVector.Z);
                Vertices[3] = BasePos + FVector(0, 0, SizeVector.Z);
                break;
                
            case EVoxelFace::Top: // +Z (facing positive Z - up)
                Vertices[0] = BasePos + FVector(0, 0, VoxelSize);
                Vertices[1] = BasePos + FVector(SizeVector.X, 0, VoxelSize);
                Vertices[2] = BasePos + FVector(SizeVector.X, SizeVector.Y, VoxelSize);
                Vertices[3] = BasePos + FVector(0, SizeVector.Y, VoxelSize);
                break;
                
            case EVoxelFace::Bottom: // -Z (facing negative Z - down)
                Vertices[0] = BasePos + FVector(0, SizeVector.Y, 0);
                Vertices[1] = BasePos + FVector(SizeVector.X, SizeVector.Y, 0);
                Vertices[2] = BasePos + FVector(SizeVector.X, 0, 0);
                Vertices[3] = BasePos + FVector(0, 0, 0);
                break;
        }
        
        // Check for unreasonable vertex positions
        
        // Validate vertex positions are reasonable
        for (int32 i = 0; i < 4; i++)
        {
            float Distance = Vertices[i].Size();
            if (Distance > 10000.0f) // More than 10000 units away
            {
                UE_LOG(LogHearthshireVoxel, Error, TEXT("UNREASONABLE VERTEX: Quad %d, Vertex %d at %s (Distance: %.1f)"),
                    QuadIndex, i, *Vertices[i].ToString(), Distance);
            }
        }
        
        // Calculate tangent for the quad
        FVector Edge1 = Vertices[1] - Vertices[0];
        FVector Edge2 = Vertices[2] - Vertices[0];
        FVector2D DeltaUV1 = FVector2D(Quad.Size.X, 0);      // U dimension
        FVector2D DeltaUV2 = FVector2D(Quad.Size.X, Quad.Size.Y);  // U and V dimensions
        
        float Div = DeltaUV1.X * DeltaUV2.Y - DeltaUV2.X * DeltaUV1.Y;
        FVector Tangent = FVector::ZeroVector;
        
        if (!FMath::IsNearlyZero(Div))
        {
            float InvDiv = 1.0f / Div;
            Tangent = (Edge1 * DeltaUV2.Y - Edge2 * DeltaUV1.Y) * InvDiv;
            Tangent.Normalize();
        }
        
        FProcMeshTangent ProcTangent(Tangent, false);
        FColor OpaqueWhite(255, 255, 255, 255);
        
        // Get or create vertex indices for each corner
        int32 VertexIndices[4];
        for (int32 i = 0; i < 4; i++)
        {
            FVertexKey Key(Vertices[i]);
            
            // Check if vertex already exists
            int32* ExistingIndex = VertexMap.Find(Key);
            
            if (ExistingIndex)
            {
                // Reuse existing vertex
                VertexIndices[i] = *ExistingIndex;
                DuplicateVerticesSaved++;
            }
            else
            {
                // Create new vertex
                int32 NewIndex = OutMeshData.Vertices.Num();
                VertexIndices[i] = NewIndex;
                VertexMap.Add(Key, NewIndex);
                
                
                // Add vertex data
                OutMeshData.Vertices.Add(Vertices[i]);
                OutMeshData.Normals.Add(Normal);
                OutMeshData.VertexColors.Add(OpaqueWhite);
                OutMeshData.Tangents.Add(ProcTangent);
                
                // Calculate UVs based on world position
                FVector2D UV = FVector2D::ZeroVector; // Initialize to zero
                switch (Quad.Face)
                {
                    case EVoxelFace::Front: // +Y
                    case EVoxelFace::Back:  // -Y
                        UV.X = Vertices[i].X / VoxelSize;
                        UV.Y = Vertices[i].Z / VoxelSize;
                        break;
                    case EVoxelFace::Right: // +X
                    case EVoxelFace::Left:  // -X
                        UV.X = Vertices[i].Y / VoxelSize;
                        UV.Y = Vertices[i].Z / VoxelSize;
                        break;
                    case EVoxelFace::Top:    // +Z
                    case EVoxelFace::Bottom: // -Z
                        UV.X = Vertices[i].X / VoxelSize;
                        UV.Y = Vertices[i].Y / VoxelSize;
                        break;
                    default:
                        // Should never happen, but handle it to avoid warning
                        UV.X = 0.0f;
                        UV.Y = 0.0f;
                        break;
                }
                
                // Normalize to 0-1 range for tiling
                UV.X = FMath::Fmod(UV.X, 1.0f);
                UV.Y = FMath::Fmod(UV.Y, 1.0f);
                if (UV.X < 0) UV.X += 1.0f;
                if (UV.Y < 0) UV.Y += 1.0f;
                
                OutMeshData.UV0.Add(UV);
            }
        }
        
        
        // Validate indices before adding
        int32 MaxIndex = OutMeshData.Vertices.Num();
        for (int32 i = 0; i < 4; i++)
        {
            if (VertexIndices[i] >= MaxIndex || VertexIndices[i] < 0)
            {
                UE_LOG(LogHearthshireVoxel, Error, TEXT("INVALID VERTEX INDEX: Quad %d, Vertex %d, Index %d (Max: %d)"),
                    QuadIndex, i, VertexIndices[i], MaxIndex - 1);
            }
        }
        
        // Add triangles with correct winding order
        // UE5 uses clockwise winding for front faces when viewed from outside
        
        // Debug logging for top faces
        if (Quad.Face == EVoxelFace::Top && QuadIndex < 3)
        {
            UE_LOG(LogHearthshireVoxel, Warning, TEXT("TOP FACE DEBUG - Quad %d:"), QuadIndex);
            UE_LOG(LogHearthshireVoxel, Warning, TEXT("  Normal: %s"), *Normal.ToString());
            UE_LOG(LogHearthshireVoxel, Warning, TEXT("  V0[%d]: %s"), VertexIndices[0], *OutMeshData.Vertices[VertexIndices[0]].ToString());
            UE_LOG(LogHearthshireVoxel, Warning, TEXT("  V1[%d]: %s"), VertexIndices[1], *OutMeshData.Vertices[VertexIndices[1]].ToString());
            UE_LOG(LogHearthshireVoxel, Warning, TEXT("  V2[%d]: %s"), VertexIndices[2], *OutMeshData.Vertices[VertexIndices[2]].ToString());
            UE_LOG(LogHearthshireVoxel, Warning, TEXT("  V3[%d]: %s"), VertexIndices[3], *OutMeshData.Vertices[VertexIndices[3]].ToString());
        }
        
        // Check if this is a top face (Face 4) which needs special winding
        if (Quad.Face == EVoxelFace::Top) // Face 4 = +Z
        {
            // For top faces viewed from above (+Z looking down)
            // Vertices are ordered: (0,0,Z), (X,0,Z), (X,Y,Z), (0,Y,Z)
            // We need counter-clockwise when viewed from above (which is clockwise from below)
            // First triangle: 0-3-1 (reversed from normal)
            OutMeshData.Triangles.Add(VertexIndices[0]);
            OutMeshData.Triangles.Add(VertexIndices[3]);
            OutMeshData.Triangles.Add(VertexIndices[1]);
            
            // Second triangle: 1-3-2 (reversed from normal)
            OutMeshData.Triangles.Add(VertexIndices[1]);
            OutMeshData.Triangles.Add(VertexIndices[3]);
            OutMeshData.Triangles.Add(VertexIndices[2]);
            
            if (QuadIndex < 3)
            {
                UE_LOG(LogHearthshireVoxel, Warning, TEXT("  TOP FACE: Using reversed winding (0-3-1, 1-3-2)"));
            }
        }
        else
        {
            // Normal winding order for all other faces
            // First triangle: 0-1-2
            OutMeshData.Triangles.Add(VertexIndices[0]);
            OutMeshData.Triangles.Add(VertexIndices[1]);
            OutMeshData.Triangles.Add(VertexIndices[2]);
            
            // Second triangle: 0-2-3
            OutMeshData.Triangles.Add(VertexIndices[0]);
            OutMeshData.Triangles.Add(VertexIndices[2]);
            OutMeshData.Triangles.Add(VertexIndices[3]);
        }
        
        // Add material section mapping
        int32 SectionIndex = FVoxelMeshGenerator::GetOrCreateMaterialSection(OutMeshData, Quad.Material);
        
        QuadIndex++;
    }
    
    // Update mesh statistics
    OutMeshData.TriangleCount = OutMeshData.Triangles.Num() / 3;
    OutMeshData.VertexCount = OutMeshData.Vertices.Num();
    
    
    // Validate all triangle indices
    int32 InvalidIndexCount = 0;
    for (int32 i = 0; i < OutMeshData.Triangles.Num(); i++)
    {
        if (OutMeshData.Triangles[i] >= OutMeshData.Vertices.Num() || OutMeshData.Triangles[i] < 0)
        {
            InvalidIndexCount++;
            UE_LOG(LogHearthshireVoxel, Error, TEXT("Invalid triangle index at position %d: %d (Max allowed: %d)"),
                i, OutMeshData.Triangles[i], OutMeshData.Vertices.Num() - 1);
        }
    }
    
    if (InvalidIndexCount > 0)
    {
        UE_LOG(LogHearthshireVoxel, Error, TEXT("CRITICAL: Found %d invalid triangle indices!"), InvalidIndexCount);
    }
    
    // Check for vertex position outliers
    float MaxReasonableDistance = 1000.0f * VoxelSize; // 1000 voxels max distance
    int32 OutlierCount = 0;
    
    for (int32 i = 0; i < OutMeshData.Vertices.Num(); i++)
    {
        const FVector& Vertex = OutMeshData.Vertices[i];
        float Distance = Vertex.Size();
        
        if (Distance > MaxReasonableDistance)
        {
            OutlierCount++;
            if (OutlierCount <= 5) // Log first 5 outliers
            {
                UE_LOG(LogHearthshireVoxel, Error, TEXT("Vertex %d is an outlier: %s (Distance: %.1f)"),
                    i, *Vertex.ToString(), Distance);
            }
        }
    }
    
    if (OutlierCount > 0)
    {
        UE_LOG(LogHearthshireVoxel, Error, TEXT("Found %d vertex position outliers!"), OutlierCount);
    }
    
    // Log vertex welding statistics
    float VerticesPerQuad = OutMeshData.Vertices.Num() > 0 ? 
        (float)OutMeshData.Vertices.Num() / (float)Quads.Num() : 0.0f;
    
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

void FVoxelGreedyMesher::GenerateGreedyMeshFromData(
    const TArray<EVoxelMaterial>& VoxelData,
    const FVoxelChunkSize& ChunkSize,
    float VoxelSize,
    FVoxelMeshData& OutMeshData)
{
    // Create a temporary chunk data structure
    FVoxelChunkData TempChunkData;
    TempChunkData.ChunkSize = ChunkSize;
    TempChunkData.ChunkPosition = FIntVector::ZeroValue;
    
    // Convert EVoxelMaterial array to FVoxel array
    TempChunkData.Voxels.SetNum(VoxelData.Num());
    for (int32 i = 0; i < VoxelData.Num(); i++)
    {
        TempChunkData.Voxels[i].Material = VoxelData[i];
    }
    
    // Generate greedy quads
    TArray<FGreedyQuad> Quads;
    GenerateGreedyMesh(TempChunkData, Quads);
    
    // Convert quads to mesh
    ConvertQuadsToMesh(Quads, OutMeshData, VoxelSize);
}