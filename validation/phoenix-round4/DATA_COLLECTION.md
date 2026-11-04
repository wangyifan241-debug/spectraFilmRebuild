# Real film collection contract v1

Dataset status: EMPTY / NOT VERIFIED. No measured or fitted results.

Each sample manifest must include generation (Phoenix 200 or Phoenix II), batch, roll_id, scene_id, split, ISO, EI, exposure time/aperture/compensation, scene description, light SPD or declared unknown, chart model/serial/reference source, C-41 lab/date/time/temperature/chemistry/push-pull, negative file path, scanner model/serial/light SPD or unknown, scan settings, ICC file/path/hash, all automatic correction switches, file SHA256, source/owner/permission/license and permitted redistribution. Preserve originals; derivatives record parent hash and recipe hash. Unknown values are explicit null with reason, never fabricated.

Split by connected roll AND scene groups before fitting: all records sharing either roll_id or scene_id must have the same split. Adjacent bracket exposures cannot cross splits. Freeze validation before optimization. Require independent validation scenes and rolls, provenance and lawful access before fitting. Empty dataset must never pass a real-film accuracy gate.

Acquisition: include clear developed base, dark instrument reference, calibrated transmitted-light measurements where available, neutral step wedge, chart and exposure brackets. Record measurement geometry, units, uncertainty and spectral support. Scanner RGB and manufacturer plots are not measured dye spectra. Keep optical-print, software comparison and true-film validation reports distinct.

Optimization remains disabled until paired reliable data and independent splits exist. Order: neutral/exposure/luminance, base/dyes, scan conversion/balance, held-out D50 Lab ΔE00, exposure/scene stability. Report gray, luminance, hue, chroma and clipping alongside ΔE00. A fitted photo LUT is not physical validation.
