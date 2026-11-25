"""Deterministic synthetic baselines. No external audio or pretrained dependencies."""
from pathlib import Path
import argparse
import json
import numpy as np

parser = argparse.ArgumentParser(description=__doc__)
for model in ("room", "instrument", "scene"):
    parser.add_argument("--" + model, type=Path, help="NPZ with x, y and group arrays")
parser.add_argument("--output", type=Path, default=Path(__file__).resolve().parent.parent)
args = parser.parse_args()

rng = np.random.default_rng(20261125)

def use_data(name, shape, synthetic):
    path = getattr(args, name)
    if path is None:
        return synthetic()
    with np.load(path, allow_pickle=False) as data:
        x, y, group = data["x"], data["y"], data["group"]
    if x.ndim != 2 or y.ndim != 2 or (x.shape[1], y.shape[1]) != shape:
        parser.error(f"{name}: expected {shape[0]} input and {shape[1]} target columns")
    if len(x) != len(y) or group.shape != (len(x),):
        parser.error(f"{name}: group must identify each row")
    if not all(np.isfinite(a).all() and (a >= 0).all() and (a <= 1).all() for a in (x,y)):
        parser.error(f"{name}: expected finite normalized values in [0, 1]")
    if name == "instrument" and not np.allclose(y.sum(axis=1), 1):
        parser.error("instrument: target rows must sum to one")
    groups = rng.permutation(np.unique(group))
    if len(groups) < 5:
        parser.error(f"{name}: at least five independent groups are required")
    test = np.isin(group, groups[:max(1, len(groups)//5)])
    return x[~test], y[~test], x[test], y[test]
sig = lambda x: 1 / (1 + np.exp(-np.clip(x, -30, 30)))

def fit(name, x, y, xt, yt, width, softmax=False):
    w = rng.normal(0, .35, (x.shape[1], width))
    b = np.zeros(width)
    v = rng.normal(0, .25, (width, y.shape[1]))
    c = np.zeros(y.shape[1])
    def predict(inputs):
        h = np.maximum(inputs @ w + b, 0)
        z = h @ v + c
        if softmax:
            q = np.exp(z - z.max(axis=1, keepdims=True))
            return h, q / q.sum(axis=1, keepdims=True)
        return h, sig(z)
    for _ in range(1800):
        i = rng.integers(len(x), size=192)
        a, t = x[i], y[i]
        h, o = predict(a)
        d = (o - t) / len(i)
        if not softmax:
            d *= o * (1 - o)
        e = (d @ v.T) * (h > 0)
        for param, grad in zip((w, b, v, c), (a.T @ e, e.sum(0), h.T @ d, d.sum(0))):
            param -= .32 * grad
    _, p = predict(xt)
    metrics[name] = {'test_examples':len(xt), 'mae':np.abs(yt-p).mean(axis=0).round(5).tolist(),
                     'constant_baseline_mae':np.abs(yt-y.mean(axis=0)).mean(axis=0).round(5).tolist()}
    if softmax:
        metrics[name]['accuracy'] = float(np.mean(yt.argmax(1)==p.argmax(1)))
    def array(a):
        return '{ ' + ', '.join(f'{x:.8e}f' for x in a.flat) + ' }'
    models.append(f'inline constexpr Dense<{x.shape[1]}, {width}, {y.shape[1]}> {name}{{\n    {array(w)},\n    {array(b)},\n    {array(v)},\n    {array(c)}\n}};')

metrics, models = {}, []
def rooms(n):
    # Each row is a new shoebox/material + source. Test uses independent rooms.
    x = rng.random((n, 6))
    s, absorption, diffusion, px, py, depth = x.T
    length, width, height = 3+14*s, 2.6+9*s, 2.4+4*s
    area = 2*(length*width+length*height+width*height)
    alpha = .10 + .70*absorption
    rt60 = .161*length*width*height / (-area*np.log(1-alpha))
    source = np.stack([length*(.12+.76*px), width*(.12+.76*depth), height*(.24+.52*py)], 1)
    listener = np.stack([length*.5, width*.18, height*.43], 1)
    direct = np.linalg.norm(source-listener,axis=1)
    taps = []
    for axis, edge in ((0,0),(0,length),(1,0),(1,width)):
        image = source.copy()
        image[:,axis] = 2*edge-source[:,axis]
        distance = np.linalg.norm(image-listener,axis=1)
        delay = (distance-direct)/343
        gain = (1-alpha)**.5/(1+distance*.32)
        taps.extend((delay/.065,gain))
    late = .07+.5*rt60/(.8+rt60)+.18*diffusion
    return x, np.clip(np.column_stack((rt60/4, late, *taps)), .005, .995)
fit('room', *use_data('room', (6, 10), lambda: (*rooms(20000), *rooms(2500))), 32)

def instruments(n):
    # Random frequency/amplitude/envelope variations on stereo synthetic stems.
    t = np.arange(512)[None,:] / 12000
    labels = rng.integers(6, size=n)
    f = np.array([65,190,2700,95,290,520])[labels,None] * rng.uniform(.65,1.45,(n,1))
    phase = 2*np.pi*f*t
    noise = rng.normal(0,1,(n,len(t[0])))
    env = np.exp(-t * np.array([68,105,142,8,16,25])[labels,None])
    tone = np.sin(phase) + np.array([.08,.25,.2,.38,.61,.97])[labels,None]*np.sin(2*phase+.2)
    m = np.array([.02,.72,.96,.04,.08,.13])[labels,None]
    audio = rng.uniform(.14,.63,(n,1))*env*((1-m)*tone + m*noise)
    side = rng.uniform(0,.65,(n,1))*audio*rng.choice([-1,1],(n,1))
    l, r = audio+side, audio-side
    power = np.mean(audio**2,axis=1)
    peak = np.max(np.abs(audio),axis=1)
    delta = np.diff(audio,prepend=0,axis=1)
    x = np.stack([np.minimum(1,np.sqrt(power)*4),
                  np.minimum(1,np.sqrt(np.mean(delta**2,axis=1)/(power+1e-8))*.5),
                  np.minimum(1,peak/(np.sqrt(power)+1e-6)*.13),
                  np.mean((audio[:,1:]>0)!=(audio[:,:-1]>0),axis=1),
                  np.sqrt(np.mean((l-r)**2,axis=1)/(4*power+1e-8))],1)
    return np.clip(x,0,1), np.eye(6)[labels]
fit('instrument', *use_data('instrument', (5, 6), lambda: (*instruments(14000), *instruments(2200))), 24, True)

def scenes(n):
    rows, targets = [], []
    for _ in range(n):
        k = rng.integers(3,13)
        roles = rng.integers(6,size=k)
        power = rng.uniform(.05,.9,k)
        width = rng.uniform(.02,.9,k)
        seed = rng.random(k)
        context = np.r_[np.eye(6)[roles].mean(0), power.mean(), width.mean()]
        for i, role in enumerate(roles):
            # Expert-inspired synthetic teacher. Balance, role density and signal features vary by scene.
            companions = (roles==role).sum()-1
            span = np.array([.035,.2,.75,.05,.23,.66])[role]
            pan = (2*seed[i]-1)*span*(.8+.13*min(3,companions))*(1-.35*context[-1])
            depth = np.clip(.09+np.array([0,.18,.43,.07,.12,.5])[role]+.18*(1-power[i])+.19*context[5],0,1)
            room = np.clip(.26+.52*depth+.09*width[i],0,1)
            rows.append(np.r_[np.eye(6)[role],power[i],width[i],seed[i],context])
            targets.append([.5+.5*pan,.43+.13*(role==2)-.13*(role==3),depth,room])
    return np.array(rows),np.array(targets)
fit('scene', *use_data('scene', (17, 4), lambda: (*scenes(5000), *scenes(620))), 28)
root=args.output
(root/'Source/Neural').mkdir(parents=True, exist_ok=True)
(root/'Training').mkdir(parents=True, exist_ok=True)
(root/'Source/Neural/Weights.h').write_text('#pragma once\n#include "Dense.h"\n\nnamespace panner::neural {\n'+'\n\n'.join(models)+'\n}\n')
(root/'Training/metrics.json').write_text(json.dumps(metrics,indent=2)+'\n')
print(json.dumps(metrics,indent=2))
