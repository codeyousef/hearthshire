# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Heartshire is a voxel-based farming life simulation game built with Unreal Engine 5.6. The project has a unique philosophy: proving that AAA-quality games can be built entirely with Blueprints, without C++ game logic.

**Key Features:**
- 25cm refined voxels (4x more detail than typical 1m voxel games)
- Pure Blueprint architecture for accessibility
- Mobile-first optimization targeting both PC and mobile platforms
- Template + Seed system for hand-crafted worlds with procedural variation

## Development Commands

Currently, the project uses standard Unreal Engine workflows. The following scripts are planned but not yet implemented:

```bash
# Generate project files (TODO: implement in Scripts/)
Scripts/generate_project.sh

# Build the project (TODO: implement)
Scripts/build.sh

# Open in Unreal Editor (TODO: implement)
Scripts/open_editor.sh
```

For now, use standard Unreal Engine methods:
- Open Heartshire.uproject directly in Unreal Editor
- Use Generate Visual Studio Project Files from the .uproject context menu
- Build through Visual Studio (Heartshire.sln) or Unreal Editor

## Architecture Overview

### Blueprint Class Hierarchy

```
BP_VoxelWorld (Actor) - Main world manager
├── Chunk Manager Component - Handles chunk loading/unloading
├── Template Loader Component - Loads pre-built world templates
├── Seed System Component - Procedural variation
└── Performance Monitor Component - Tracks frame times

BP_VoxelChunk (Actor) - Individual chunk representation
├── ProceduralMeshComponent - Renders the chunk mesh
├── Voxel Data Container - 16³ array of voxel IDs
├── Mesh Generator Functions - Creates optimized meshes
└── LOD State Machine - Manages detail levels

BP_GreedyMesher (Blueprint Function Library)
├── CombineAdjacentFaces() - Merges same-material voxels
├── GenerateQuadFromVoxels() - Creates mesh quads
├── OptimizeMeshData() - Reduces vertex count
└── CalculateNormals() - Generates smooth normals
```

### Voxel System Pipeline

```
25cm Voxel Data → Blueprint Greedy Meshing → 
Blueprint Mesh Generation → Blueprint LOD Manager → 
ProceduralMeshComponent → Final Render
```

## Performance Guidelines

### Critical Performance Targets

- **Blueprint Execution**: Must stay under 8ms per frame
- **Chunk Processing**: Process 1-2 chunks per frame maximum
- **Mobile Memory**: Keep total runtime under 400MB
- **Draw Calls**: Target <500 on mobile, <1000 on PC

### Blueprint Optimization Rules

1. **NEVER use Tick events** - Use timers or event-driven updates
2. **Limit loop iterations** - Process in chunks of 100-200 items
3. **Cache calculated values** - Store and reuse mesh data
4. **Pool components** - Pre-spawn 50 ProceduralMeshComponents
5. **Use soft object references** - Especially for mobile builds

### LOD System Implementation

- **LOD 0**: Full 25cm detail (0-50m)
- **LOD 1**: 50cm equivalent (50-100m)  
- **LOD 2**: 1m equivalent (100-200m)
- **LOD 3**: Billboard imposters (200m+)

## Development Constraints

### Pure Blueprint Approach

This project avoids C++ for game logic to prove accessibility. When implementing features:
- Use Blueprint Function Libraries for reusable logic
- Leverage Blueprint Nativization for performance-critical paths in shipping builds
- Profile Blueprint execution time constantly (stat ScriptTime)

### Mobile-First Design

Every feature must work on mid-range mobile devices:
- Test on actual devices early and often
- Simplify math operations where possible
- Batch draw calls aggressively
- Monitor memory usage against 400MB limit

### Voxel Size Implications

The 25cm voxel size (vs traditional 1m) means:
- 64x more voxels per cubic meter
- More aggressive optimization required
- Greedy meshing is critical for performance
- Chunk size limited to 16³ for mobile compatibility

## Current Development Phase

**Phase 0.5: Refined Voxel Implementation** (Current)
- Implementing Blueprint greedy meshing for 25cm voxels
- Target: 60 FPS with multiple chunks loaded
- Focus: Optimizing mesh generation pipeline

Next phases leading to Kickstarter in 3 months:
- Phase 1.0: Core farming mechanics
- Phase 1.5: Building with templates
- Phase 2.0: Polish and Kickstarter prep

## Testing Approach

When implementing new features:
1. **Profile first**: Check Blueprint execution time
2. **Test on mobile**: Verify performance on target devices
3. **Monitor memory**: Track against platform limits
4. **Verify visuals**: Maintain "painted world" aesthetic

## Key Technical Decisions

1. **ProceduralMeshComponent** over Voxel plugins for full control
2. **16³ chunk size** optimized for mobile memory
3. **1 byte per voxel** for material index (supports 256 materials)
4. **Event-driven architecture** to avoid frame-dependent updates
5. **Template + Seed system** instead of full procedural generation

## File Organization

- **Content/Blueprints/**: All Blueprint classes
- **Content/Materials/**: Voxel materials and instances
- **Content/Templates/**: Pre-built world templates
- **Docs/**: Design documents and roadmap
- **Scripts/**: (TODO) Automation scripts

## Common Development Tasks

When working on the voxel system:
1. Check performance impact with stat ScriptTime
2. Test chunk generation time stays under 10ms
3. Verify memory usage per chunk stays under 10KB
4. Ensure LOD transitions are smooth

When optimizing for mobile:
1. Profile on actual devices, not just Mobile Preview
2. Check draw call count with stat RHI
3. Monitor texture memory with stat TextureMemory
4. Verify simplified shaders are used