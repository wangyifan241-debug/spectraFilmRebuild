'use strict';
const fs=require('fs'),path=require('path'),crypto=require('crypto');
exports.vulkanHeaders=function(root=process.env.SPEKTRALAB_VULKAN_HEADERS||'C:/Users/Strickland/SpektraLab-deps-20261028/Vulkan-Headers/include'){
 const p=path.join(root,'vulkan/vulkan_core.h'),bytes=fs.readFileSync(p);
 const macros=bytes.toString().split(/\r?\n/).filter(x=>/^#define VK_HEADER_VERSION(?:_COMPLETE)?\s/.test(x));
 if(macros.length!==2)throw Error('Cannot identify Vulkan header version');
 return {git_commit:null,reason:'identity bound to installed core header bytes; Git ancestry not required',version_macros:macros,core_header_sha256:crypto.createHash('sha256').update(bytes).digest('hex')};
};
