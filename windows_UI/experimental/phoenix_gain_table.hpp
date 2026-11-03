#pragma once
#include <string>
#include <cstdlib>
#include <cstring>
namespace spk::phoenix_preview {
inline void apply_fixed_gain(const std::string& id, double* gain) {
 const char* flag=std::getenv("SPEKTRALAB_PHOENIX_FIXED_GAIN"); if(!flag||std::strcmp(flag,"1")!=0)return;
 if(id=="harman_phoenix_200_exp_v1"){gain[0]=0.40848703560034955;gain[1]=0.40848703560034955;gain[2]=0.40848703560034955;return;}
 if(id=="harman_phoenix_ii_200_exp_v1"){gain[0]=0.32690421903309935;gain[1]=0.32690421903309935;gain[2]=0.32690421903309935;return;}
 if(id=="phx_200_d55_r2"){gain[0]=0.40848703560034955;gain[1]=0.40848703560034955;gain[2]=0.40848703560034955;return;}
 if(id=="phx_200_linear_r2"){gain[0]=0.40848703560034955;gain[1]=0.40848703560034955;gain[2]=0.40848703560034955;return;}
 if(id=="phx_200_no_tungsten_r2"){gain[0]=0.40848703560034955;gain[1]=0.40848703560034955;gain[2]=0.40848703560034955;return;}
 if(id=="phx_200_shared_r2"){gain[0]=0.40848703560034955;gain[1]=0.40848703560034955;gain[2]=0.40848703560034955;return;}
 if(id=="phx_ii_d55_r2"){gain[0]=0.32690421903309935;gain[1]=0.32690421903309935;gain[2]=0.32690421903309935;return;}
 if(id=="phx_ii_linear_r2"){gain[0]=0.32690421903309935;gain[1]=0.32690421903309935;gain[2]=0.32690421903309935;return;}
 if(id=="phx_ii_shared_r2"){gain[0]=0.32690421903309935;gain[1]=0.32690421903309935;gain[2]=0.32690421903309935;return;}
 if(id=="phx_ii_width115_r2"){gain[0]=0.32690421903309935;gain[1]=0.32690421903309935;gain[2]=0.32690421903309935;return;}
 if(id=="phx_ii_width130_r2"){gain[0]=0.32690421903309935;gain[1]=0.32690421903309935;gain[2]=0.32690421903309935;return;}
 if(id=="phx_200_base_amp010_r3"){gain[0]=0.40848703560034955;gain[1]=0.40848703560034955;gain[2]=0.40848703560034955;return;}
 if(id=="phx_200_base_center560_r3"){gain[0]=0.40848703560034955;gain[1]=0.40848703560034955;gain[2]=0.40848703560034955;return;}
 if(id=="phx_200_base_neutral_r3"){gain[0]=0.40848703560034955;gain[1]=0.40848703560034955;gain[2]=0.40848703560034955;return;}
 if(id=="phx_200_base_sigma075_r3"){gain[0]=0.40848703560034955;gain[1]=0.40848703560034955;gain[2]=0.40848703560034955;return;}
 if(id=="phx_200_dye_nonnegative_r3"){gain[0]=0.40848703560034955;gain[1]=0.40848703560034955;gain[2]=0.40848703560034955;return;}
 if(id=="phx_200_dye_portra_proxy_r3"){gain[0]=0.40848703560034955;gain[1]=0.40848703560034955;gain[2]=0.40848703560034955;return;}
 if(id=="phx_ii_200_base_amp010_r3"){gain[0]=0.32690421903309935;gain[1]=0.32690421903309935;gain[2]=0.32690421903309935;return;}
 if(id=="phx_ii_200_base_center560_r3"){gain[0]=0.32690421903309935;gain[1]=0.32690421903309935;gain[2]=0.32690421903309935;return;}
 if(id=="phx_ii_200_base_neutral_r3"){gain[0]=0.32690421903309935;gain[1]=0.32690421903309935;gain[2]=0.32690421903309935;return;}
 if(id=="phx_ii_200_base_sigma075_r3"){gain[0]=0.32690421903309935;gain[1]=0.32690421903309935;gain[2]=0.32690421903309935;return;}
 if(id=="phx_ii_200_dye_nonnegative_r3"){gain[0]=0.32690421903309935;gain[1]=0.32690421903309935;gain[2]=0.32690421903309935;return;}
 if(id=="phx_ii_200_dye_portra_proxy_r3"){gain[0]=0.32690421903309935;gain[1]=0.32690421903309935;gain[2]=0.32690421903309935;return;}
}
inline constexpr const char* manifest_sha256="0ce386317a2cf7ece8f221cf397b4cc3bece3a63a6634abc9bf4eda7fd9645ab";
inline bool is_phoenix(const std::string& id){return id=="harman_phoenix_200_exp_v1"||id=="harman_phoenix_ii_200_exp_v1"||id=="phx_200_d55_r2"||id=="phx_200_linear_r2"||id=="phx_200_no_tungsten_r2"||id=="phx_200_shared_r2"||id=="phx_ii_d55_r2"||id=="phx_ii_linear_r2"||id=="phx_ii_shared_r2"||id=="phx_ii_width115_r2"||id=="phx_ii_width130_r2"||id=="phx_200_base_amp010_r3"||id=="phx_200_base_center560_r3"||id=="phx_200_base_neutral_r3"||id=="phx_200_base_sigma075_r3"||id=="phx_200_dye_nonnegative_r3"||id=="phx_200_dye_portra_proxy_r3"||id=="phx_ii_200_base_amp010_r3"||id=="phx_ii_200_base_center560_r3"||id=="phx_ii_200_base_neutral_r3"||id=="phx_ii_200_base_sigma075_r3"||id=="phx_ii_200_dye_nonnegative_r3"||id=="phx_ii_200_dye_portra_proxy_r3";}
}
