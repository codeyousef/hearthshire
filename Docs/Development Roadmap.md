# [[Hearthshire]] - Development Roadmap (Pure Blueprint + 25cm Voxels)

## From Fresh Start to Kickstarter Success - Refined Voxel Edition

---

## Overview: The 90-Minute Magic Demo (Now with Refined Voxels)

**Kickstarter Launch: End of Month 3**

The demo showcases:

- **Beautiful 25cm voxel worlds** that feel painted, not blocky
- **Pure Blueprint architecture** proving accessibility
- **Smooth terrain and detailed buildings** never before seen in voxel games
- **Mobile-optimized performance** with smart LOD system
- **"Your unique world"** with our template + seed system

---

## Phase 0: Blueprint Foundation (Week 1) ✅ **COMPLETED**

**Goal: Core voxel world with blueprint systems** ✅

### Achievements ✅

- [x] VoxelChunk Blueprint Class with ProceduralMeshComponent ✅
- [x] Basic 32x32x32 voxel structure ✅
- [x] Procedural mesh generation working ✅
- [x] **MAJOR BREAKTHROUGH**: Pipeline validated! ✅

---

## Phase 0.5: Refined Voxel Implementation (Week 1-2) 🎯 **CURRENT**

**Goal: 25cm voxels with Blueprint greedy meshing**

### This Week's Goals

- [ ] **Voxel Size Adjustment**: Change from 1m to 25cm voxels
    
    - [ ] Update chunk dimensions (16³ for performance)
    - [ ] Adjust world scale accordingly
    - [ ] Test memory usage
- [ ] **Blueprint Greedy Meshing**:
    
    - [ ] Face detection algorithm
    - [ ] Adjacent voxel combination
    - [ ] Optimal quad generation
    - [ ] Performance profiling
- [ ] **Basic Materials**:
    
    - [ ] 5 voxel types: Grass, Dirt, Stone, Wood, Leaves
    - [ ] Simple color variation per voxel
    - [ ] Material index system

### Success Metrics

- Generate terrain with visible 25cm detail
- 60 FPS with multiple chunks loaded
- Smooth slopes and curves visible
- Blueprint execution under 10ms

---

## Phase 1: Refined World Templates (Weeks 2-3)

**Goal: Hand-crafted biome templates showcasing 25cm voxel beauty**

### Week 2: Enhanced Template Tools

- [ ] **Voxel Painting Tools**: In-editor voxel placement
- [ ] **Refined Meadow Valley**:
    - [ ] Smooth rolling hills with 25cm precision
    - [ ] Mae's shop with architectural details
    - [ ] Flower patches with individual blooms
- [ ] **Detailed Asset Library**:
    - [ ] Doors (2x5 voxel minimum)
    - [ ] Windows (2x3 voxel minimum)
    - [ ] Fences, paths, decorations

### Week 3: Seed Variation for Refined Voxels

- [ ] **Smart Noise Functions**: Smooth terrain at 25cm scale
- [ ] **Detail Preservation**: Ensure variations don't break structures
- [ ] **Voxel-Aware Generation**: Prevent single-voxel artifacts
- [ ] **Performance Optimization**: LOD system foundation
- [ ] **Material Blending**: Natural transitions between voxel types

### Milestone Demo

"Three unique worlds, but you can see individual flowers in Mae's window box in each one"

---

## Phase 2: Blueprint Systems Enhancement (Weeks 4-5)

**Goal: Core gameplay with refined voxel interaction**

### Week 4: Precision Building System

- [ ] **25cm Grid Building**: Precise placement system
- [ ] **Multi-Voxel Tools**: Place/remove voxel groups
- [ ] **Smart Snapping**: Architectural alignment aids
- [ ] **Material Palette**: Quick material switching
- [ ] **Ghost Preview**: See before you place

### Week 5: Refined World Interaction

- [ ] **Smooth Terrain Modification**: Natural-looking changes
- [ ] **Voxel Physics**: Small debris when breaking
- [ ] **Particle Integration**: Voxel-sized particles
- [ ] **Save System**: Efficient 25cm voxel storage
- [ ] **LOD Integration**: Seamless detail transitions

### Milestone Demo

"Build a detailed cottage with actual window frames and decorative trim"

---

## Phase 3: Character & NPC Refinement (Weeks 6-7)

**Goal: Characters that match our refined world**

### Week 6: Detailed Character Models

- [ ] **Mae with Expressions**: 32-voxel wide head allows emotion
- [ ] **Visible Features**: Eyes, nose, mouth clearly defined
- [ ] **Clothing Details**: Aprons, buttons, pockets visible
- [ ] **Animation System**: Smooth movement despite voxels
- [ ] **Facial Animation**: Simple expression changes

### Week 7: World-Aware Behaviors

- [ ] **Precision Pathfinding**: Navigate 25cm detail
- [ ] **Environmental Interactions**: Sit on chairs, lean on fences
- [ ] **Detail-Aware Dialogue**: Comment on player's fine work
- [ ] **Building Participation**: Place individual voxels
- [ ] **Emotive Responses**: Use facial expressions

### Milestone Demo

"Mae smiles when you give her flowers, and you can see it"

---

## Phase 4: Community Projects Enhanced (Weeks 8-9)

**Goal: Collaborative building at 25cm scale**

### Week 8: Precision Construction

- [ ] **Detailed Bridge Project**: Visible planks and nails
- [ ] **NPC Contributions**: Watch them place individual voxels
- [ ] **Progress Visualization**: See every small addition
- [ ] **Quality Standards**: NPCs fix misaligned voxels
- [ ] **Celebration Details**: Decorations at voxel scale

### Week 9: Advanced Projects

- [ ] **Trading Post Details**: Shelving, counters, signs
- [ ] **Infrastructure**: Lamp posts, benches, planters
- [ ] **Seasonal Decorations**: Individual ornaments
- [ ] **Community Gardens**: Individual plants visible
- [ ] **Player Customization**: Add personal touches

### Milestone Demo

"The bridge has visible wood grain, and each plank was placed with purpose"

---

## Phase 5: Demo Polish for Refined Voxels (Weeks 10-12)

**Goal: Showcase the beauty of 25cm voxels**

### Week 10: Visual Excellence

- [ ] **Material Polish**: Subtle variations per voxel
- [ ] **Lighting Refinement**: Shadows between voxels
- [ ] **Ambient Occlusion**: Depth in every crevice
- [ ] **Weather Effects**: Rain on individual voxels
- [ ] **Time of Day**: Light catching voxel edges

### Week 11: Performance Optimization

- [ ] **Blueprint Profiling**: Under 8ms execution
- [ ] **LOD Refinement**: Smooth transitions
- [ ] **Mobile Testing**: 30+ FPS on target devices
- [ ] **Memory Optimization**: Efficient voxel storage
- [ ] **Chunk Streaming**: No hitches

### Week 12: Demo Variants

- [ ] **PC Demo**: Full 25cm detail showcase
- [ ] **Mobile Demo**: Optimized but beautiful
- [ ] **Builder's Demo**: Creative mode highlights
- [ ] **Story Demo**: NPC interactions focus
- [ ] **Tech Demo**: Behind-the-scenes option

### Milestone Demo

"Every screenshot looks like concept art, but it's all real voxels"

---

## Blueprint Development Strategy

### Performance Optimization Plan

**Week-by-Week Focus:**

1. **Greedy Meshing**: Reduce polygons by 90%
2. **Chunk Pooling**: Reuse components
3. **LOD System**: Distance-based detail
4. **Update Distribution**: Spread across frames
5. **Mobile Optimization**: Platform-specific tweaks

### Pure Blueprint Advantages

- **Rapid Iteration**: Test new features instantly
- **Visual Debugging**: See data flow clearly
- **Team Accessible**: Artists can contribute
- **Moddable**: Community can enhance

### Performance Targets

|Milestone|PC Target|Mobile Target|
|---|---|---|
|Single Chunk|144 FPS|60 FPS|
|5x5 Chunks|120 FPS|45 FPS|
|Full World|90 FPS|30 FPS|
|With NPCs|75 FPS|30 FPS|
|Final Demo|60 FPS|30 FPS|

---

## Risk Mitigation

### Technical Risks (Updated)

**Risk**: 25cm voxels overwhelming mobile GPU

- **Mitigation**: Aggressive LOD, smaller chunks
- **Fallback**: Dynamic voxel size based on platform

**Risk**: Blueprint performance with detailed voxels

- **Mitigation**: Greedy meshing, update distribution
- **Fallback**: Blueprint nativization for shipping

**Risk**: Memory usage with smaller voxels

- **Mitigation**: Streaming, compression, pooling
- **Fallback**: Reduce active chunk count

---

## Success Indicators

### Technical Milestones

- ✅ **Week 1**: Voxel pipeline proven
- 🎯 **Week 2**: 25cm voxels running smoothly
- ⏳ **Week 4**: Building system with detail
- ⏳ **Week 6**: Expressive characters
- ⏳ **Week 8**: Community building
- ⏳ **Week 10**: Visual polish
- ⏳ **Week 12**: Demo ready

### Community Response Goals

- "I've never seen voxels look this good"
- "It doesn't even look like voxels from a distance"
- "I can't believe this is all Blueprints"
- "The detail is incredible but it still runs on my phone"
- "This is what voxel games should have been all along"

---

## The Refined Hearthshire Promise

By Month 3, every demo player experiences:

**Opening (10 min)**: "These aren't the voxels I expected"

**Midpoint (45 min)**: "I'm building with actual architectural detail"

**Climax (75 min)**: "The whole world feels handcrafted and alive"

**Ending (90 min)**: "I need to own this and build my dream home"

---

_"25cm voxels, pure Blueprints, infinite beauty, magical worlds."_

**🎯 CURRENT FOCUS: Implementing Blueprint greedy meshing for 25cm voxels**