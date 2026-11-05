#pragma once
// A diagnostic reference integral for THIS proxy negative, not measured film truth.
struct DirectSpectral {
 Lab& lab;Scanner& scanner;Device& device;gpu::BufferRef weights;Image lastXYZ;QJsonObject receipt;
 DirectSpectral(Lab& l,Scanner& s,Device& d):lab(l),scanner(s),device(d){
  weights=lab.gpu->upload_persistent_f32(device.xyzWeights.data(),device.xyzWeights.size(),err);need(bool(weights));
  double matrix[9];device.xyzRGB.to_row_major(matrix);
  std::vector<float> dyes(243);need(lab.gpu->read(scanner.chd.get(),0,dyes.data(),dyes.size()*sizeof(float),err));
  receipt={{"XYZ_integral_weights_D50",array(device.xyzWeights)},{"XYZ_to_ProPhoto_row_major",array(Vec(matrix,matrix+9))},{"sanitized_dye_density_GPU_f32",array(Vec(dyes.begin(),dyes.end()))},{"base","zero, exactly as the frozen scanner; missing dye wavelengths sanitized to zero as in frozen scanner"},{"domain","linear XYZ of spectral transmission under D50; full 81-point target matches Device calibration target definition"},{"classification","diagnostic integral of the same proxy negative, not another material or validated film reconstruction"}};
 }
 std::vector<float> transmission(const Image& negative){
  lastXYZ=lab.alloc(negative);uint32_t meta[3]={uint32_t(negative.pixels()),81,1};
  need(lab.gpu->dispatch("spk_spectral_epilogue",{gpu::Arg::buf(negative.buf),gpu::Arg::buf(scanner.chd),gpu::Arg::buf(scanner.base),gpu::Arg::buf(weights),gpu::Arg::buf(scanner.ep),gpu::Arg::inline_bytes(meta,3),gpu::Arg::buf(lastXYZ.buf)},negative.pixels(),err));
  auto xyz=lab.values(lastXYZ),rgb=xyz;
  for(size_t i=0;i<xyz.size();i+=3){double physical[3],out[3];for(int c=0;c<3;c++)physical[c]=double(xyz[i+c])-1e-10;device.xyzRGB.apply(physical,out);for(int c=0;c<3;c++)rgb[i+c]=float(out[c]);}
  return rgb;
 }
};
