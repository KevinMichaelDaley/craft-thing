# Streamed GPU granular-particle storage

The active chunk atlas owns 8192 fixed particle records per chunk on the GPU.
The first 4096 records are indexed by local cell, so painting a granular pixel
activates its primary record in one compute dispatch. The second 4096 records
hold a second particle per cell when the MPM gather accepts a collision.
Painting an occupied granular cell changes its material without adding mass.
Painting a non-granular material removes both particles. The single-command
diagnostic transfer rejects an occupied destination; the MPM stage ranks
competing arrivals by stable ID. An upload rejects inconsistent counts or
exhaustion. No particle state is copied to the CPU during a physics frame.

Each record contains fixed-point local position and velocity, deformation,
stable 64-bit ID, mass, grain size, material and flags. Chunk coordinates plus
local position give world coordinates without reducing position precision on
the infinite canvas. IDs are derived from chunk coordinates and the original
cell, and do not change on seam transfer. The GPU gather deterministically
compacts each cell's accepted particles into primary and secondary slots after
each substep.

The stream worker writes sparse active records into version 4 chunk files.
Older files seed records for their granular material cells on load. GPU upload
does the same for legacy in-memory chunks. Eviction downloads state once, then
the worker saves it; resident frames remain GPU-only. The particle test reports
allocated capacity and synchronous wall time for upload, paint plus diagnostic
readback, and seam transfer. The MPM GPU timestamp and solver policy are in
[mpm_solver.md](mpm_solver.md).
