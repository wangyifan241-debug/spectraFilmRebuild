# Actual source evidence

Source locations and exact file SHA256 captured for the color-chain experiment. No production file was edited.

## engine/src/core/colour.cpp

SHA256: `ee47ffc6987a25145279609979b137e4462251c7c80dcfa69ec8e567c57094da`

[160](../../engine/src/core/colour.cpp#L160) `bool Colour::matrix_RGB_to_XYZ(const std::string& cs_name, const double* illuminant_xy,`

[173](../../engine/src/core/colour.cpp#L173) `bool Colour::matrix_XYZ_to_RGB(const std::string& cs_name, const double* illuminant_xy,`

## engine/src/core/numeric.hpp

SHA256: `cf704ad852162d8f8b3dd8cc2fa1cf065830b95e904e3ddac27c1c5886a54832`

[27](../../engine/src/core/numeric.hpp#L27) `struct Mat3 {`

[54](../../engine/src/core/numeric.hpp#L54) `void apply(const double in[3], double out[3]) const {`

## engine/src/core/profile.cpp

SHA256: `0ee703a339ac0b20bb93a2ce944ef1121638724090589f964e06c219ea4a7105`

[198](../../engine/src/core/profile.cpp#L198) `// density_curves - nanmin(density_curves, axis=0), and the nanmax of`

## engine/src/shaders/vulkan/spk_spectral_epilogue.comp

SHA256: `38c265ca3c448e6c9d09c84f619fd7b1057070991831ff23c54a76175e604a54`

[28](../../engine/src/shaders/vulkan/spk_spectral_epilogue.comp#L28) `float density = c0 * characteristic.value[3u * wavelength]`

[32](../../engine/src/shaders/vulkan/spk_spectral_epilogue.comp#L32) `float transmission = exp2(-density * 3.321928094887362);`

[40](../../engine/src/shaders/vulkan/spk_spectral_epilogue.comp#L40) `if (meta.value[2] == 0u) value = log(value) * 0.4342944819032518;`

## windows_UI/phoenix_research_v01/Research.hpp

SHA256: `ee1a43f7271432603ad3dc4f68fc92fc424abbcaccaf3366925fac4967475bba`

[3](../../windows_UI/phoenix_research_v01/Research.hpp#L3) `struct Device {`

[17](../../windows_UI/phoenix_research_v01/Research.hpp#L17) `double aa=1e-8,ab=0,bb=1e-8,ay[3]={},by[3]={};`

[20](../../windows_UI/phoenix_research_v01/Research.hpp#L20) `need(l.colour.matrix_XYZ_to_RGB("ProPhoto RGB",nullptr,"CAT02",xyzRGB,err));`

[34](../../windows_UI/phoenix_research_v01/Research.hpp#L34) `std::vector<float> optical(const std::vector<float>& logT){auto out=logT;for(size_t i=0;i<out.size();i+=3){double s[3],xyz[3],rgb[3];for(int c=0;c<3;c++)s[c]=std::pow(10.,logT[i+c])-1e-10;detectorXYZ.apply(s,xyz);xyzRGB.apply(xyz,rgb);for(int c=0;c<3;c++)out[i+c]=float(rgb[c]);}return out;}`

[36](../../windows_UI/phoenix_research_v01/Research.hpp#L36) `struct Tone {`

## windows_UI/phoenix_research_v02/ToneV02.hpp

SHA256: `de28cc1650968ce3e4be955d232a42c3a35b241a096ce8de771238d363e87a18`

[17](../../windows_UI/phoenix_research_v02/ToneV02.hpp#L17) `double exposure=.184*std::exp2(-6.+.125*(k+f)),u[3],mean=0;`

[21](../../windows_UI/phoenix_research_v02/ToneV02.hpp#L21) `u[c]=std::log(ref/t);mean+=weight[c]*u[c];`

[24](../../windows_UI/phoenix_research_v02/ToneV02.hpp#L24) `double scale=norm>1e-12?std::tanh(norm/bound)/(norm/bound):1.;`

[26](../../windows_UI/phoenix_research_v02/ToneV02.hpp#L26) `for(int c=0;c<3;c++)out[i+c]=float(exposure*shape[c]/sum);`

## windows_UI/phoenix_color_chain_probe/DirectSpectral.hpp

SHA256: `6e1dc12cb91783c09574319f5f83d5aabb51df7799d1ccf8dfa0a40f63620d60`

[6](../../windows_UI/phoenix_color_chain_probe/DirectSpectral.hpp#L6) `weights=lab.gpu->upload_persistent_f32(device.xyzWeights.data(),device.xyzWeights.size(),err);need(bool(weights));`

[12](../../windows_UI/phoenix_color_chain_probe/DirectSpectral.hpp#L12) `lastXYZ=lab.alloc(negative);uint32_t meta[3]={uint32_t(negative.pixels()),81,1};`

[15](../../windows_UI/phoenix_color_chain_probe/DirectSpectral.hpp#L15) `for(size_t i=0;i<xyz.size();i+=3){double physical[3],out[3];for(int c=0;c<3;c++)physical[c]=double(xyz[i+c])-1e-10;device.xyzRGB.apply(physical,out);for(int c=0;c<3;c++)rgb[i+c]=float(out[c]);}`

## engine/src/pipeline/pipeline.cpp

SHA256: `3d846df0f5e8837852ebec93ae5f81b95eaa53b5bd7ed9d32e1fdaf660125cea`

[1316](../../engine/src/pipeline/pipeline.cpp#L1316) `bool Pipeline::node_film_curves(const Image& in, Image& out, std::string& error) {`

[1322](../../engine/src/pipeline/pipeline.cpp#L1322) `bool Pipeline::node_dir_couplers(const Image& cmy, const Image& log_raw, Image& out,`


## Calibration layout observer

SHA256: `2e0f0a3c54bccfe74a50ecbfbdaf40d4b6907091d50f26048444b3a5d9cb546a`

[main.cpp:48](../../windows_UI/phoenix_color_chain_probe/main.cpp#L48) `std::vector<float> permuted(neutral.size());for(int i=0;i<97;i++)for(int c=0;c<3;c++)permuted[3*i+c]=neutral[3*((i*37)%97)+c];`

[main.cpp:51](../../windows_UI/phoenix_color_chain_probe/main.cpp#L51) `std::vector<float> uniformNeg(neutral.size());for(int i=0;i<97;i++){std::vector<float> constant(97*3,neutral[3*i]);auto uniform=lab.values(lab.film(pipe,lab.upload(constant,97,1)));for(int c=0;c<3;c++)uniformNeg[3*i+c]=uniform[48*3+c];}`

[main.cpp:54](../../windows_UI/phoenix_color_chain_probe/main.cpp#L54) `lab.report["neutral_layout_controls_"+label]=QJsonObject{{"dir_couplers_active",p.film_render.dir_couplers.active},{"diffusion_size_um",p.film_render.dir_couplers.diffusion_size_um},{"diffusion_tail_um",p.film_render.dir_couplers.diffusion_tail_um},{"diffusion_tail_weight",p.film_render.dir_couplers.diffusion_tail_weight},{"permutation","index -> (37*index) mod 97, restored before comparison"},{"uniform_probe","97 independent homogeneous 97x1 frames; center sample retained; no parameter changes"}};`
