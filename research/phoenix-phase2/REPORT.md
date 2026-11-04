# Phase 2 — model comparison on the existing 32 references

Status: executed exploratory comparison, not film-model validation. No new reference acquisition or parameter fitting. Data: 16 images / 5 authors for Phoenix 200; 16 / 9 for Phoenix II. Source/rights/ICC/masks remain those in the Phase 1 manifest.

## Author and scene sensitivity
Author-balanced II minus I mean Lab D65: L +1.659, a -3.533, b -0.577. Descriptive author-bootstrap 95% intervals are respectively [-11.541,13.268], [-9.090,2.814], [-8.529,7.308]. All include zero. Leave-one-author-out ranges: L [-1.800,7.277], a [-5.951,-2.335], b [-2.606,1.817]. The greenward a direction survives this leave-one-out check but not the broader bootstrap; it is a collection observation, not a stable physical film trait.

Median luminance and p95–p05 differences also change sign under leave-one-author-out. The bootstrap samples author means, not independent rolls or labs; it does not remove shared-scanner or scene confounds. No significance or accuracy claim follows.

Broad matched categories are sparse: architecture 3 vs 1 images (one author each), flowers 3 vs 5 (two authors each), vegetation 3 vs 3 (one vs two authors), landscape 2 vs 1 (two vs one author), indoor 1 vs 1, human portraits 0 vs 4 (zero vs three authors). Flower types and lighting differ; indoor station lights and an indoor object are not interchangeable calibration scenes. Author/scene partition definitions and exact IDs are in comparison.json. Raw portrait validation cannot be inferred from the red reference set because it has no human portraits.

## Actual Sony RAW output and image delivery
build/phoenix-phase2/index.html opens the real saved engine images: digital baseline, Round 5 I, Round 5 II, encoded-RGB absolute difference x10, portrait histogram overlay and manually inspected skin patches. These are verified copies of actual prior engine renders, not a newly executed render or a Research v0.1 output. Their PNG and canonical RGB8 hashes and original execution provenance are bound in comparison.json. This deliberately reuses evidence instead of rerunning Phase 0.

RAW: DSC02443.ARW, SHA256 2f0fbdac0097e7fb2a91c049fdb869ff45354208a9e60da5e0df471d94f91202, original 5120×7168, stride 7, linear ProPhoto compatible decode/as-shot WB, zero exposure adjustment and stochastic effects off. The digital baseline uses the same output shoulder and display conversion. Thus it differs from the earlier unshouldered digital baseline. Selected film resources are the same Round 5 data used by the audited path; actual loaded profiles/executable/resource receipts remain in the original report.

Paired software-only RGB MAE /255 and mean/P95 DE00 in **Lab D65**:
- Round 5 I vs shared-shoulder digital: 4.1531; 2.5799 / 4.2703.
- Round 5 II vs digital: 2.2623; 1.3068 / 2.3320.
- II vs I: 2.1702; 1.6736 / 3.2292.

These numbers use RGB8 inspection and Lab D65, whereas historical Phase 0 metrics used Bradford D50 and sometimes a different baseline. Do not silently mix the metric conditions or interpret their small numerical difference as a renderer change.

## Region and distribution evidence
Manually inspected skin rectangles are explicit in comparison.json, not automatic semantic segmentation. Sony patch mean Lab: digital (58.167,10.037,14.834), Round 5 I (57.538,12.802,21.173), II (57.532,10.483,15.702). I therefore adds predominantly warm chroma in this input region; II stays close to the digital baseline. The blue-reference cheek patch is (55.645,22.049,48.257), but it is a different person outdoors beneath an orange parasol with a different scanner. No unpaired skin DeltaE or fitting target is reported.

The four available blue-reference portrait histograms have broader/different L/a/Y distributions than this Sony image, while the two Round 5 histograms remain close to its digital baseline. This is a measured collection mismatch, **not an established film-model error**: content, exposure, lighting and scanner are not matched. There is no supported red-portrait target. Neither an overall saturation boost nor forcing the Sony patch toward the orange reference patch is justified.

## Concrete conclusions and priorities
Confirmed engine behavior: Round 5 is nonidentity; I and II differ; synthetic per-film neutral inversion and reconstruction regression suppress differences in controlled Phase 0 ablations. The final scan on this RAW is close to the digital distribution, particularly II. This is a scanner-policy/design limitation when the research objective is to retain film response, not proof that the measured material curves are wrong.

Not established: exact generation-specific saturation, skin hue, toe/shoulder shape, grain or halation parameters; physically accurate reconstruction from these JPEGs; camera/scanner-independent population differences. No observed JPEG histogram is treated as truth.

Prioritize: separate device calibration from film-response inversion; test neutral and exposure continuity before applying any new scanner; freeze whitepoint/ICC/display conversions; stratify reference likelihoods by author/scene/scan rather than fit one pooled histogram. Keep material profiles unchanged. A/B/C/D and candidate E are controls, not candidates to promote directly. Candidate E's extreme neutral drift remains a rejection reason.

## Research v0.1 decision
Evidence does not identify defensible numeric material/color parameters. No Research model, fake Research PNG or new Windows EXE was produced. Phase 3 is limited to a preregistered controlled experiment design in a separate commit/PR. Rendering an arbitrary stronger look would not satisfy the evidence requirement. The HTML explicitly marks Research v0.1 unavailable.

Run: python tools/phoenix-phase2/compare.py; python tools/phoenix-phase2/plot_stability.py. Real reference originals and the Sony portrait remain local, not Git assets. This report's plots contain only distributions; no underlying photo redistribution is implied.
