// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ProceduralMeshComponent.h"
#include "VoxelTypes.generated.h"

// Forward declarations
class UMaterialInterface;

/**
 * Voxel material types - supports up to 256 materials
 */
UENUM(BlueprintType)
enum class EVoxelMaterial : uint8
{
    Air = 0         UMETA(DisplayName = "Air"),
    Grass = 1       UMETA(DisplayName = "Grass"),
    Dirt = 2        UMETA(DisplayName = "Dirt"),
    Stone = 3       UMETA(DisplayName = "Stone"),
    Wood = 4        UMETA(DisplayName = "Wood"),
    Leaves = 5      UMETA(DisplayName = "Leaves"),
    Sand = 6        UMETA(DisplayName = "Sand"),
    Water = 7       UMETA(DisplayName = "Water"),
    Snow = 8        UMETA(DisplayName = "Snow"),
    Ice = 9         UMETA(DisplayName = "Ice"),
    // Add more materials as needed up to 255
    
    Max = 255       UMETA(Hidden)
};

/**
 * Compact voxel representation - 1 byte per voxel
 */
USTRUCT(BlueprintType)
struct HEARTHSHIREVOXEL_API FVoxel
{
    GENERATED_BODY()
    
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Voxel")
    EVoxelMaterial Material;
    
    FVoxel() : Material(EVoxelMaterial::Air) {}
    FVoxel(EVoxelMaterial InMaterial) : Material(InMaterial) {}
    
    FORCEINLINE bool IsAir() const { return Material == EVoxelMaterial::Air; }
    FORCEINLINE bool IsSolid() const { return Material != EVoxelMaterial::Air; }
    FORCEINLINE bool IsTransparent() const { return Material == EVoxelMaterial::Water || Material == EVoxelMaterial::Ice; }
    
    FORCEINLINE bool operator==(const FVoxel& Other) const { return Material == Other.Material; }
    FORCEINLINE bool operator!=(const FVoxel& Other) const { return Material != Other.Material; }
};

/**
 * Voxel face for mesh generation
 */
UENUM(BlueprintType)
enum class EVoxelFace : uint8
{
    Front = 0,      // +Y
    Back = 1,       // -Y
    Right = 2,      // +X
    Left = 3,       // -X
    Top = 4,        // +Z
    Bottom = 5,     // -Z
    Max = 6
};

/**
 * Platform-specific chunk size configuration
 */
USTRUCT(BlueprintType)
struct HEARTHSHIREVOXEL_API FVoxelChunkSize
{
    GENERATED_BODY()
    
    UPROPERTY(BlueprintReadOnly, Category = "Voxel")
    int32 X;
    
    UPROPERTY(BlueprintReadOnly, Category = "Voxel")
    int32 Y;
    
    UPROPERTY(BlueprintReadOnly, Category = "Voxel")
    int32 Z;
    
    FVoxelChunkSize()
    {
#if VOXEL_MOBILE_PLATFORM
        X = Y = Z = 16; // 16x16x16 for mobile
#else
        X = Y = Z = 32; // 32x32x32 for PC
#endif
    }
    
    FVoxelChunkSize(int32 Size) : X(Size), Y(Size), Z(Size) {}
    FVoxelChunkSize(int32 InX, int32 InY, int32 InZ) : X(InX), Y(InY), Z(InZ) {}
    
    FORCEINLINE int32 GetVoxelCount() const { return X * Y * Z; }
    FORCEINLINE FIntVector ToIntVector() const { return FIntVector(X, Y, Z); }
};

/**
 * Voxel chunk data container
 */
USTRUCT()
struct HEARTHSHIREVOXEL_API FVoxelChunkData
{
    GENERATED_BODY()
    
    // Flat array of voxels (size = ChunkSize.X * ChunkSize.Y * ChunkSize.Z)
    TArray<FVoxel> Voxels;
    
    // Chunk dimensions
    FVoxelChunkSize ChunkSize;
    
    // Chunk position in world grid
    FIntVector ChunkPosition;
    
    // Dirty flag for regeneration
    bool bIsDirty;
    
    // Generation timestamp
    double GenerationTime;
    
    FVoxelChunkData()
    {
        ChunkSize = FVoxelChunkSize();
        ChunkPosition = FIntVector::ZeroValue;
        bIsDirty = true;
        GenerationTime = 0.0;
        
        // Pre-allocate voxel array
        const int32 VoxelCount = ChunkSize.GetVoxelCount();
        Voxels.SetNum(VoxelCount);
    }
    
    // Get voxel at local position
    FORCEINLINE FVoxel GetVoxel(int32 X, int32 Y, int32 Z) const
    {
        if (X < 0 || X >= ChunkSize.X || Y < 0 || Y >= ChunkSize.Y || Z < 0 || Z >= ChunkSize.Z)
        {
            return FVoxel(EVoxelMaterial::Air);
        }
        
        const int32 Index = X + Y * ChunkSize.X + Z * ChunkSize.X * ChunkSize.Y;
        return Voxels[Index];
    }
    
    // Set voxel at local position
    FORCEINLINE void SetVoxel(int32 X, int32 Y, int32 Z, const FVoxel& Voxel)
    {
        if (X >= 0 && X < ChunkSize.X && Y >= 0 && Y < ChunkSize.Y && Z >= 0 && Z < ChunkSize.Z)
        {
            const int32 Index = X + Y * ChunkSize.X + Z * ChunkSize.X * ChunkSize.Y;
            Voxels[Index] = Voxel;
            bIsDirty = true;
        }
    }
    
    // Get voxel index from position
    FORCEINLINE int32 GetIndex(int32 X, int32 Y, int32 Z) const
    {
        return X + Y * ChunkSize.X + Z * ChunkSize.X * ChunkSize.Y;
    }
    
    // Clear all voxels
    void Clear()
    {
        for (auto& Voxel : Voxels)
        {
            Voxel = FVoxel(EVoxelMaterial::Air);
        }
        bIsDirty = true;
    }
};

/**
 * Mesh data for procedural generation
 */
USTRUCT(BlueprintType)
struct HEARTHSHIREVOXEL_API FVoxelMeshData
{
    GENERATED_BODY()
    
    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UV0;
    TArray<FProcMeshTangent> Tangents;
    TArray<FColor> VertexColors;
    
    // Material sections
    TMap<EVoxelMaterial, int32> MaterialSections;
    TArray<int32> MaterialTriangles;
    
    // Statistics
    int32 TriangleCount;
    int32 VertexCount;
    float GenerationTimeMs;
    
    FVoxelMeshData()
    {
        TriangleCount = 0;
        VertexCount = 0;
        GenerationTimeMs = 0.0f;
    }
    
    void Clear()
    {
        Vertices.Empty();
        Triangles.Empty();
        Normals.Empty();
        UV0.Empty();
        Tangents.Empty();
        VertexColors.Empty();
        MaterialSections.Empty();
        MaterialTriangles.Empty();
        TriangleCount = 0;
        VertexCount = 0;
    }
    
    // Reserve memory for expected mesh size
    void Reserve(int32 ExpectedVertices, int32 ExpectedTriangles)
    {
        Vertices.Reserve(ExpectedVertices);
        Triangles.Reserve(ExpectedTriangles);
        Normals.Reserve(ExpectedVertices);
        UV0.Reserve(ExpectedVertices);
        Tangents.Reserve(ExpectedVertices);
        VertexColors.Reserve(ExpectedVertices);
    }
};

/**
 * Configuration for voxel material properties
 */
USTRUCT(BlueprintType)
struct HEARTHSHIREVOXEL_API FVoxelMaterialConfig
{
    GENERATED_BODY()
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
    UMaterialInterface* Material;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
    FLinearColor BaseColor;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float Roughness;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float Metallic;
    
    FVoxelMaterialConfig()
    {
        Material = nullptr;
        BaseColor = FLinearColor::White;
        Roughness = 0.5f;
        Metallic = 0.0f;
    }
};

/**
 * Material configuration data asset
 */
UCLASS(BlueprintType)
class HEARTHSHIREVOXEL_API UVoxelMaterialSet : public UDataAsset
{
    GENERATED_BODY()
    
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Materials")
    TMap<EVoxelMaterial, FVoxelMaterialConfig> Materials;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Materials")
    UMaterialInterface* DefaultMaterial;
    
    UVoxelMaterialSet();
    
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    UMaterialInterface* GetMaterial(EVoxelMaterial VoxelMaterial) const;
    
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    FLinearColor GetBaseColor(EVoxelMaterial VoxelMaterial) const;
};

/**
 * LOD configuration
 */
USTRUCT(BlueprintType)
struct HEARTHSHIREVOXEL_API FVoxelLODConfig
{
    GENERATED_BODY()
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LOD", meta = (ClampMin = "0.0"))
    float Distance;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LOD", meta = (ClampMin = "0.25", ClampMax = "4.0"))
    float VoxelScale;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LOD")
    bool bUseGreedyMeshing;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LOD")
    bool bGenerateCollision;
    
    FVoxelLODConfig()
    {
        Distance = 0.0f;
        VoxelScale = 1.0f;
        bUseGreedyMeshing = true;
        bGenerateCollision = true;
    }
};

/**
 * Debug visualization configuration
 */
USTRUCT(BlueprintType)
struct HEARTHSHIREVOXEL_API FVoxelDebugConfig
{
    GENERATED_BODY()
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug", meta = (DisplayName = "Show Chunk Bounds"))
    bool bShowChunkBounds;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug", meta = (DisplayName = "Show Voxel Grid"))
    bool bShowVoxelGrid;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug", meta = (DisplayName = "Show Performance Stats"))
    bool bShowPerformanceStats;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug", meta = (DisplayName = "Show LOD Info"))
    bool bShowLODInfo;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug", meta = (DisplayName = "Debug Color", HideAlphaChannel))
    FLinearColor DebugColor;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug", meta = (ClampMin = "1", ClampMax = "16", DisplayName = "Grid Step Size"))
    int32 GridStepSize;
    
    FVoxelDebugConfig()
    {
        bShowChunkBounds = false;
        bShowVoxelGrid = false;
        bShowPerformanceStats = false;
        bShowLODInfo = false;
        DebugColor = FLinearColor::Green;
        GridStepSize = 4;
    }
};

/**
 * Optimization configuration
 */
USTRUCT(BlueprintType)
struct HEARTHSHIREVOXEL_API FVoxelOptimizationConfig
{
    GENERATED_BODY()
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization", meta = (DisplayName = "Enable Greedy Meshing"))
    bool bUseGreedyMeshing;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization", meta = (DisplayName = "Enable Multithreading"))
    bool bUseMultithreading;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization", meta = (DisplayName = "Enable Async Mesh Generation"))
    bool bUseAsyncGeneration;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization", meta = (DisplayName = "Enable Index Optimization"))
    bool bOptimizeIndices;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization", meta = (ClampMin = "1", ClampMax = "8", DisplayName = "Worker Thread Count"))
    int32 WorkerThreadCount;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimization", meta = (ClampMin = "1", ClampMax = "10", DisplayName = "Chunks Per Frame"))
    int32 MaxChunksPerFrame;
    
    FVoxelOptimizationConfig()
    {
        bUseGreedyMeshing = true;
        bUseMultithreading = true;
        bUseAsyncGeneration = true;
        bOptimizeIndices = true;
        WorkerThreadCount = 4;
        MaxChunksPerFrame = 5;
    }
};

/**
 * Terrain generation configuration
 */
USTRUCT(BlueprintType)
struct HEARTHSHIREVOXEL_API FVoxelGenerationConfig
{
    GENERATED_BODY()
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation", meta = (DisplayName = "Terrain Type"))
    FName TerrainPreset;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation", meta = (ClampMin = "0.001", ClampMax = "1.0", DisplayName = "Noise Scale"))
    float NoiseScale;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation", meta = (ClampMin = "1", ClampMax = "100", DisplayName = "Height Scale"))
    float HeightScale;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation", meta = (ClampMin = "1", ClampMax = "8", DisplayName = "Octaves"))
    int32 Octaves;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation", meta = (DisplayName = "Seed"))
    int32 Seed;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation", meta = (DisplayName = "Base Height"))
    int32 BaseHeight;
    
    FVoxelGenerationConfig()
    {
        TerrainPreset = "Default";
        NoiseScale = 0.01f;
        HeightScale = 50.0f;
        Octaves = 4;
        Seed = 12345;
        BaseHeight = 10;
    }
};

/**
 * Performance statistics
 */
USTRUCT(BlueprintType)
struct HEARTHSHIREVOXEL_API FVoxelPerformanceStats
{
    GENERATED_BODY()
    
    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    float MeshGenerationTimeMs;
    
    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    float GreedyMeshingTimeMs;
    
    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    int32 TriangleCount;
    
    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    int32 VertexCount;
    
    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    float TriangleReductionPercent;
    
    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    int32 ActiveChunks;
    
    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    float MemoryUsageMB;
    
    FVoxelPerformanceStats()
    {
        MeshGenerationTimeMs = 0.0f;
        GreedyMeshingTimeMs = 0.0f;
        TriangleCount = 0;
        VertexCount = 0;
        TriangleReductionPercent = 0.0f;
        ActiveChunks = 0;
        MemoryUsageMB = 0.0f;
    }
};