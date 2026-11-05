# Research v0.2 — one bounded, luminance-normalized reconstruction

Preregistered before running v0.2. Preserve Device and Tone v0.1 verbatim.
No scene RGB blending, stock-profile change, saturation slider or relaxed gate.

## Why v0.1 can fail

For device-linear ProPhoto transmission T, reference neutral N(e), scalar
neutral-inverse exposure e, v0.1 returns P_c=e*N_c(e)/T_c. Its log derivative is
d ln P_c/dEV = d ln e/dEV + (d ln N_c/d ln e)*(d ln e/dEV)
              - d ln T_c/dEV.
The last term grows at small T_c. Each channel gets a different gain; their
weighted sum is not constrained to e. A neutral luminance lookup alone cannot
prevent a non-neutral luminance reversal. Estimated XYZ-to-RGB transmission
also has no general positive-channel guarantee. These mechanisms can amplify
orange/red and blue/cyan; they do not prove a measured film hue is wrong.

## The sole candidate

Keep the same independently calibrated optical device and neutral tone table.
Only replace positive reconstruction. For positive finite T and interpolated
N(e), form u_c=ln(N_c/T_c); remove its luminance-weighted mean. Radially bound
this log-chromatic vector by b=u*tanh(||u||/L)/(||u||/L), L=ln(4). Form s=exp(b)
and P=e*s/(w dot s), w=(.2880402,.7118741,.0000857).

This is an opponent log-transmission reconstruction, not device calibration.
No skin targets or subjective image statistics select L: ln(4) is a fixed
maximum log-chromatic norm (two-stop instrument reconstruction bound), not an
empirical film coefficient. Small residuals keep their first-order response;
extremes compress continuously instead of diverging as 1/T. Because ||b||<=L,
any channel ratio <=exp(2L)=16. P is strictly positive and w dot P=e by
construction, so chromatic reconstruction cannot reverse its scalar tone.
For T=N, u=0 and P=(e,e,e). The same neutral interpolation residual as v0.1
is measured, never repaired by fitting a new look.

Scalar e remains the inverse of the measured neutral device-Y table. Thus P's
Y is continuous and nondecreasing as measured device-Y decreases, including
endpoint plateaus. Monotonicity along arbitrary scene exposures additionally
requires device-Y to decrease: verify all supplied ramps rather than claiming
a global proof for arbitrary spectra/film. Table knots have continuous values,
not guaranteed continuous derivatives. Invalid/nonpositive inputs are rejected,
not clamped. Finite/negative/headroom stats retained before display.

## Unchanged gates, added domain separation

Keep v0.1 thresholds: shader 2e-5; spectral held-out XYZ MAE .03/max .15;
neutral relative spread .005; finite outputs; Y step >=-1e-5; normalized second
difference <.25; skin adjacent hue <15 degrees where Lab chroma >2;
display clipping increase <=.001. Apply monotonicity to all eight ramps.
Report hue twice: unencoded float32 ProPhoto -> XYZ D50 -> Lab D50 without
clipping or quantization; and exported RGB8 sRGB -> Lab D65. Never confuse the
two whitepoints or use their numeric difference as an accuracy claim.
Also check the channel-ratio bound and Y=e reconstruction residual <2e-6 relative.

Same RAW, ramps, EV 0, shoulder and output transform. Preserve every v0.1
artifact and EXE. Four-way Digital/Round5/v0.1/v0.2 comparison, including face,
arm, white uniform, dark clothing and background crops; ROIs are consistency
diagnostics only. No optical-paper claim. No original-resolution export.
Run once, retain failures and stop for visual review. GUI presets remain
conditional on all engineering gates; passing these is not film validation.
