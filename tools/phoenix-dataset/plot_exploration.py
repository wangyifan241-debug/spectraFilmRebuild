"""Author-balanced exploratory distributions, never a causal film comparison."""
import json
from pathlib import Path
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
base=Path('research/phoenix-reference-dataset');data=json.loads((base/'exploratory_statistics.json').read_text())
fig,axes=plt.subplots(2,4,figsize=(15,7))
for stock,color in [('Phoenix 200','tab:red'),('Phoenix II 200','tab:blue')]:
    selected=[r for r in data if r['film_stock']==stock];authors=sorted({r['photographer'] for r in selected})
    def balanced(key,channel=None):
        groups=[]
        for author in authors:
            vectors=[]
            for r in selected:
                if r['photographer']!=author:continue
                vector=np.asarray(r[key][channel] if channel is not None else r[key],dtype=float)
                if 'histogram' in key:vector/=max(1,vector.sum())
                vectors.append(vector)
            groups.append(np.mean(vectors,axis=0))
        return np.mean(groups,axis=0)
    label=f'{stock}, n={len(selected)}, authors={len(authors)}'
    for c in range(3):
        for row,key,ranges in [(0,'Lab_channel_histograms',[(0,100),(-128,128),(-128,128)]),(1,'OKLab_channel_histograms',[(0,1),(-.5,.5),(-.5,.5)])]:
            lo,hi=ranges[c];axes[row,c].plot(np.linspace(lo,hi,40,endpoint=False)+(hi-lo)/80,balanced(key,c),color=color,label=label)
    axes[0,3].plot(np.linspace(.0125,.9875,40),balanced('Y_histogram'),color=color,label=label)
    axes[1,3].plot([1,5,25,50,75,95,99],balanced('linear_Y_percentiles'),color=color,label=label)
for row in range(2):
    for c in range(3):axes[row,c].set_title(('Lab D65 ' if row==0 else 'OKLab ')+['L','a','b'][c]);axes[row,c].set_ylabel('author-balanced frequency')
axes[0,3].set_title('linear sRGB Y distribution');axes[1,3].set_title('Y percentiles (author-balanced)')
for axis in axes.flat:axis.grid(alpha=.3)
handles,labels=axes[0,0].get_legend_handles_labels();fig.legend(handles,labels,loc='lower center',ncol=2)
fig.suptitle('Exploratory web scans: scene, scanner and author confounds remain; NOT intrinsic film response')
fig.tight_layout(rect=(0,.07,1,.94));fig.savefig(base/'exploratory-distributions.png',dpi=150)
