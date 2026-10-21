#!/usr/bin/env python3
"""Plots the filter response chart in docs/review/phase-2a/ from the test harness CSVs.

Needs matplotlib. Usage:
    DICEVOX_RESPONSE_CSV=after.csv build/DiceVoxTests
    python3 tools/dsp/plot-filter-response.py before.csv after.csv docs/review/phase-2a/filter-resonance-response.png
("before" = a build without the per-stage Q limit; prints a markdown peak table too.)
"""
import csv, sys, collections
import matplotlib; matplotlib.use("Agg")
import matplotlib.pyplot as plt
def load(p):
    d=collections.defaultdict(lambda:([],[]))
    for r in csv.DictReader(open(p)):
        k=(float(r["sample_rate"]),float(r["resonance"]),int(float(r["low_cut"])),int(float(r["high_cut"])))
        d[k][0].append(float(r["freq_hz"])); d[k][1].append(float(r["gain_db"]))
    return d
b,a=load(sys.argv[1]),load(sys.argv[2])
limit=8.14
fig,ax=plt.subplots(1,2,figsize=(13,4.8),sharey=True)
cols=["#8F32C7","#2a7ab0","#c7682b","#3a9a3a"]
for c,(lc,hc) in zip(cols,[(1000,1000),(1000,1100),(1000,1400),(1000,2000)]):
    f,g=b[(48000.0,100.0,lc,hc)]; ax[0].semilogx(f,g,color=c,ls="--",lw=1.2,alpha=0.7)
    f,g=a[(48000.0,100.0,lc,hc)]; ax[0].semilogx(f,g,color=c,lw=2,label=f"low cut {lc} / high cut {hc} Hz")
ax[0].set_title("Close cutoffs, resonance 100 %: before (dashed) vs after (solid)")
for c,(lc,hc) in zip(cols,[(300,3400),(20,2000),(80,20000),(2000,200)]):
    f,g=a[(48000.0,100.0,lc,hc)]; ax[1].semilogx(f,g,color=c,lw=2,label=f"{lc} / {hc} Hz")
ax[1].set_title("Typical settings after the fix, resonance 100 %")
for x in ax:
    x.axhline(limit,color="k",lw=0.8,ls=":"); x.text(25,limit+0.5,"one-stage peak +8.14 dB",fontsize=8)
    x.set_xlim(20,20000); x.set_ylim(-40,18); x.grid(True,which="both",alpha=0.25); x.set_xlabel("Hz"); x.legend(fontsize=8,loc="lower center")
ax[0].set_ylabel("gain (dB), measured from impulse response, 48 kHz")
fig.suptitle("DiceVox Phase 2A filter: measured response (offline test harness)")
fig.tight_layout(); fig.savefig(sys.argv[3],dpi=110)
# table
print("| Low / High cut (Hz) | Before, 100 % | After, 100 % | Before, 50 % | After, 50 % | After, worst of 44.1/48/96 kHz at 100 % |")
print("|---|---|---|---|---|---|")
pairs=sorted({(k[2],k[3]) for k in a}, key=lambda p:(abs(__import__('math').log2(p[1]/p[0])),p))
for lc,hc in pairs:
    m=lambda d,sr,res: max(d[(sr,res,lc,hc)][1])
    worst=max(m(a,sr,100.0) for sr in (44100.0,48000.0,96000.0))
    print(f"| {lc} / {hc} | {m(b,48000.0,100.0):+.2f} dB | **{m(a,48000.0,100.0):+.2f} dB** | {m(b,48000.0,50.0):+.2f} dB | {m(a,48000.0,50.0):+.2f} dB | {worst:+.2f} dB |")
