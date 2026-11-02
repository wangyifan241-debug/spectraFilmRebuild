# Phoenix round 2 — source audit and preregistered experiments

Date: 2026-11-02. Status: research in progress; no photographic accuracy claim.

## Source review before candidate construction

Primary I: https://www.harmanphoto.co.uk/amfile/file/download/file/1963/product/2143/
September 2024, seven pages. Page 2 identifies a 2856 K tungsten wedge spectrogram; page 3 supplies the I characteristic graph, standard C41. High contrast is stated, not a numerical gamma constraint.

Primary II: https://www.harmanphoto.co.uk/amfile/file/download/file/1968/product/2208/
July 2025, five pages. Page 2 identifies D55 daylight. The reviewed document has no characteristic-curve section. Page 1 states more normal contrast and wider latitude than I; these do not determine channel slopes, toe positions, Dmax or an exposure anchor.

Secondary candidate: https://www.japancamerahunter.com/2025/07/film-newfilm-news-harman-phoenix-ii-risess-harman-phoenix-rises-again/
Existing source artifact and CSV remain unchanged. The II article context conflicts with the Phoenix I caption recorded in characteristic-candidate.json. Classification: **Unverified Generation**. Neither a lower apparent slope nor publication date authenticates II attribution. No manufacturer-measured II characteristic curve is established by this review.

## Mathematical audit

Let q_c(lambda) be the digitized ordinate, lambda in nm. Existing code assumes S=10^q, hence log10 S=q. The printed Relative Sensitivity label does not establish logarithmic scaling or exposure criterion. A linear interpretation S=q (positive support) is a controlled alternative, not a correction justified by manufacturer metrology.

Existing interpolation is linear in q after dropping null samples; it bridges gaps rather than preserving missing observations. Endpoints are not extrapolated: outside support S=10^-8. This is an assumed numerical floor, not measured cutoff. Channels are assigned by labeled blue/green/red branches, not plotted ink color alone; crossings remain uncertain.

Existing I correction divides S by B_lambda(2856)/B_550(2856). B_lambda is proportional to lambda^-5/[exp(c2/(lambda*T))-1], with c2=1.438776877e7 nm K. II receives no analogous D55 division. A wedge illumination caption alone does not say whether the chart has already been corrected for spectral radiant exposure. Dividing an already corrected sensitivity would double-correct it. Recovering intrinsic sensitivity requires the instrument response, calibration convention and density criterion; those are unknown.

Existing normalization sets a_c = sum(S_donor,c B_5500)/sum(S_c B_5500), then S'_c=a_c S_c. The wavelength step cancels only for the same uniform grid; physical response integrals require sum(S E_lambda delta_lambda), delta_lambda=5 nm. Channelwise factors impose donor neutral ratios and erase relative channel speed evidence. A shared factor preserves channel ratios but can move neutral balance. Both may change non-neutral colors through overlap and subsequent nonlinear development.

D55 is a CIE daylight spectral distribution, not a 5500 K Planck radiator. Replacing one by the other is a model assumption, even at similar correlated color temperature. A D55 candidate must use a documented spectral table/formula and preserve the existing 5500 K setup elsewhere; changing the engine illuminant simultaneously would be an interaction experiment.

## Preregistered single-factor experiments

All inherit v1 dyes, base, couplers, grain, halation, exposure anchor and frozen parent print pack. No recalibration, aesthetic selection or LUT. Each requires unique stock ID, incremented semantic version, exact parent Git-blob SHA256 and photographic_fidelity_validated=false.

1. S-ordinate: linear positive q versus exponential q, preserving current illuminant treatment and normalization policy. Nulls remain documented; positivity floor explicit.
2. S-illumination-I: omit tungsten division versus v1, preserving exponential interpretation and channelwise normalization.
3. S-normalization: one shared normalization factor versus donor channelwise normalization, preserving spectral shape construction.
4. S-daylight: replace only normalization weighting B5500 with documented D55. Do not call this recovered measured sensitivity.
5. C-II-width: bounded monotone horizontal dilation about the unchanged exposure anchor, width factors 1.15 and 1.30, preserving density bounds. This expresses wider-latitude uncertainty without claiming measured II curves. Evaluate derivative and extrapolated endpoints. Keep the unverified current candidate as baseline.

Any joint ordinate/illumination/normalization experiment must be separately labeled interaction and compared against its single-factor arms.

## Validation and decisions

Bind the final pushed PR6 repair SHA before candidate validation. Record renderer binary hash and source inventory, fixed RAW byte hash, decode settings and complete recipe before renders. Existing input.png is not a RAW substitute. Test gray and 24 synthetic color patches, skin/green/blue, EV -6..+6 including +/-2, channel finite values and monotonic exposure response; report toe/mid/shoulder derivatives, RGB, D50 Lab and relative Y changes. Native preview/export and unaffected-stock regressions must be actual executions, separated from historical credentials. Unsupported tests remain Not Run.

Synthetic Lab differences are model sensitivity, never true-film DeltaE00 accuracy. With no independent paired film references, default physical decision is Inconclusive; retain Experimental v1 in product. Engineering failure implies Reject. Adopt requires source evidence and unacceptable-regression review, not favorable colors.

最终来源纠错见 SOURCE_REVIEW_FINAL.md：直接放大原图确认图注是Phoenix II，早期Phoenix I抄录错误；厂家原始数据真实性仍未认证。本文旧来源结论由该文件取代。
