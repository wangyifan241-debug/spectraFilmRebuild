# Phoenix reference dataset v0.1
`dataset_manifest.jsonl` has one real image candidate per line. Unknown metadata is explicit; quality_score is nullable until local decode. `tools/phoenix-dataset/acquire_seed.py` reuses and rechecks existing Round 5 leads, writes local originals only under ignored build/. `screen.py` records exact SHA256, dHash flags and resolution score; it never auto-accepts a photograph based on these proxies.

Run: python tools/phoenix-dataset/acquire_seed.py; python tools/phoenix-dataset/screen.py.

Selection: exclude wrong/ambiguous stock, scans of prints without provenance, packaging/screenshots/simulations, severe compression, obvious watermarks/large borders and broken ICC. Unknown scanner/post-processing remain stratification/confidence limits, not invented values. Scene labels require visual review; balance portrait/skin, sky, vegetation, urban, indoor, night, neutral objects with lighting/format/exposure covariates. Record exclusion reasons; do not select for a preferred look.

Split policy: connect samples sharing photographer OR source/roll/duplicate cluster; assign whole connected components to train/validation with a fixed versioned seed. Two seed records from one Flickr platform are quarantined rather than pretend to have independent validation. Require multiple independent components before freezing a split; identical/near-identical cross-platform reposts stay connected. Never infer film accuracy from nonpaired image DeltaE.

Phase 2 can start exploratory scene/scanner-stratified statistics once credible images are actually acquired and reviewed. Until then no fit or grain recovery claim. 200 per film is a goal, not an acceptance count.
