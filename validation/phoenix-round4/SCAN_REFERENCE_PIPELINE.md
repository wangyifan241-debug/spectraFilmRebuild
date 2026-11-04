# Spectral scan reference experiment

Status: idealized proxy; NOT VERIFIED against film or any scanner. Not SP3000 or Noritsu. Independent from Optical Print and existing Phoenix v1. No commercial LUT.

The runnable Node module scan-reference.cjs accepts wavelength-resolved total negative density, matching clear-base density, illuminant and detector responses. Wavelength grid must be uniform; all arrays must cover exactly the same samples. It evaluates Beer–Lambert transmission and base-referenced detector integrals. Base correction is performed before integration, not by subtracting three RGB offsets. A fixed declared density range inverts integrated relative transmission into a positive proxy. This operation is an instrument-space normalization, not recovery of scene radiance. It cannot resolve metamerism or replace measured neutral characteristic curves.

Detector channels may be supplied as CIE XYZ colour matching functions with a declared illuminant; arbitrary detector channels must not be labelled XYZ. No implicit XYZ/ProPhoto/sRGB conversion, white adaptation, gamut mapping, transfer curve or ICC tagging is performed. Retain unclipped output and report clipping before display encoding. Production export needs explicit source white, chromatic adaptation, output profile, TIFF16 encoding and preview/export agreement tests. Those integrations remain pending.

Run tests: `node validation/phoenix-round4/scan-reference.cjs --test`.
Run an independently supplied spectral input: `node validation/phoenix-round4/scan-reference.cjs input.json > result.json`.

Input fields: density, base, illuminant (equal-length arrays, at least two samples), detectors (three equal-length arrays), densityRange (positive scalar), wavelengthStepNm (positive scalar), detectorSpace (nonempty provenance label). No synthetic demonstration is a film measurement. No fitting is enabled.
