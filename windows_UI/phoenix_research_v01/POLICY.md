# DecoupledDeviceToneV01 — preregistered local candidate

Scan-only, preview only, exposure EV 0, as-shot WB, deterministic film stages.
One candidate, no look tuning after results. Legacy files and materials stay unchanged.

Device: common intersection of the two stocks' finite wavelength support; normalized
D50 Gaussian detectors (610/540/450 nm, sigma 30/30/25). Fit a white-constrained
linear detector-transmission to XYZ matrix using 400 known smooth transmission
spectra. Test on 100 distinct spectra; no scene RGB or film outputs are targets.
Spectra are sums of three smooth absorption lobes with deterministic parameters.
XYZ converts to ProPhoto transmission. Do not apply this matrix after inversion.

Tone hypothesis: invert only the scalar device Y neutral response, then divide
the device RGB neutral transmission at that tone by measured RGB transmission.
Multiply this reciprocal chromatic ratio by reconstructed neutral exposure.
This is an explicitly hypothetical positive reconstruction, not film chemistry.
Endpoint saturation is recorded; no negative or invalid transmission is silently
clamped (invalid values remain NaN in diagnostic buffers).

Gates fixed before execution: shader probe absolute error <2e-5; held-out XYZ
absolute mean error <0.03 and maximum <0.15 (white Y=1); neutral relative channel
spread <0.005; no nonfinite positive pixels; neutral and biased-ramp Y no decrease
beyond 1e-5; normalized exposure-step second difference <0.25; synthetic skin
adjacent hue change <15 degrees at chroma >2; no increase in display clipping
fraction above legacy by 0.001. Linear >1 is headroom, not clipping.

Failure means retain actual images and diagnosis, skip GUI preset/EXE promotion,
and stop. The compiled diagnostic runner is not an accepted Research GUI build.
