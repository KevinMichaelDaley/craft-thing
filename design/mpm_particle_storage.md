# Streamed GPU granular-particle storage

The active chunk atlas owns 8192 fixed particle records per chunk on the GPU.
The first 4096 records are indexed by local cell, so painting a granular pixel
activates its primary record in one compute dispatch. The second 4096 records
are reserved for the later MPM solver's multi-particle cells. Current activation
is limited to one primary particle per cell; painting an occupied granular cell
changes its material without adding mass. Painting a non-granular material
removes that particle. A cross-chunk transfer to an occupied destination is
rejected, leaving the source unchanged. An upload rejects inconsistent counts
or exhaustion. No particle state is copied to the CPU during a physics frame.

Each record contains fixed-point local position and velocity, deformation,
stable 64-bit ID, mass, grain size, material and flags. Chunk coordinates plus
local position give world coordinates without reducing position precision on
the infinite canvas. IDs are derived from chunk coordinates and the original
cell, and do not change on seam transfer. The boundary transfer compute pass
changes ownership and local position as one operation and updates both GPU
counts. Pool order is fixed by cell slot, so no nondeterministic compaction is
needed for the current one-particle-per-cell path. The later MPM solver must
define deterministic allocation for the reserved slots before using them.

The stream worker writes sparse active records into version 4 chunk files.
Older files seed records for their granular material cells on load. GPU upload
does the same for legacy in-memory chunks. Eviction downloads state once, then
the worker saves it; resident frames remain GPU-only. The particle test reports
allocated capacity and synchronous wall time for upload, paint plus diagnostic
readback, and seam transfer. These are storage-path timings, not MPM solver
timings; the solver stages are implemented in a following ticket.
