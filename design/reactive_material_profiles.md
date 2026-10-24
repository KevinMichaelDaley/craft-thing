# Reactive material profile foundation

Ticket: `ctlo-ldb7`. The immutable reference data is generated from
`assets/fixtures/fire_smoke_v1.json`. Twenty material identities include all
existing IDs 0-10 unchanged, followed by coal, char, ash, oil, red-slime jelly,
Greek fire, corrosion residue, gold and a magical ingredient.

The foundation provides coefficients, validated physical predicates and CPU/GPU
ignition-rate references. It does not yet implement the burning state machine,
moving gas, oil transport, persistent reactive state or new-material gameplay
brushes. Those remain separate dependent tickets. In particular, adding a material
identity does not claim that all existing physics/render consumers already support
its later phase behavior.

## Representation and namespace

`fr_reactive_profile_t` is 128 bytes / 32 words with ABI version 1. It contains
material identity, legacy body mapping, phase/capabilities, thermal data, finite
fuel/oxidizer and char/ash/soot/exhaust yields, conductivity, water capacity,
oxidation/corrosion response and sludge/adhesion coefficients. Shared word offsets
are in `include/dungeoncraft/reactive_profile_layout.h`.

The body namespace is mapped explicitly: body ID 2 means wood (terrain ID 7),
and body ID 3 means mortar (terrain ID 6). Unknown body IDs return no profile.
Later geometry admission adds reactive body materials; it must not reinterpret
terrain IDs numerically. Existing stone/wood/leaves/etc. density defaults remain
unchanged. Ash is its own nonfuel granular profile, and sludge is continuous
liquid with positive yield stress and shear dependence, not loose granular cells.

The physical table has no pigment, emission color or texture fields. Rendering
keeps independent channels; neither CPU nor GPU ignition reference accepts a
color as heat. A real hardware probe changes visual input words and verifies
bit-identical physical coefficients/rates across the change.

## Reference modifiers

The CPU API returns separate substrate and coating hazards. Oil on inert stone
can ignite only the finite coating; it does not turn the stone into fuel. Oil
coating increases exposed wood's ignition hazard. Wet saturation reduces ordinary
hazard and increases the capacity used by a prospective shock heat input.
Oxidation uses per-material modifiers, not a universal rust bonus. Conductivity is
independent of fuel: water can conduct while remaining noncombustible.

Ordinary rates require adequate external oxygen. A bound-oxidizer profile uses
its finite local oxidizer inventory instead; Greek fire's water-dousing flag
does not cancel this reaction merely because saturation is high. This reference
computes propensity only: the subsequent reaction stage must reserve and debit
actual fuel/oxygen/heat and admit products before changing state.

All coefficients and exposure inputs are finite and nonnegative; compositions,
flags, legacy mappings and retained-char energy are validated. Invalid exposure
returns an error without modifying the caller's output. Shock energy here is a
prospective thermal input. Once the energy stage has deposited it, the modifier
receives zero additional shock energy so it is not counted twice.

## Explicit balance revision 2

The draft compound values violated the ingredient energy ledger. At a 50/50
oil/jelly mass mixture, input fuel energy is
`0.5*1*10000 + 0.5*0.4*16000 = 8200` per mixture-mass unit. The draft compound
claimed `0.7*16000 = 11200` without an input source. A committed failing test
(`967c7d9`) exposes that exact discrepancy.

Revision `fire-smoke-acceptance-2-energy-conservation` corrects Greek fire's
chemical/primary energy per fuel mass to `8200/0.7 = 11714.285714285714`. Its
fuel fraction remains 0.7, bound oxidizer 0.3, and all input quantities, maps,
burn-rate/depletion, wall-damage, delayed-launch, wetness and frame thresholds
remain unchanged. This is a conservation correction before runtime mixing, not
a relaxed gameplay/performance oracle. Python validation and generated C data
now enforce matching ingredient fuel/oxidizer/energy.

## Validation and regeneration

```sh
python3 tools/diagnostics/fire_fixture.py \
  --emit-material-c src/fire/material_profile_data.inc
python3 tools/diagnostics/fire_fixture.py \
  --check-material-c src/fire/material_profile_data.inc
make test_reactive_materials test_fire_fixtures
./tools/with-vulkan make build/reactive_profile_gpu_tests
env DC_VK_VALIDATE=1 \
  VK_LAYER_ENABLES=VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT \
  ./tools/with-vulkan ./build/reactive_profile_gpu_tests
```

Nine C reference cases cover identity, namespace, phases, modifiers, invalid
input and mixed fuel energy. Six Python fixture/data checks also pass. The actual
Intel Iris Xe probe dispatches 256 independent material/exposure cases and compares
128-byte profile layout and GPU ignition rates against the CPU reference. It
checks wet/dry, oxidation, prospective shock, external/bound oxidizer and ordinary
oil/magical coatings, including pigment independence. This is unit-level hardware
evidence; the final displayed gameplay/performance gates remain required.
