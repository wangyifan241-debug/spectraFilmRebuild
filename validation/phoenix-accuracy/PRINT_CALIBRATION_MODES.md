# Print calibration comparison modes — design v1

## Implemented: frozen_parent_print

The existing/default mode remains unchanged: one physical hypothesis changes,
the candidate gets a new material ID/version, and the parent's print-pack values
are copied to that ID in private staging. No fitting, neutralization or automatic
white balance is performed. Reports retain original and staged pack SHA256 plus
the explicit comparison mode. Legacy plans without a mode mean this mode.

This measures the candidate's behavior **under the original printing conditions**.
A changed dye/base/sensitivity model can shift neutral balance; that is a relevant
fixed-print result, not necessarily a complete measure of attainable calibrated color.

## Design only: separate_recalibration

Not executable this round. Plans requesting this mode fail explicitly; it cannot
silently use frozen calibration or run a fitter. No current film/print data change.

A future implementation must produce a separate versioned calibration artifact,
never mutate a film profile or the frozen parent table. Required records:

- parent and candidate material hashes; solver/tool commit and binary hash;
- paper, illuminant, spectral resources, objective, allowed print controls,
  bounds, initial values, seed, convergence/residuals and failed-fit behavior;
- licensed neutral/calibration inputs and exact grouping manifest hashes;
  **training/calibration split only**, no validation or held-out test tuning;
- original and fitted print-pack values/hashes, recipe hashes and provenance;
- explicit assumptions and calibration limitations, including model-neutral
  fitting vs independent real-film/reference-neutral fitting.

Produce a four-cell factorial comparison on the same held-out evaluation inputs:

| Material | Original parent print pack | Its separately fitted pack |
|---|---|---|
| Parent v1 | frozen control | parent refit control |
| Candidate | isolated fixed-print physical change | physical change + printing adaptation |

Keep all four artifacts and report their paired deltas separately. Candidate vs
parent with original packs is the physical-change comparison. Candidate own-fit
vs candidate original-pack isolates the calibration effect for that material.
The final fitted-vs-fitted difference combines both effects and must never be
presented as pure emulsion improvement. Refit the parent too: comparing only a
refitted candidate against an unrefitted parent biases the result.

ROI registration, correspondence evidence, data leakage rules and photographic
acceptance remain mandatory. A lower training neutral residual is an internal
calibration result, not real photographic accuracy. Any future renderer/recipe
adapter needs explicit mode/version support before these runs become comparable.
