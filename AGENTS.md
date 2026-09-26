# Agent Instructions

This project uses **tk** (ticket) for issue tracking. Run `tk help` to see available commands.

## Quick Reference

```bash
tk ready              # Find available work (deps resolved)
tk show <id>          # View issue details
tk status <id> in_progress  # Claim work
tk close <id>         # Complete work
tk dep tree <id>      # View dependency tree
tk blocked            # List tickets with unresolved deps
```

## Ticket Commands

```bash
# Listing
tk ls                              # All tickets
tk ls --status=open                # Open tickets only
tk ready -T procgen                # Ready procgen tickets
tk blocked                         # Tickets waiting on deps

# Viewing / editing
tk show <id>                       # Full ticket view
tk edit <id>                       # Open in $EDITOR
tk add-note <id> "note text"       # Append timestamped note

# Dependencies
tk dep <id> <dep-id>               # Add dependency (id depends on dep-id)
tk dep tree <id>                   # Show dependency tree
tk dep cycle                       # Find dependency cycles

# Status transitions
tk status <id> open                # Reopen a closed ticket
tk status <id> in_progress         # Start work
tk close <id>                      # Mark complete
```

## Test-Driven Development (TDD)

**ALL work MUST follow RED-GREEN-REFACTOR:**

### Phase 1: RED (Write failing tests first)

1. Read the ticket's acceptance criteria and design notes
2. Create the test file in `tests/<subsystem>/` following existing patterns:
   - Include the test macros: `RUN()`, `ASSERT_TRUE()`, `ASSERT_EQ()`, `ASSERT_INT_EQ()`
   - Use `g_pass`/`g_fail` counters and `PASS()` macro
   - Each `test_*()` function tests one behavior
3. Write tests that validate the ticket's acceptance criteria
4. Verify tests **compile** (`make <test_target>`) and **fail** (RED)
5. Commit RED tests: `git add tests/... && git commit -m "[RED] <ticket-id>: failing tests for <description>"`

### Phase 2: GREEN (Minimal implementation)

1. Create/modify the implementation files in `src/<subsystem>/` and `include/ferrum/<subsystem>/`
2. Follow existing code conventions (see below)
3. Write the **minimal** code needed to make tests pass
4. Run tests: `make <test_target> && ./build/<test_binary>`
5. All tests must pass with zero failures
6. Commit: `git commit -m "[GREEN] <ticket-id>: implementation passing tests"`

### Phase 3: REFACTOR (Clean up)

1. Extract duplicated code into helper functions
2. Remove magic numbers, add proper constants
3. Ensure no warnings (`-Wall -Wextra`)
4. Run ALL tests (not just the new ones): `make test`
5. Commit: `git commit -m "[REFACTOR] <ticket-id>: cleanup after tests"`

### Test File Template

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ferrum/<subsystem>/<header>.h"

static int g_pass = 0;
static int g_fail = 0;

#define RUN(fn) do { \
    printf("RUN  %s\n", #fn); \
    fn(); \
    printf("OK   %s\n", #fn); \
} while (0)

#define ASSERT_TRUE(expr) do { \
    if (!(expr)) { \
        printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
        g_fail++; \
        return; \
    } \
} while (0)

#define ASSERT_EQ(a, b)   ASSERT_TRUE((a) == (b))
#define ASSERT_INT_EQ(a, b) ASSERT_TRUE((int)(a) == (int)(b))
#define PASS() g_pass++

static void test_description_of_behavior(void) {
    // Arrange
    // Act
    // Assert
    ASSERT_EQ(result, expected);
    PASS();
}

int main(void) {
    printf("=== <Subsystem> Tests ===\n\n");
    RUN(test_description_of_behavior);
    /* ... more RUN() calls ... */
    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
```

## Code Conventions

- **Language**: C11 (`-std=c11`), no compiler extensions unless gated behind `#ifdef`
- **Naming**: `snake_case` for functions, `snake_case` for types with `_t` suffix, `UPPER_SNAKE` for macros/enums
- **Prefixes**: Public API uses `fr_` prefix (e.g., `fr_vec3_add`). Subsystem-internal functions use subsystem prefix (e.g., `procgen_tokenize_*`)
- **Headers**: `#pragma once` alternative: `#ifndef FERRUM_SUBSYSTEM_HEADER_H` / `#define` / `#endif`
- **Includes**: System headers first, then project headers in alphabetical order
- **Error handling**: Return `bool` or `int` (0=success, -1=error). Use `char *err_buf, uint32_t err_cap` pattern for descriptive error messages
- **Memory**: Arena/bump allocators for frame lifetime, pool for fixed-size objects. No `malloc` in hot paths
- **Comments**: Doxygen `/** ... */` on all public functions. No inline comments unless code is non-obvious
- **File length**: Target <500 lines per .c file. Split when it grows beyond that
- **No dead code**: Remove `#if 0` blocks before committing
- **Zero warnings**: `-Wall -Wextra` must be clean

## Build Commands

```bash
make all                 # Full build (libheadless.a + all binaries)
make test                # Build and run all headless tests
make test_timeout        # Tests with per-test timeout (20s default)
make test_renderer       # Renderer tests (requires SDL2/OpenGL)

# Procgen-specific
make procgen             # Build only procgen objects
make procgen-test        # Build and run all procgen tests
make procgen-bench       # Build and run procgen benchmarks

# Individual test targets
make pNNN_*_tests        # Build specific test binary
./build/pNNN_*_tests     # Run specific test binary
```

## Model Versions, Materials, UVs, and Rigging

Collider detector parts use the Blender authoring, audit, and visual-review
pipeline. Do not carry vehicle-specific Isaac/Omniverse export or rendering
instructions over to detector assets. Export a detector asset to a simulator
only when the user explicitly requests that for the detector task.

Every model MUST be authored and delivered in two coordinated versions:

1. **Very high-poly visual source**: actual geometric detail for close inspection
   and rendering, including bolts, ridges, seams, and surface features.
2. **Part-decomposed collision LOD**: simple closed collision shapes used purely
   for physics. Use separate convex pieces where required; preserve functional
   articulation and do not substitute the visual mesh for collision geometry.

Create UVs and named material assignments as each mesh is built, including the
collision meshes. Keep UV islands usable for texturing: document texture
sets, avoid unintended overlap, and reserve padding. Maintain meaningful material
decompositions (painted metal, bare metal, rubber, glass, fabric, lights, etc.)
across versions. Record correspondence, units, axes, and names in the asset's
manifest. Run the existing Blender topology audit on authored output and verify
UV validity and material coverage before delivery.

All vehicles MUST remain assemblies of functional parts so they can be rigged
and imported into Omniverse or this engine. Do not flatten a vehicle into one
inseparable mesh. Use a stable named hierarchy with separate chassis/body,
wheels, steering carriers, suspension groups, doors, hoods, tailgates, wipers, rotors,
and other applicable moving parts. Put origins at actual hinge, axle, or sliding
interfaces; record joint axes, limits, rest transforms, and parent relationships.
Keep render and collision counterparts aligned to the same articulated parts.
Fixed detail parts may share their parent part's motion. Preserve material slots,
UVs, part names, and transforms in native files and exports; distinguish a
riggable asset from a fully configured runtime vehicle controller.

**Fuse connected stationary geometry.** Objects that do not need to move
separately and that overlap, touch, or need to form a connected structure MUST
be topologically fused into one connected mesh for that functional part. Merely
joining object containers or parenting intersecting primitives is insufficient:
create shared, welded topology; remove buried/internal faces, duplicate surfaces,
and unintended gaps; and audit the resulting mesh. Preserve UVs and material
decompositions within the fused mesh. Keep separate geometry where independent
motion requires it, such as doors, wheels, suspension, hood panels, and rotors.
Collision may remain a set of convex hulls for the same rigid part when required
by physics, but those hulls must share the correct articulated parent and must
not be used to excuse unfused render/source geometry.

**Author proper edge flow; Boolean fusion is forbidden.** Build connected
surfaces with shared vertices, deliberate edge loops, bridged boundaries, and
support loops that follow the reference forms. Boolean union, difference, or
intersection operations are forbidden for authoring render/source geometry,
including applied modifiers and external Boolean kernels. Joined intersecting
shells, box soup, Boolean soup, and remeshing do not satisfy topological fusion.
Create openings and attached details through authored topology. Passing a
manifold check alone does not establish proper edge flow.

**It is forbidden to simplify complex geometry.** Do not use decimation,
automatic simplification, dissolving detail, voxel remeshing, or replacement
with simpler primitives to reduce authored complexity, hide topology defects,
or make audits pass. Preserve the high-poly geometric details and reference
forms. Author the required collision version separately; its existence does
not authorize simplifying or degrading the source geometry.

Every mesh audit MUST warn whenever an object has more than one connected
component. Include the exact object name and component count in each warning,
including objects sharing a mesh datablock. Report all such objects. Warnings
must not replace the existing failure gates for unintended disconnected parts.
Regression tests must cover these warnings and forbidden authoring operations;
checking only unapplied modifiers is insufficient.

For complex mechanical assemblies, inventory the real parts before modeling.
Model them as separate, assemblable pieces, each consuming the same immutable
global procedural parameter set and derived dimensions. Preserve mating
interfaces, assembly transforms, part identifiers, and fixed or moving joints.
Do not fuse distinct manufactured components into a single vehicle mesh. The
edge-flow and fusion rules apply within each authored part, not across actual
component boundaries such as screws, clamps, circuit boards, connectors, and
wires. Do not scatter unrelated per-part scale or mass constants through scripts.

## Generated Textures and PBR Maps

Use the existing fal.ai and Patina script pipeline for authored model textures.
The fal.ai credential is stored at `~/.ssh/FAL_AI_API_KEY` (the script itself is
`scripts/gen_patina_materials.py`, not a file in `~/.ssh`). Never print or commit
the credential. The script generates flat photographic texture sources through
fal.ai, then uses `fal-ai/patina` to turn each source into albedo, normal,
roughness, metallic, and height maps.

First author the high-poly details and UV-unwrap the visual source. Then generate
the surface textures and derive their PBR maps with this pipeline. Preserve any
authored geometric normal/AO maps when combining surface maps; Patina microdetail
must not replace geometric bolts, seams, ridges, and panel forms.
Bolts, ribs, and other manufactured features must be actual geometry in the
high-poly source. Render that source for visual reviews and presentation images.
Keep prompts, seeds, source images, map provenance, texture-set correspondence,
and any bake settings used with the asset. Use physically plausible material values and
restrained, reference-driven wear, dust, and roughness variation for a
photographic finish. Audit UVs and material coverage on the final textured mesh.

## Concept Art and Gemini Model Review Loop

Before modeling, generate detailed concept art grounded in reference photographs
of real objects, not diagrams. Match the mechanical detail level of the detector
concepts. For a quadrotor, this includes recognizable electronics, populated
boards, connectors, jumper wires, routed power cables, motor hardware, and
modeled screw threads. The high-poly source must contain the physical details;
textureless presentation is not permission to omit them.

Use **Gemini through OpenRouter** for iterative visual review. The OpenRouter
API key is stored in `~/.ssh/OPENROUTER_API_KEY`; load it locally without printing or committing
the credential. Each review is a fresh request with no conversation history.
Supply all three of these inputs:

- A brief, fixed description of the object and its intended function.
- The concept art as actual image input.
- The actual contact sheet rendered from the current high-poly model, as image
  input. Include useful overall views and detail close-ups. Never substitute
  concept art, a generated mock-up, file paths, or a verbal account for renders.

Tell Gemini to critique the model against the concept art and identify concrete
visual omissions or defects. Ask whether it is satisfied with the model as a
finished asset. **Use exactly the same prompt and concept art every time**;
only the current model contact sheet changes. Do not include prior critiques,
revision history, claimed fixes, audit results, or leading quality assessments.

Implement the critique, render a new contact sheet, and submit another fresh
Gemini review. **Iterate until Gemini is satisfied.** Do not stop with known
visual blockers merely filed as follow-up work. Archive the exact prompt,
concept art, each actual contact sheet, model identifier, and full response for
every iteration. This visual loop complements the topology/physics tests; it
does not replace them or authorize merging without requested approval.

## Subsystem: Procedural Dungeon Grammar (`src/procgen/`)

Key concepts:
- **Grammar**: A compiled C module implementing `tokenize()` + `rasterize()`. Registered in grammar registry.
- **Token string**: The "DNA" of a dungeon level — a linear sequence of typed tokens (ROOM_QUAD, CORRIDOR_H, SPAWN, MARKER, etc.)
- **Layout** (`fr_dungeon_layout_t`): Intermediate representation with rooms, corridors, ramps, markers, nav graph
- **Architect VLM**: Uses LLM infrastructure (engine_settings) to generate token strings from natural language
- **Critic**: Automated playtester using NitroGen vision-action model, with engine hooks for death/marker detection
- **TDD**: Every procgen ticket follows RED-GREEN-REFACTOR. Tests go in `tests/procgen/`.

See `design/procgen_dungeon_grammar.md` for the full architecture document.

**When ending a work session**, you MUST complete ALL steps below. Work is NOT complete until `git push` succeeds.

**MANDATORY WORKFLOW:**

1. **File issues for remaining work** - Create issues for anything that needs follow-up
2. **Run all tests** To be sure what we did worked, and nothing broke.
3. **Update issue status** - Close finished work, update in-progress items
4. **PUSH TO REMOTE** - This is MANDATORY:
   ```bash
   git pull --rebase
   git push
   git status  # MUST show "up to date with origin"
   ```
5. **Clean up** - Clear stashes, prune remote branches
6. **Verify** - All changes committed AND pushed
7. **Hand off** - Provide context for next session

**CRITICAL RULES:**
- Work is NOT complete until `git push` succeeds
- NEVER stop before pushing - that leaves work stranded locally
- NEVER say "ready to push when you are" - YOU must push
- NEVER use `git stash` — it disrupts the working tree and can hide changes
- NEVER use `sed` to edit files — always use the Edit tool
- If push fails, resolve and retry until it succeeds
