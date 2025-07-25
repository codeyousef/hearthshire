# [[Hearthshire]] - Technical Requirements Document (Pure Blueprint Approach)

## Version 3.0 - Refined Voxel Aesthetic

---

## Core Technical Update: 25cm Voxels

### The Refined Voxel Revolution

**Moving from 1m "Minecraft blocks" to 25cm refined voxels**

- **4x more detail** per dimension = 64x more voxels per chunk
- **Smoother terrain** with gentle slopes and curves
- **Architectural detail** allowing actual windows, doors, trim
- **Character expressions** possible with smaller voxel faces
- **"Painted world" aesthetic** - voxels visible up close, beautiful from afar

---

## New Architecture Overview

### Pure Blueprint Philosophy

**Pre-Generated Templates + Seed Variation + Blueprint-Optimized Rendering = Unique Beautiful Worlds**

- **25cm Voxels**: Small enough for beauty, large enough for performance
- **Blueprint Greedy Meshing**: Visual scripting for mesh optimization
- **Blueprint LOD System**: Distance-based detail reduction
- **Smart Blueprint Architecture**: Performance through clever design

---

## System Requirements (Updated for 25cm Voxels)

### PC Requirements

#### Minimum

- **OS**: Windows 10 64-bit
- **Processor**: Intel i5-8400 / AMD Ryzen 5 2600
- **Memory**: 8 GB RAM
- **Graphics**: GTX 1060 6GB / RX 580 8GB
- **Storage**: 30 GB available space (increased for detailed voxel data)
- **Voxel Rendering**: 60 FPS with refined voxels in view

#### Recommended

- **OS**: Windows 11 64-bit
- **Processor**: Intel i7-10700K / AMD Ryzen 7 3700X
- **Memory**: 16 GB RAM
- **Graphics**: RTX 3070 / RX 6700 XT
- **Storage**: 30 GB SSD space
- **Voxel Rendering**: 120 FPS with maximum voxel detail

### Mobile Requirements (Optimized for 25cm Voxels)

#### iOS

- **Device**: iPhone 12 or newer (A14 Bionic minimum)
- **OS**: iOS 15.0 or later
- **Storage**: 6 GB available space
- **Performance**: 30-60 FPS with LOD optimization

#### Android

- **OS**: Android 11.0 or later
- **Chipset**: Snapdragon 865 or newer
- **RAM**: 8 GB minimum
- **Storage**: 6 GB available space
- **GPU**: Adreno 650, Mali-G77, or newer

---

## Pure Blueprint Voxel Pipeline

### Entirely Visual Scripting Based

```
25cm Voxel Data → Blueprint Greedy Meshing → 
Blueprint Mesh Generation → Blueprint LOD Manager → 
ProceduralMeshComponent → Final Render
```

### Blueprint Optimization Strategies

**1. Smart Greedy Meshing (Blueprint)**

- Custom Blueprint functions for face combining
- Data structure optimization using arrays
- Chunk-based processing to avoid frame drops
- Async blueprint execution where possible

**2. Blueprint LOD System**

- **LOD 0**: Full 25cm detail (0-50m)
- **LOD 1**: 50cm equivalent (50-100m)
- **LOD 2**: 1m equivalent (100-200m)
- **LOD 3**: Billboard imposters (200m+)

**3. Blueprint Culling System**

- Distance-based chunk activation
- Simple frustum checks in Blueprint
- Underground chunk sleeping system
- Event-driven updates instead of tick

**4. Mobile Blueprint Optimizations**

- Reduced chunk size (16³ instead of 32³)
- Simplified Blueprint execution paths
- Pooled ProceduralMeshComponents
- Staggered chunk updates across frames

---

## Performance Targets (25cm Voxels, Pure Blueprint)

### Frame Rate Targets

|Platform|Target FPS|Voxel Count|Render Distance|
|---|---|---|---|
|PC High-end|100 FPS|~1.5M visible|250m|
|PC Mid-range|60 FPS|~750K visible|150m|
|Mobile High-end|45 FPS|~400K visible|100m|
|Mobile Mid-range|30 FPS|~200K visible|75m|

_Note: Slightly reduced from C++ targets due to Blueprint overhead_

### Memory Usage

- **Voxel Data**: 1 byte per voxel (material index)
- **Chunk Size**: 16³ voxels = 4KB per chunk
- **Active Chunks**: ~500 chunks loaded = 2MB voxel data
- **Mesh Cache**: ~300MB for generated geometry
- **Blueprint Overhead**: ~50MB
- **Total Runtime**: <400MB on mobile, <800MB on PC

---

## Blueprint Architecture

### Core Blueprint Classes

**BP_VoxelWorld (Actor)**

```
Blueprint Components:
├── Chunk Manager Component
├── Template Loader Component
├── Seed System Component
└── Performance Monitor Component
```

**BP_VoxelChunk (Actor)**

```
Blueprint Components:
├── ProceduralMeshComponent
├── Voxel Data Container (Array)
├── Mesh Generator Functions
├── LOD State Machine
└── Material Instance Dynamic
```

**BP_GreedyMesher (Blueprint Function Library)**

```
Pure Functions:
├── CombineAdjacentFaces()
├── GenerateQuadFromVoxels()
├── OptimizeMeshData()
└── CalculateNormals()
```

---

## Blueprint Performance Tricks

### 1. Frame Distribution

```
Instead of: Process all chunks every frame
We use: Process 1-2 chunks per frame in rotation
Result: Smooth performance, slight generation delay
```

### 2. Pooled Components

```
Pre-spawn 50 ProceduralMeshComponents
Reuse instead of create/destroy
Massive performance gain on mobile
```

### 3. Smart Updates

```
Event-driven instead of Tick
Only update chunks when:
- Player moves significantly
- Voxel is modified
- LOD distance crossed
```

### 4. Blueprint Nativization

```
For final builds only:
- Convert critical blueprints to C++
- 20-40% performance gain
- Still develop in pure Blueprint
```

---

## Mobile Rendering Strategy (Pure Blueprint)

### Chunk Streaming State Machine

```
States (Blueprint Enum):
├── Unloaded
├── Loading (spread across frames)
├── Loaded_LOD3 (billboard)
├── Loaded_LOD2 (simplified)
├── Loaded_LOD1 (medium)
└── Loaded_LOD0 (full detail)
```

### Quality Settings

**Ultra (High-end Mobile)**

- 25cm voxels with smart batching
- 100m render distance
- Simple real-time shadows
- Reduced particle density

**High (Mid-range Mobile)**

- 25cm voxels with aggressive LOD
- 75m render distance
- No real-time shadows
- Minimal particles

**Medium (Older Devices)**

- Larger apparent voxel size via LOD
- 50m render distance
- Baked lighting only
- No particles

---

## Blueprint Development Best Practices

### Performance Guidelines

1. **Avoid Tick Events**
    
    - Use timers or events instead
    - If needed, use low frequency (0.1s+)
2. **Minimize Loop Iterations**
    
    - Process in chunks of 100-200
    - Use "Delay Until Next Tick" for large operations
3. **Cache Everything**
    
    - Store calculated values
    - Reuse mesh data when possible
    - Pre-calculate common operations
4. **Profile Constantly**
    
    - Use Blueprint profiler
    - Monitor stat unit
    - Test on target hardware early

### Mobile-Specific Blueprint Rules

1. **Simplified Math**
    
    - Avoid complex calculations per-frame
    - Pre-calculate when possible
    - Use lookup tables for common values
2. **Reduced Draw Calls**
    
    - Batch similar materials
    - Use texture atlases
    - Minimize unique material instances
3. **Memory Management**
    
    - Clear references when unused
    - Use soft object references
    - Implement chunk unloading

---

## Development Workflow Updates

### Phase 0.5: Pure Blueprint Voxel System (Current)

1. **25cm voxel grid implementation** ✓
2. **Blueprint greedy meshing** 🎯 IN PROGRESS
3. **Blueprint LOD system**
4. **Mobile performance validation**

### Weekly Milestones

**Week 1**: Blueprint Greedy Meshing

- Face combination algorithm
- Vertex generation functions
- Normal calculation

**Week 2**: Blueprint LOD System

- Distance-based detail reduction
- Smooth transitions
- Billboard generation for far chunks

**Week 3**: Mobile Optimization

- Chunk update distribution
- Component pooling
- Memory management

**Week 4**: Polish & Profiling

- Performance profiling
- Visual quality tuning
- Platform-specific settings

---

## Success Metrics

### Technical Goals

- **Generation Time**: <100ms per chunk on mobile
- **Memory Usage**: <400MB on mid-range mobile
- **Frame Rate**: Stable 30 FPS minimum
- **Blueprint Execution**: <8ms per frame

### Visual Goals

- **Smooth Terrain**: Natural slopes and curves
- **Detail Level**: Visible architectural elements
- **Draw Distance**: Atmospheric fog hiding LOD transitions
- **Artistic Vision**: "Painted world" aesthetic achieved

---

_"Beautiful worlds, built entirely with Blueprints, optimized for everyone."_