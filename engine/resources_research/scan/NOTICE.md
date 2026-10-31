# Offline scan-data fitting research — not runtime materials

Original author: **OwlMightyCh**, Film Scan Calibration, CC BY 4.0.
Source repository: https://github.com/OwlMightyCh/film-scan-calibration
Pinned commit: `8d03a0e0c111625b8a88fb8647a35d95b491249d`.
The unmodified original data files are `data/films/ProImage100_*.json` at
that commit; this directory preserves their exact bytes and `LICENSE-DATA`.
Source URLs: append the corresponding filename to
https://github.com/OwlMightyCh/film-scan-calibration/blob/8d03a0e0c111625b8a88fb8647a35d95b491249d/data/films/

| Original | SHA256 |
|---|---|
| ProImage100_datasheet_curves.json | ec23f1756de5597a86ed779676ba8546595c6e590b9749b53abb41809ee0e75e |
| ProImage100_spectral_sensitivity.json | b1a6bfe6613b9a6b8430f57fb614edc8c69bb164d35e11c84271029fc94d4ea7 |
| ProImage100_dye_density.json | 342ad845bd70f0f70ea008e8ac670db43e15352260ca12244c14a89baf34c968 |

Licence: https://creativecommons.org/licenses/by/4.0/ . Datasheet copyright
and calibration uncertainties remain with their respective holders. Preserve
attribution and identify generated fits as modifications. No third-party
standards/camera SSFs or source code are copied from that repository here.

`spk_material_fit` produces a **new, modified Status-M curve-fit report**:
bounded three-normal-CDF mixtures plus offset, explicit residuals and a
withheld interleaved sample set. Source points/axes are not overwritten.
Its fit does **not** establish an intrinsic emulsion-layer response. The
source dyes are surrogate-basis reconstructions, not independently measured
Pro Image dye layers; spectra are incomplete. Accordingly these inputs and
outputs are not automatically registered, bundled as runtime Profiles or
claimed as a fully calibrated Pro Image material. See
`windows_UI/material_fit/README.md` for remaining gates.
