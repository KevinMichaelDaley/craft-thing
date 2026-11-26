# Play clearing tree

The tree uses the 93 by 135 cell crop at `[166, 0, 259, 135]` in the level piece 03 prop package. The export keeps the original wood and leaf pixels and their aligned colors. Transparent cells preserve the destination terrain. The prop manifest records the bounds and hashes of both source channels.

Rebuild and validate the package from the repository root.

```sh
python3 scripts/props/spawn_tree_export.py
python3 scripts/props/pack_prop.py --validate assets/props/spawn_tree.prop
```

A fresh `--play` world places this prop at `(-56, 14)` and `(104, 14)`. Both root systems cross the ground line at `y=96`. Three separately authored stone bodies begin beside the trees. The two outside stones span chunk seams.

Pass the same `--world-dir` when reopening a world to retain its edits and surviving bodies. The clearing is authored once when a play world is created.
