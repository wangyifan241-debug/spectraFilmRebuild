import json
from pathlib import Path
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
base=Path('research/phoenix-reference-dataset')
data=json.loads((base/'exploratory_statistics.json').read_text())
fig,axes=plt.subplots(1,2,figsize=(11,4))
for row in data:
    axes[0].plot([1,5,25,50,75,95,99],row['linear_Y_percentiles'],label=row['id'])
    axes[1].plot([2.5+5*i for i in range(20)],row['Lab_L_histogram'],label=row['id'])
axes[0].set(xlabel='Percentile',ylabel='Linear sRGB Y',title='Different scenes: not film response')
axes[1].set(xlabel='Lab D65 L*',ylabel='Sample count',title='Per-image distributions, n=1 per stock')
for axis in axes:
    axis.legend()
    axis.grid()
fig.tight_layout()
fig.savefig(base/'exploratory-distributions.png')
