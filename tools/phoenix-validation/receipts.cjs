'use strict';
const {binding,json,requireThat,notVerified,text}=require('./common.cjs');
const required=['spk_qt_phoenix','spk_phoenix_model_data','spk_phoenix_model_counterfactual','spk_qt_frame_output','spk_qt_material_workflow','spk_core_schema','spk_core_setup'];
function nativeReceipt(root,model){
 if(!model.engineering_receipt)return notVerified('Candidate needs a reviewed native engineering receipt; rendering alone is not acceptance');
 const b=binding(root,model.engineering_receipt);if(!b.ok)return notVerified(b.reason);
 try{const r=json(b.file);requireThat(r.format==='SpektraLab.PhoenixEngineeringReceipt'&&r.version===1,'Unknown engineering receipt');requireThat(text(r.reviewed_by)&&r.renderer_commit===model.renderer_commit&&r.recipe_sha256===model.recipe_sha256,'Receipt review/renderer/recipe missing');
  requireThat(model.materials.every(p=>r.material_sha256?.includes(p.sha256)),'Receipt did not test candidate materials');
  requireThat(Array.isArray(r.tests)&&required.every(name=>r.tests.some(t=>t.name===name&&t.exit_code===0&&t.status==='Pass'&&text(t.command)&&binding(root,t.log).ok)),'Missing native command/log/pass evidence');
  return {status:'Pass',scope:'Reviewed, hash-bound existing native test receipts; software only',receipt:model.engineering_receipt,reviewed_by:r.reviewed_by};
 }catch(e){return notVerified(e.message);}
}
module.exports={nativeReceipt,required};
