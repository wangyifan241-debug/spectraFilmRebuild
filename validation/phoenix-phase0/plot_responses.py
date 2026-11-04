from pathlib import Path
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
root=Path('build/phoenix-phase0-ablation-synth');out=Path('validation/phoenix-phase0/curves');out.mkdir(exist_ok=True)
ev=np.linspace(-6,6,256)
for stock in ['harman_phoenix_200_exp_v1','harman_phoenix_ii_200_exp_v1']:
 fig,axes=plt.subplots(3,3,figsize=(15,11),sharex=True)
 stages=['film_log_and_curves','film_couplers','scan-logT','scan-logPositive','scan-linear-prophoto','affine-logPositive','B-affine-matrix','C-inverse-identity','D-affine-identity']
 for ax,stage in zip(axes.flat,stages):
  v=np.fromfile(root/(stock+'-'+stage+'.f32'),dtype='<f4').reshape(96,256,3)
  for c,col in enumerate('rgb'):ax.plot(ev,v[0,:,c],color=col,label=col.upper())
  ax.set_title(stage);ax.set_xlabel('input EV relative to 0.184');ax.grid();ax.legend()
 fig.suptitle(stock+' neutral ramp: density/log/linear units differ by stage');fig.tight_layout();fig.savefig(out/(stock+'-neutral.png'));plt.close(fig)
 fig,axes=plt.subplots(3,4,figsize=(16,10),sharex=True)
 for row,c in enumerate(range(3)):
  for col,(label,stage) in enumerate([('A','scan-linear-prophoto'),('B','B-affine-matrix'),('C','C-inverse-identity'),('D','D-affine-identity')]):
   v=np.fromfile(root/(stock+'-'+stage+'.f32'),dtype='<f4').reshape(96,256,3)
   for j,color in enumerate('rgb'):axes[row,col].plot(ev,v[24*(c+1),:,j],color=color)
   axes[row,col].set_title(label+' input '+ 'RGB'[c]+' biased ramp');axes[row,col].set_ylim(-.2,2);axes[row,col].grid();axes[row,col].set_xlabel('input EV')
 fig.suptitle('Linear ProPhoto output; plot range limited, raw data retained');fig.tight_layout();fig.savefig(out/(stock+'-channels.png'));plt.close(fig)
print('Generated four figures from existing float32 data; no rerender')
