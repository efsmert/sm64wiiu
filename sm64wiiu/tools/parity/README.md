# Parity Tooling

Use this script to regenerate the current Phase 0/1 parity artifacts against the local donor tree.

```bash
sm64wiiu/tools/parity/generate_phase0_phase1_matrix.py
```

Optional:

```bash
sm64wiiu/tools/parity/generate_phase0_phase1_matrix.py --workspace-root /path/to/workspace
```

Expected workspace layout:
- `<workspace>/sm64coopdx`
- `<workspace>/sm64wiiu`

Outputs:
- `sm64wiiu/parity/phase0_matrix.json`
- `sm64wiiu/parity/phase0_matrix.md`
- `sm64wiiu/parity/phase1_lua_port_queue.md`

Current intent:
- Phase 0: reproducible parity baseline (Lua symbols, hook surface, module tree, hook callsite counts)
- Phase 1: donor-file-oriented Lua symbol port queue with built-in-mod usage prioritization
