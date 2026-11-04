# Supplied-source batch — 2026-11-04

This batch continues the existing dataset and PR #15. It does not replace Phase 0 or modify Round 5.

## Actual files and use
- 22 user-supplied files copied into the existing build cache after full decode and SHA256 verification. Their original files were not changed or redownloaded.
- 12 new network downloads: five Lomography single-photo images and seven individually captioned Taiyou images. All used public URLs and per-host robots checks; no authentication or restriction bypass. These are published image variants, not guaranteed original scan files.
- Local total: 38 decodable files. Phoenix 200: 18; Phoenix II: 19; generation unconfirmed: one forest image.
- Two confirmed resized/cropped mirrors are retained as originals but excluded from statistics. Three small web presentations are excluded by the declared short-edge 700 / long-edge 1000 threshold. The generation-unconfirmed forest remains quarantined.
- Private exploratory color/tone set: Phoenix 200 **16**, Phoenix II **16**; remaining to the requested 15-per-stock target: **0 / 0**. Formal-fitting accepted: **0**. No licensed public photo pack or validated film model is claimed.
- Exploratory photographers: five for Phoenix 200, nine for Phoenix II. ArtByPino supplies 12/16 red-stock images and Taiyou supplies 8/16 blue-stock images: sample and scanning imbalance remains substantial. No train/validation split is frozen.

## Failure and safety evidence
Four unique Lomography leads failed: four HTTP 404 attempts on scoped photo URLs, then one verified canonical source returned HTTP 403. Failures were retained; no bypass or repeated endless retry. Album/group claimed counts were never treated as individual downloaded files. The gallery expansion was limited to individually captioned Phoenix II images; ambiguous unlabeled images were not included.

Original download_phoenix.ps1 and fetch_single_previews.py were reviewed, not run unchanged. Deletion/overwrite behavior and social-preview limitations are documented in SOURCE_KIT_REVIEW.md. Owned copies use exclusive creation and retain originals. Published high-resolution article URLs were preferred where explicitly present; no original-size endpoint guessing. The five Lomography images are around 1200 pixels and marked as web variants.

## Actual analysis and tests
exploratory_statistics.json contains 32 per-image Lab D65, OKLab and linear-Y distributions. Embedded ICC is converted through LittleCMS; missing ICC uses an explicit provisional sRGB assumption. Presentation-border/watermark masks are stored as normalized rectangles and source bytes are unchanged. Source-image publication/reuse permissions remain as recorded; non-CC originals stay private.

exploratory-distributions.png compares author-balanced distributions (red Phoenix 200 / blue Phoenix II). It describes this scan collection, not intrinsic film differences: lighting, content, format, lab, inversion, compression and author remain confounded. No grain/halation recovery, accuracy claim or Phase 3 fitting.

Executed: import_provided.py, bounded single-image/article fetchers, quality.py, review_provided.py, explore.py, plot_exploration.py, report_provided_batch.py. All 22 provided source/cache SHA256 pairs match; 32 exploratory hashes are unique. Source-byte preservation is asserted during each analysis. Five provenance-rule unit tests pass. Per-attempt errors remain in the build journal, with failed source records synchronized into the manifests.

Next: broaden authors/scenes and verify reuse permissions; refine masks; establish independent source/photographer split components. Neither the quantity target nor a B provenance grade is evidence of real-film accuracy.
