# [[Hearthshire]] - Game Design Document (Refined Voxel Edition)

## Version 3.0 - 25cm Voxels Transform Everything

---

## Core Innovation: Refined Voxels + Template Worlds

### The New Magic Formula

**Hand-Crafted Templates + Player Seeds + 25cm Voxel Detail = Unprecedented Beauty**

Every player gets a **completely unique world** with **architectural-level detail** while experiencing the **same magical story**. Mae's shop now has window boxes with individual flowers, shutters that open, and a door you could actually walk through.

---

## How 25cm Voxels Change Everything

### Building Revolution

**Before (1m voxels):**

- Basic box houses
- No real windows or doors
- Blocky, Minecraft-like construction

**Now (25cm voxels):**

- **Actual Architecture**: Window frames, door trim, roof shingles
- **Furniture Detail**: Chairs you can see, tables with legs
- **Decorative Elements**: Flower boxes, shutters, chimneys
- **Precision Placement**: Build exactly what you envision

### Farming Transformation

**Enhanced Agriculture:**

- **Individual Plants**: Each carrot is 2-3 voxels
- **Growth Stages**: Visible progression sprout by sprout
- **Detailed Tools**: Watering can with actual spout
- **Precision Farming**: Plant in exact patterns

**New Mechanics Possible:**

- Companion planting (flowers next to vegetables)
- Detailed irrigation channels
- Crop quality based on individual plant care
- Harvesting specific parts of plants

### Character Interaction Depth

**Mae's Shop Enhancement:**

- **Visible Inventory**: See items on actual shelves
- **Detailed Counter**: Cash register, scale, display items
- **Personal Touches**: Photos, decorations, lived-in feel
- **Interaction Points**: Specific spots for different activities

**NPC Improvements:**

- **Facial Expressions**: 32-voxel heads show emotion
- **Body Language**: Shrugs, waves, detailed gestures
- **Clothing Details**: Aprons with pockets, work gloves
- **Personal Items**: Oliver's toolbelt, Hazel's goggles

---

## World Generation 2.0

### Template Refinements for 25cm Scale

**Meadow Valley Template:**

```
Consistent Elements (Hand-Placed):
- Mae's shop (20x15x10 meter footprint)
  - Front door exactly 2m x 2.5m
  - Windows with 4-pane divisions
  - Flower boxes under each window
- Stone paths (1m wide, made of 4x4 voxel tiles)
- Community center foundation with detailed stonework

Variable Elements (Seed-Generated):
- Rolling hills with 1:4 max slope
- Wildflower patches (individual flowers)
- Tree placement with root systems
- Rock formations with geological detail
```

### Biome-Specific Details

**Whispering Woods:**

- Tree bark has actual texture through voxel variation
- Mushrooms with visible gills and stems
- Fallen logs with decay patterns
- Forest paths with scattered leaves

**Coastal Cliffs:**

- Erosion patterns voxel by voxel
- Tidepools with creatures
- Barnacle clusters
- Driftwood with weathering

---

## Gameplay Systems Enhanced

### Precision Building System

**Construction Tools:**

1. **Single Voxel**: Place/remove individual 25cm blocks
2. **Shape Tools**:
    - Line tool (fences, walls)
    - Rectangle tool (floors, walls)
    - Circle tool (towers, wells)
3. **Smart Tools**:
    - Window wizard (creates proper openings)
    - Door placer (ensures proper dimensions)
    - Roof builder (angled placement)

**Material System:**

- 256 materials in palette
- Each with subtle variations
- Aging and weathering options
- Paint system for customization

### Advanced Farming

**Crop Detail Levels:**

- **Seeds**: Single voxel
- **Sprouts**: 2-3 voxels
- **Young Plants**: 5-10 voxels
- **Mature Plants**: 15-30 voxels
- **Harvestable**: Visible fruit/vegetables

**New Farming Features:**

- Individual plant watering
- Pruning for better yield
- Support structures for climbing plants
- Greenhouse construction with glass voxels

### Social System Improvements

**Friendship Visualization:**

- Gift reactions show in facial expressions
- NPCs examine gifts with detailed animations
- Personal spaces reflect relationship level
- Collaborative building shows personality

**Romance Details:**

- Subtle blush on voxel cheeks
- Hand-holding animations possible
- Personal gift preferences visible
- Wedding decorations at voxel scale

---

## Technical Design Adaptations

### Chunk System (Updated)

```
Previous: 32³ voxels per chunk (1m voxels)
Now: 16³ voxels per chunk (25cm voxels)

Benefits:
- Same memory footprint
- More detailed world
- Better LOD transitions
- Smoother terrain
```

### LOD System Design

**LOD 0 - Full Detail (0-50m):**

- Every 25cm voxel rendered
- All decorative elements
- Full material variations
- Individual plant leaves

**LOD 1 - Simplified (50-100m):**

- Merge 2x2x2 voxels
- Maintain architectural shapes
- Preserve color average
- Structure silhouettes clear

**LOD 2 - Distant (100-150m):**

- 4x4x4 voxel groups
- Building shapes only
- Biome color themes
- Terrain contours

**LOD 3 - Far (150m+):**

- Billboard imposters
- Atmospheric integration
- Impressionistic rendering

---

## Progression Design

### Early Game (Detailed Start)

**Tutorial Enhancements:**

- Build a birdhouse (teaches precision)
- Plant a flower box (small-scale farming)
- Fix Mae's fence (exact placement)
- Create stepping stones (pattern making)

**First Day Goals:**

- Detailed shelter with actual door
- Small garden with individual plants
- Basic furniture (bed, chair, table)
- Decorative touches showing personality

### Mid Game (Architectural Freedom)

**Building Projects:**

- Multi-room house with details
- Greenhouse with glass panels
- Workshop with tool displays
- Barn with animal pens

**Community Integration:**

- Help design shop displays
- Build detailed market stalls
- Create park with benches
- Design festival decorations

### Late Game (Master Builder)

**Grand Projects:**

- Castle with detailed stonework
- Lighthouse with spiral staircase
- Cathedral with stained glass
- Underground city with infrastructure

---

## Platform Adaptations

### PC Experience

**Full 25cm Detail:**

- Every voxel individually lit
- Real-time shadows
- Maximum view distance
- All particle effects

**Advanced Controls:**

- Precise mouse placement
- Keyboard shortcuts for tools
- Multi-select capabilities
- Advanced building modes

### Mobile Experience

**Optimized Beauty:**

- Smart LOD for performance
- Assisted placement tools
- Gesture-based building
- Auto-detail features

**Mobile-Specific Features:**

- Tap to place voxel groups
- Pinch to zoom for detail work
- Building templates for speed
- Simplified material palette

---

## Unique Selling Points (Enhanced)

### 1. **Unprecedented Voxel Detail**

"The first voxel game where you can build a believable house"

- Windows that look like windows
- Doors with handles and hinges
- Roofs with individual shingles
- Gardens with individual flowers

### 2. **Architectural Creativity**

"Build dreams, not just boxes"

- Victorian houses possible
- Japanese temples achievable
- Modern architecture viable
- Fantasy castles with detail

### 3. **Living World Detail**

"NPCs that feel real in spaces that feel lived-in"

- Shops with visible inventory
- Homes with personal touches
- Workshops with hanging tools
- Gardens with growth stages

### 4. **Pure Blueprint Magic**

"Built entirely with visual scripting"

- Proves AAA possible without C++
- Modding-friendly architecture
- Accessible development
- Community can contribute

---

## Emotional Journey (Enhanced)

### First 10 Minutes

"This isn't the voxel game I expected"

- Smooth hills catch the eye
- Mae's shop has real windows
- Flowers wave individually
- Door actually opens

### First Hour

"I can build anything I imagine"

- Window placement just works
- Roof angles look natural
- Furniture has real detail
- NPCs notice improvements

### First Day

"This is my home"

- Personal touches everywhere
- NPCs comment on details
- Seasons change beauty
- Screenshots look painted

### First Week

"I need to show everyone"

- Share architectural marvels
- Compare building techniques
- Trade design blueprints
- Visit friends' detailed worlds

---

## Success Metrics

### Technical Excellence

- 30+ FPS on mobile with full detail
- 60+ FPS on mid-range PC
- Instant building response
- Smooth world streaming

### Artistic Achievement

- "Doesn't look like voxels" comments
- Screenshot-worthy every angle
- Concept art quality in-game
- Magazine coverage for beauty

### Player Satisfaction

- 4+ hour first sessions
- 90% return rate day 2
- Social sharing spike
- "Best building game" reviews

---

_"25cm voxels transform Hearthshire from a voxel game into a building dream."_