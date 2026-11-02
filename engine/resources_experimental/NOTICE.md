# Phoenix experimental materials v0.1.0

Two independent versioned material models: `harman_phoenix_200_exp_v1` and
`harman_phoenix_ii_200_exp_v1`. These are approximations, not measured Phoenix profiles.
HARMAN has not endorsed these models. The brand identifies the intended stock.

Dye spectra are a derivative of the spektrafilm Kodak Gold 200 profile by
Andrea Volpato (https://github.com/andreavolpato/spektrafilm), CC BY-SA 4.0.
The two profile JSON files remain CC BY-SA 4.0:
https://creativecommons.org/licenses/by-sa/4.0/ . Preserve attribution, this
notice and modification records in redistributions and derivatives. The bundled
spektrafilm license supplies the original project's attribution and license text.

Changes: HARMAN chart-derived sensitivity; normalized response fitted through an
explicit synthetic Gaussian densitometer; modeled purple unmasked base; estimated
exposure anchor, grain particle area and halation parameters. Gold dyes are a
proxy, not a claim that HARMAN uses Kodak dyes. Estimated material DIR coupling
is 0.15 of the generic negative preset, preserving monotone transformed exposure
without rewriting the chart curves; it is not measured HARMAN chemistry. Manufacturer graphs retain their
own rights and are not relicensed by this notice or bundled with the application.

Evidence grades: A = manufacturer chart or explicit statement; B = secondary
chart with incomplete provenance; C = mathematical/borrowed/estimated proxy.
Individual data/assumptions and hashes are embedded in each profile metadata.
Both sensitivity ordinates have an unconfirmed linear/log interpretation.
Phoenix II's provisional characteristic comes from a JCH launch article whose
image caption says Phoenix I; its generation is not independently verified.
Do not present that curve as an authenticated official Phoenix II measurement.

Grain/halation use the existing physical engine models and independent user
amounts. No LUT substitutes for the film pipeline. Estimated baseline parameters
are in `info.render_model` v1. They are adjustable by producing a new immutable
material version; existing exposure/grain/halation UI controls remain available.

Source extraction, generator, assumptions, diagnostics and upgrade instructions
live in `engine/resources_research/phoenix*` in the source tree. No offline tools
or research source images are required at application runtime.

The companion neutral-filter database preserves every existing calibration and
adds explicit generic C/M/Y [0,65,55] packs for these two stocks, for each existing
paper/illuminant pair. For Portra Endura / TH-KG3, model-grey balances solved
through the actual Vulkan print chain replace those starting packs:
I [0,57.09484,79.04972], II [0,85.57272,96.63273]. These are C-level model-output
calibrations, not measurements of physical HARMAN film. Other pairs retain
generic starting estimates. Explicit packs prevent inheriting the previously selected
stock's print state. The derivative database remains CC BY-SA 4.0 with the same
spektrafilm attribution. Imported custom material versions need their own print
calibration; custom imports do not automatically install global filter tables.

Software tests exercise schema, finite numerics, exposure, color patches, grain,
halation, UI selection/history, and preview/export agreement. They do NOT validate
photographic fidelity to actual HARMAN film. Replace measured inputs and generate
a new stock/version when measurements become available; preserve v1 for recipes.
