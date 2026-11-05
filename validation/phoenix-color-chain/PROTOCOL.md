# Frozen color-chain localization, not a v0.3 model

One diagnostic comparison fixed before execution: approximate detector->XYZ vs
direct 81-band XYZ integration of the identical virtual negative. Same dye
profile, zero base, missing-dye sanitization, linear ProPhoto conversion,
per-stock neutral reconstruction formula, -6..6 EV endpoints and display path.
Recompute the neutral reference through the corresponding instrument to keep
the reference internally consistent. No image-fitting or look coefficients.

Direct integration is an oracle for the chosen PROXY spectral model only; it
is not measured Phoenix transmission. The profile already borrows dye spectra.
Do not select a model because the resulting photo looks closer to Digital.

Capture negative CMY densities, exact/approximate XYZ and ProPhoto transmission,
neutral references and positives. Verify GPU spectral reduction against a CPU
81-band reference on fixed sample indices. Check RGB/XYZ orientation and white.
Decompose fixed-exposure log residual change into reference and pixel terms:
delta ln(N/T) = ln(N_exact/N_fit) - ln(T_exact/T_fit).
Quantify before/after neutral correction and bounded-positive reconstruction.
Inspect local Jacobian/cross-channel response using a fixed +/-1 percent R/G/B central-difference probe at five EVs (-2..2),
without fitting an inverse or new color matrix. Check the proxy dye matrices
of the two stocks for identity and provenance; no edits to them.

No endpoint extrapolation. Keep original 15-degree/C>2 RGB8 gate and report
native RGB16, high-byte RGB8 and correctly rounded RGB8 separately. Include
linear ProPhoto->XYZ->Lab D50 evaluation; never conflate it with display D65.

If this localizes an approximation error but does not uniquely identify the
photographic positive-color mapping, deliver diagnostic evidence and stop.
No automatic candidate or preset, no material changes, no PR merge.
