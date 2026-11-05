# Phase 3 local shader-domain checkpoint

Status: source inspection only; GPU numerical verification and candidate execution pending.

- `engine/src/shaders/vulkan/spk_spectral_epilogue.comp`: density = dot(CMY, wavelength dye density) + base; transmission = 10^(-density). Detector integral is multiplied by epilogue gain, offset, floored at zero and incremented by 1e-10. With meta[2]=0 output is log10 of this quantity.
- `engine/src/desktop/phoenix_round5_scan.hpp`: spectral() uses meta[2]=0, gains 1, offsets 0 and zero base. Its output is log10(normalized detector transmission + epsilon), not XYZ and not positive RGB. Detector validity masks currently depend on stock.
- `spk_curves.comp`: componentwise piecewise linear table interpolation with endpoint saturation. Scanner tables map log10 detector transmission to log10 original neutral exposure. This is stock-specific tone inversion.
- `spk_round4_scan.comp`: x=10^input; output[c]=sum_j matrix[3*c+j]*x[j]. Matrix is row-major and multiplies a column vector. It does not clamp negative output.

Implementation implication: independent transmission-device calibration requires a separate linear transmission stage and an explicit positive reconstruction policy. A matrix trained on transmission cannot replace the existing post-inversion matrix. Stock-dependent validity masks must also be addressed explicitly before claiming a shared device.

No candidate has been accepted or rejected by numerical evidence at this checkpoint. No profiles, Round 5 implementation or original-resolution export guard changed. No fresh images, executable or Draft PR produced yet. Do not interpret this document as completion of the experiment contract.
