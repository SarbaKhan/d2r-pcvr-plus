# Act III renderer allocation failure

Two reported Spider Forest crashes reach the same null renderer-allocation
path. One logs `Failed to allocate from Renderer pool!` immediately before the
fault. This establishes an allocation failure, not a room-unloading defect,
physical RAM/VRAM exhaustion, or a headset-specific problem.

This change ports Sergious / BodyWalkVR's native pool growth from
[upstream 47a258c](https://github.com/natpoh/diablo2-vr-port/tree/47a258c971c6a3be030769acba39764fcc1f6067),
retaining its MIT attribution. There is no separate upstream room-unloading fix.

At startup the mod increases the renderer TLSF heap from 12 to 64 MiB and the
entity heap from 70 to 280 MiB. It adds a separate buffer using the game's
`tlsf_free` under the allocator's SRW lock. The native object's original buffer
and size remain intact. Added memory remains available for the process lifetime.
The change adds 262 MiB of capacity at the defaults; it is not an FPS improvement.

Before handing memory to the game, the code resolves the expected signatures,
checks the allocator vtable and Free method/call, validates the heap fields,
then walks the original pool through its end marker while holding its lock.
Unknown layouts are left alone and reported. Uninitialized/encrypted state is
retried by the existing startup worker. A process marker prevents repeated growth
if the plugin is loaded again.

The existing `[render] render_pool_mb=64` and `entity_pool_mb=280` settings are
now consumed by PCVR. Zero disables growth of that pool, smaller values do not
shrink it, and changes require a full game restart. No calibration, save,
draw-distance or other graphics setting is changed.

## Validation

`steamvr/build_windows.ps1 -Test` includes synthetic allocator fixtures for
growth under the native lock, waiting/missing signatures, disabled/already-large
pools, wrong vtable/Free method/call, inconsistent layout, malformed blocks and
end-marker flags. Repeated startup must not grow a pool twice. These fixtures
test the preflight and handoff boundary, not the game's TLSF implementation.

The upstream `cleanroom/tests/pools_test.cpp` is compiled as
`pools-snapshot-test`. To exercise actual TLSF with an owner's local D2R
3.3.93787 memory image, configure `D2R_POOL_TEST_IMAGE` to its absolute path;
CTest then enables seven additional native allocator cases. The image is not
distributed or attached to the PR. Without it these cases are not claimed run.

On 2026-10-16, the owner confirmed that Act III loads and works in the combined
developer preview on Steam D2R 3.3.93787 through SteamVR/OpenXR. The log confirms
`memory pools: renderer 12 -> 64 MB, entity 70 -> 280 MB`; the recorded travel
command selected Kurast Bazaar (level 80). The test configuration used terrain
2 and objects 510. The headset model and character class were not separately
reported. The owner accepted the fix provisionally and declined a long session.

This verifies pool activation and basic Act III gameplay, not extended crash
stability. If the allocation crash recurs, capture the new session logs. A future
longer test should revisit Spider Forest and new jungle rooms at the previously
failing distances for longer than the reported 30-minute sessions.
