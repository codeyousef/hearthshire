// Copyright Epic Games, Inc. All Rights Reserved.

#include "VoxelMeshGenerator.h"
#include "VoxelGreedyMesher.h"
#include "VoxelPerformanceStats.h"
#include "ProceduralMeshComponent.h"
#include "Logging/LogMacros.h"
#include "HearthshireVoxelModule.h"

// FVoxelMeshGenerator Implementation

void FVoxelMeshGenerator::GenerateBasicMesh(
    const FVoxelChunkData& ChunkData,
    FVoxelMeshData& OutMeshData,
    const FGenerationConfig& Config)
{
#if VOXEL_ENABLE_STATS
    SCOPE_CYCLE_COUNTER(STAT_VoxelMeshGeneration);
#endif
    
    const double StartTime = FPlatformTime::Seconds();
    
    OutMeshData.Clear();
    
    // Reserve space for worst case
    const int32 MaxFaces = ChunkData.ChunkSize.GetVoxelCount() * 6;
    OutMeshData.Reserve(MaxFaces * 4, MaxFaces * 6);
    
    UE_LOG(LogHearthshireVoxel, Log, TEXT("GenerateBasicMesh: Starting basic mesh generation for chunk %s"),
        *ChunkData.ChunkPosition.ToString());
    
    int32 SolidVoxelCount = 0;
    int32 FacesGenerated = 0;
    
    // Iterate through all voxels
    for (int32 Z = 0; Z < ChunkData.ChunkSize.Z; Z++)
    {
        for (int32 Y = 0; Y < ChunkData.ChunkSize.Y; Y++)
        {
            for (int32 X = 0; X < ChunkData.ChunkSize.X; X++)
            {
                const FVoxel Voxel = ChunkData.GetVoxel(X, Y, Z);
                
                // Skip air voxels
                if (Voxel.IsAir())
                {
                    continue;
                }
                
                SolidVoxelCount++;
                const FVector Position = FVector(X, Y, Z) * Config.VoxelSize;
                
                // Check each face
                for (int32 FaceIndex = 0; FaceIndex < 6; FaceIndex++)
                {
                    EVoxelFace Face = static_cast<EVoxelFace>(FaceIndex);
                    
                    if (IsFaceVisible(ChunkData, X, Y, Z, Face))
                    {
                        AddFace(OutMeshData, Position, Face, Voxel.Material, Config.VoxelSize);
                        FacesGenerated++;
                    }
                }
            }
        }
    }
    
    UE_LOG(LogHearthshireVoxel, Log, TEXT("GenerateBasicMesh: Solid voxels: %d, Faces generated: %d"), 
        SolidVoxelCount, FacesGenerated);
    
    // Post-processing
    if (Config.bOptimizeIndices)
    {
        OptimizeMeshData(OutMeshData);
    }
    
    if (Config.bGenerateTangents)
    {
        CalculateTangents(OutMeshData);
    }
    
    // Update stats
    OutMeshData.TriangleCount = OutMeshData.Triangles.Num() / 3;
    OutMeshData.VertexCount = OutMeshData.Vertices.Num();
    OutMeshData.GenerationTimeMs = (FPlatformTime::Seconds() - StartTime) * 1000.0f;
}

void FVoxelMeshGenerator::GenerateGreedyMesh(
    const FVoxelChunkData& ChunkData,
    FVoxelMeshData& OutMeshData,
    const FGenerationConfig& Config)
{
#if VOXEL_ENABLE_STATS
    SCOPE_CYCLE_COUNTER(STAT_GreedyMeshing);
#endif
    
    const double StartTime = FPlatformTime::Seconds();
    
    // Generate greedy quads
    TArray<FVoxelGreedyMesher::FGreedyQuad> Quads;
    FVoxelGreedyMesher::GenerateGreedyMesh(ChunkData, Quads);
    
    // Convert quads to mesh
    FVoxelGreedyMesher::ConvertQuadsToMesh(Quads, OutMeshData, Config.VoxelSize);
    
    
    // Post-processing
    if (Config.bGenerateTangents)
    {
        CalculateTangents(OutMeshData);
    }
    
    // Update stats
    OutMeshData.TriangleCount = OutMeshData.Triangles.Num() / 3;
    OutMeshData.VertexCount = OutMeshData.Vertices.Num();
    OutMeshData.GenerationTimeMs = (FPlatformTime::Seconds() - StartTime) * 1000.0f;
    
}

void FVoxelMeshGenerator::GenerateLODMesh(
    const FVoxelChunkData& ChunkData,
    FVoxelMeshData& OutMeshData,
    int32 LODLevel,
    const FGenerationConfig& Config)
{
    // TODO: Implement LOD generation
    // For now, just use basic mesh generation
    GenerateBasicMesh(ChunkData, OutMeshData, Config);
}

void FVoxelMeshGenerator::ApplyMeshToComponent(
    UProceduralMeshComponent* Component,
    const FVoxelMeshData& MeshData,
    UVoxelMaterialSet* MaterialSet)
{
    if (!Component)
    {
        UE_LOG(LogHearthshireVoxel, Error, TEXT("ApplyMeshToComponent: Component is null"));
        return;
    }
    
    Component->ClearAllMeshSections();
    
    if (MeshData.Vertices.Num() == 0)
    {
        UE_LOG(LogHearthshireVoxel, Warning, TEXT("ApplyMeshToComponent: No vertices to apply"));
        return;
    }
    
    
    
    // Configure component for opaque rendering
    Component->bUseAsyncCooking = true;
    Component->bUseComplexAsSimpleCollision = false;
    Component->SetCastShadow(true);
    Component->bRenderCustomDepth = false;
    Component->bRenderInMainPass = true;
    Component->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Component->SetCollisionResponseToAllChannels(ECR_Block);
    
    // Check if we have material sections
    if (MeshData.MaterialSections.Num() == 0)
    {
        UE_LOG(LogHearthshireVoxel, Warning, TEXT("ApplyMeshToComponent: No material sections, creating single section"));
        
        // Validate vertex colors are opaque
        TArray<FColor> ValidatedColors = MeshData.VertexColors;
        for (FColor& Color : ValidatedColors)
        {
            Color.A = 255; // Force full opacity
        }
        
        // Create a single mesh section with all data (fallback for basic visibility)
        Component->CreateMeshSection(
            0, // Section index
            MeshData.Vertices,
            MeshData.Triangles,
            MeshData.Normals,
            MeshData.UV0,
            ValidatedColors,
            MeshData.Tangents,
            true // Create collision
        );
        
        // Try to apply a default material if available
        if (MaterialSet)
        {
            UMaterialInterface* DefaultMaterial = MaterialSet->GetMaterial(EVoxelMaterial::Stone);
            if (DefaultMaterial)
            {
                Component->SetMaterial(0, DefaultMaterial);
                UE_LOG(LogHearthshireVoxel, Log, TEXT("ApplyMeshToComponent: Applied default stone material"));
            }
            else
            {
                UE_LOG(LogHearthshireVoxel, Warning, TEXT("ApplyMeshToComponent: No default material found"));
            }
        }
    }
    else
    {
        // Create a single mesh section if we have multiple materials
        // The material sections map indicates which materials are present but we'll use a single section
        // to avoid duplicate geometry issues
        
        // Validate vertex colors are opaque
        TArray<FColor> ValidatedColors = MeshData.VertexColors;
        for (FColor& Color : ValidatedColors)
        {
            Color.A = 255; // Force full opacity
        }
        
        // Create a single mesh section with all geometry
        Component->CreateMeshSection(
            0, // Single section index
            MeshData.Vertices,
            MeshData.Triangles,
            MeshData.Normals,
            MeshData.UV0,
            ValidatedColors,
            MeshData.Tangents,
            true // Create collision
        );
        
        // Apply the most common material or default
        if (MaterialSet && MeshData.MaterialSections.Num() > 0)
        {
            // Get the first material (we'll improve this later with proper per-face materials)
            EVoxelMaterial FirstMaterial = MeshData.MaterialSections.begin().Key();
            UMaterialInterface* Material = MaterialSet->GetMaterial(FirstMaterial);
            
            if (Material)
            {
                Component->SetMaterial(0, Material);
            }
        }
        
    }
    
    // Update bounds and mark render state dirty
    Component->UpdateBounds();
    Component->MarkRenderStateDirty();
}

void FVoxelMeshGenerator::AddFace(
    FVoxelMeshData& MeshData,
    const FVector& Position,
    EVoxelFace Face,
    EVoxelMaterial Material,
    float VoxelSize)
{
    FVector Vertices[4];
    GetFaceVertices(Face, Position, VoxelSize, Vertices);
    
    FVector2D UVs[4];
    GetFaceUVs(Face, UVs);
    
    FVector Normal = GetFaceNormal(Face);
    
    AddQuad(MeshData, Vertices[0], Vertices[1], Vertices[2], Vertices[3], Normal, UVs[0], UVs[1], UVs[2], UVs[3], Material);
}

void FVoxelMeshGenerator::AddQuad(
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
    EVoxelMaterial Material)
{
    // Calculate tangent
    FVector Edge1 = V1 - V0;
    FVector Edge2 = V2 - V0;
    FVector2D DeltaUV1 = UV1 - UV0;
    FVector2D DeltaUV2 = UV2 - UV0;
    
    float Div = DeltaUV1.X * DeltaUV2.Y - DeltaUV2.X * DeltaUV1.Y;
    FVector Tangent = FVector::ZeroVector;
    
    if (!FMath::IsNearlyZero(Div))
    {
        float InvDiv = 1.0f / Div;
        Tangent = (Edge1 * DeltaUV2.Y - Edge2 * DeltaUV1.Y) * InvDiv;
        Tangent.Normalize();
    }
    
    FProcMeshTangent ProcTangent(Tangent, false);
    
    // Add vertices with fully opaque white color
    int32 StartIndex = MeshData.Vertices.Num();
    FColor OpaqueWhite(255, 255, 255, 255); // Ensure full opacity
    
    AddVertex(MeshData, V0, Normal, UV0, ProcTangent, OpaqueWhite);
    AddVertex(MeshData, V1, Normal, UV1, ProcTangent, OpaqueWhite);
    AddVertex(MeshData, V2, Normal, UV2, ProcTangent, OpaqueWhite);
    AddVertex(MeshData, V3, Normal, UV3, ProcTangent, OpaqueWhite);
    
    // Add triangles (two triangles per quad) with correct winding order
    // UE5 uses clockwise winding for front faces when viewed from outside
    int32 SectionIndex = GetOrCreateMaterialSection(MeshData, Material);
    
    // First triangle: V0, V1, V2 (clockwise when viewed from outside)
    MeshData.Triangles.Add(StartIndex + 0);
    MeshData.Triangles.Add(StartIndex + 1);
    MeshData.Triangles.Add(StartIndex + 2);
    
    // Second triangle: V0, V2, V3 (clockwise when viewed from outside)
    MeshData.Triangles.Add(StartIndex + 0);
    MeshData.Triangles.Add(StartIndex + 2);
    MeshData.Triangles.Add(StartIndex + 3);
}

int32 FVoxelMeshGenerator::AddVertex(
    FVoxelMeshData& MeshData,
    const FVector& Position,
    const FVector& Normal,
    const FVector2D& UV,
    const FProcMeshTangent& Tangent,
    const FColor& Color)
{
    int32 Index = MeshData.Vertices.Add(Position);
    MeshData.Normals.Add(Normal);
    MeshData.UV0.Add(UV);
    MeshData.Tangents.Add(Tangent);
    MeshData.VertexColors.Add(Color);
    
    return Index;
}

bool FVoxelMeshGenerator::IsFaceVisible(
    const FVoxelChunkData& ChunkData,
    int32 X, int32 Y, int32 Z,
    EVoxelFace Face)
{
    // First check if the current voxel is solid
    FVoxel CurrentVoxel = ChunkData.GetVoxel(X, Y, Z);
    if (CurrentVoxel.IsAir())
    {
        return false; // Air voxels don't generate faces
    }
    
    // Get the direction offset for this face
    FIntVector Direction = GetFaceDirection(Face);
    int32 NeighborX = X + Direction.X;
    int32 NeighborY = Y + Direction.Y;
    int32 NeighborZ = Z + Direction.Z;
    
    // Check if neighbor is out of bounds - if so, face is visible
    if (NeighborX < 0 || NeighborX >= ChunkData.ChunkSize.X ||
        NeighborY < 0 || NeighborY >= ChunkData.ChunkSize.Y ||
        NeighborZ < 0 || NeighborZ >= ChunkData.ChunkSize.Z)
    {
        return true; // Face is at chunk boundary, so it's visible
    }
    
    // Get the neighbor voxel
    FVoxel Neighbor = ChunkData.GetVoxel(NeighborX, NeighborY, NeighborZ);
    
    // Face is visible if neighbor is air or transparent with different material
    return Neighbor.IsAir() || 
           (Neighbor.IsTransparent() && CurrentVoxel.Material != Neighbor.Material);
}

FVoxel FVoxelMeshGenerator::GetNeighborVoxel(
    const FVoxelChunkData& ChunkData,
    int32 X, int32 Y, int32 Z,
    EVoxelFace Face)
{
    FIntVector Direction = GetFaceDirection(Face);
    return ChunkData.GetVoxel(X + Direction.X, Y + Direction.Y, Z + Direction.Z);
}

FIntVector FVoxelMeshGenerator::GetFaceDirection(EVoxelFace Face)
{
    switch (Face)
    {
        case EVoxelFace::Front:  return FIntVector(0, 1, 0);   // +Y
        case EVoxelFace::Back:   return FIntVector(0, -1, 0);  // -Y
        case EVoxelFace::Right:  return FIntVector(1, 0, 0);   // +X
        case EVoxelFace::Left:   return FIntVector(-1, 0, 0);  // -X
        case EVoxelFace::Top:    return FIntVector(0, 0, 1);   // +Z
        case EVoxelFace::Bottom: return FIntVector(0, 0, -1);  // -Z
        default: return FIntVector::ZeroValue;
    }
}

FVector FVoxelMeshGenerator::GetFaceNormal(EVoxelFace Face)
{
    switch (Face)
    {
        case EVoxelFace::Front:  return FVector(0, 1, 0);   // +Y
        case EVoxelFace::Back:   return FVector(0, -1, 0);  // -Y
        case EVoxelFace::Right:  return FVector(1, 0, 0);   // +X
        case EVoxelFace::Left:   return FVector(-1, 0, 0);  // -X
        case EVoxelFace::Top:    return FVector(0, 0, 1);   // +Z
        case EVoxelFace::Bottom: return FVector(0, 0, -1);  // -Z
        default: return FVector::ZeroVector;
    }
}

void FVoxelMeshGenerator::GetFaceVertices(EVoxelFace Face, const FVector& Position, float Size, FVector OutVertices[4])
{
    const float S = Size;
    
    // IMPORTANT: Vertices must be ordered clockwise when viewed from outside the cube
    // This ensures proper face culling and lighting in UE5
    
    switch (Face)
    {
        case EVoxelFace::Front: // +Y (facing positive Y)
            OutVertices[0] = Position + FVector(0, S, 0);
            OutVertices[1] = Position + FVector(S, S, 0);
            OutVertices[2] = Position + FVector(S, S, S);
            OutVertices[3] = Position + FVector(0, S, S);
            break;
            
        case EVoxelFace::Back: // -Y (facing negative Y)
            OutVertices[0] = Position + FVector(S, 0, 0);
            OutVertices[1] = Position + FVector(0, 0, 0);
            OutVertices[2] = Position + FVector(0, 0, S);
            OutVertices[3] = Position + FVector(S, 0, S);
            break;
            
        case EVoxelFace::Right: // +X (facing positive X)
            OutVertices[0] = Position + FVector(S, S, 0);
            OutVertices[1] = Position + FVector(S, 0, 0);
            OutVertices[2] = Position + FVector(S, 0, S);
            OutVertices[3] = Position + FVector(S, S, S);
            break;
            
        case EVoxelFace::Left: // -X (facing negative X)
            OutVertices[0] = Position + FVector(0, 0, 0);
            OutVertices[1] = Position + FVector(0, S, 0);
            OutVertices[2] = Position + FVector(0, S, S);
            OutVertices[3] = Position + FVector(0, 0, S);
            break;
            
        case EVoxelFace::Top: // +Z (facing positive Z - up)
            OutVertices[0] = Position + FVector(0, 0, S);
            OutVertices[1] = Position + FVector(S, 0, S);
            OutVertices[2] = Position + FVector(S, S, S);
            OutVertices[3] = Position + FVector(0, S, S);
            break;
            
        case EVoxelFace::Bottom: // -Z (facing negative Z - down)
            OutVertices[0] = Position + FVector(0, S, 0);
            OutVertices[1] = Position + FVector(S, S, 0);
            OutVertices[2] = Position + FVector(S, 0, 0);
            OutVertices[3] = Position + FVector(0, 0, 0);
            break;
    }
}

void FVoxelMeshGenerator::GetFaceUVs(EVoxelFace Face, FVector2D OutUVs[4])
{
    // Simple UV mapping - can be improved with texture atlasing
    OutUVs[0] = FVector2D(0, 0);
    OutUVs[1] = FVector2D(1, 0);
    OutUVs[2] = FVector2D(1, 1);
    OutUVs[3] = FVector2D(0, 1);
}

int32 FVoxelMeshGenerator::GetOrCreateMaterialSection(FVoxelMeshData& MeshData, EVoxelMaterial Material)
{
    if (int32* ExistingSection = MeshData.MaterialSections.Find(Material))
    {
        return *ExistingSection;
    }
    
    int32 NewSection = MeshData.MaterialSections.Num();
    MeshData.MaterialSections.Add(Material, NewSection);
    return NewSection;
}

void FVoxelMeshGenerator::OptimizeMeshData(FVoxelMeshData& MeshData)
{
    // TODO: Implement vertex welding and index optimization
}

void FVoxelMeshGenerator::CalculateTangents(FVoxelMeshData& MeshData)
{
    // Tangents are already calculated per-quad in AddQuad
    // This function can be used for more sophisticated tangent calculation if needed
}