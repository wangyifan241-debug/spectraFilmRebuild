import json
from pathlib import Path
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
root=Path('research/phoenix-phase2');d=json.loads((root/'comparison.json').read_text())
names=['Lab_L','Lab_a','Lab_b','Y_median','Y_p95_minus_p05']
fig,axes=plt.subplots(1,2,figsize=(12,5),gridspec_kw={'width_ratios':[1,1.2]})
for i,key in enumerate(names[:3]):
    r=d['author_stability'][key];lo,hi=r['author_bootstrap_95_interval'];a,b=r['leave_one_author_out_range']
    axes[0].plot([lo,hi],[i,i],color='tab:blue',lw=5,alpha=.35)
    axes[0].plot([a,b],[i,i],color='black',lw=2)
    axes[0].scatter(r['II_minus_I_author_balanced'],i,color='tab:red')
axes[0].axvline(0,color='gray',ls='--');axes[0].set_yticks(range(3),names[:3]);axes[0].set_title('II minus I: author-balanced Lab means');axes[0].set_xlabel('Blue: descriptive bootstrap 95%; black: leave-one-out')
groups=d['scene_groups'];keys=list(groups);counts=np.array([[groups[k][s]['n'] for s in ['Phoenix 200','Phoenix II 200']] for k in keys])
axes[1].imshow(counts,cmap='Blues',vmin=0);axes[1].set_xticks([0,1],['Phoenix 200','Phoenix II']);axes[1].set_yticks(range(len(keys)),keys)
for i,k in enumerate(keys):
    for j,s in enumerate(['Phoenix 200','Phoenix II 200']):axes[1].text(j,i,f"n={counts[i,j]} / authors={groups[k][s]['authors']}",ha='center',va='center')
axes[1].set_title('Scene coverage: no red human-portrait stratum')
fig.suptitle('Small unpaired web-scan collection; source/scanner/content confounds remain');fig.tight_layout();fig.savefig(root/'stability-scenes.png',dpi=150)
