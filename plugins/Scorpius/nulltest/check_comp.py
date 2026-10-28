"""Where the compression nulls go: the reference's wet output on nullmod's signals against (1) our gain + the learned
colour run offline in PyTorch, (2) the plug-in (ScRender), (3) the plug-in's output against the offline model.
    python check_comp.py [a|b]"""
import os, sys
import numpy as np
import torch
import orion as o
import nullmod as N
from prep_comp import gain, TUNE
from train_wh import NN

m = sys.argv[1] if len(sys.argv) > 1 else 'b'
case = 'comp ' + m
sig = N.signals()
jobs = [dict(x=x, **N.CASES[case]) for x in sig.values()]
ref = o.render_many(jobs, cache=True)
ours = o.render_many(jobs, plugin='model')
import train_wn as T
d = torch.load(os.path.join(NN, f'comp_{m}_u_wn.pt'))
h0 = np.zeros(8192, np.float32); h0[0] = 1
net = T.Model(h0, d['ch'], d['layers']); net.load_state_dict(d['state']); net.eval()
for (k, x), r, ou in zip(sig.items(), ref, ours):
    x2 = np.atleast_2d(x).astype(np.float64)
    if x2.shape[0] == 1: x2 = np.vstack([x2, x2])
    g = gain(0.5 * (x2[0] + x2[1]), **TUNE[m])
    with torch.no_grad():
        y = np.stack([net(torch.tensor((c * g).astype(np.float32)[None]))[0].numpy() for c in x2])
    print(f'{k:10s} offline {o.null_db(r, y):6.1f}   plug-in {o.null_db(r, ou):6.1f}   plug-in vs offline {o.null_db(y, ou):6.1f}', flush=True)
