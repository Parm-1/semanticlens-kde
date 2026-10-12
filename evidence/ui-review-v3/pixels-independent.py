#!/usr/bin/env python3
"""Independent source-raster comparison. Fixed bilinear resize; no content-fit search.
Usage: pixels-independent.py RAW_REPORT SOURCE_RASTER CAPTURE OUTPUT_JSON
The white-paper edge is observed from capture alone, within one pixel of the
geometrically declared box. Source pixels define the ink witness, never the crop.
"""
from pathlib import Path
import sys,json,math
import numpy as np
from PIL import Image
report,source_path,capture_path,output_path=map(Path,sys.argv[1:])
data=json.loads(report.read_text());obs=[x for x in data['render_observations'] if x['event']=='capture-after-ready-scene-frame'][-1]
capture=Image.open(capture_path).convert('RGB');source=Image.open(source_path).convert('RGB')
def gray(im):
 a=np.array(im,dtype=np.int32);return (a[:,:,0]*11+a[:,:,1]*16+a[:,:,2]*5)//32
def qround(v):return math.floor(v+.5) if v>=0 else math.ceil(v-.5)
ww,wh=obs['window_width'],obs['window_height'];assert ww>0 and wh>0
sx,sy=capture.width/ww,capture.height/wh
box=[qround(obs['painted_scene_x']*sx),qround(obs['painted_scene_y']*sy),qround(obs['painted_width']*sx),qround(obs['painted_height']*sy)]
x,y,w,h=box;assert w>0 and h>0 and x>=0 and y>=0 and x+w<=capture.width and y+h<=capture.height
full=gray(capture);l,t,r,b=max(0,x-3),max(0,y-3),min(capture.width,x+w+3),min(capture.height,y+h+3)
yy,xx=np.where(full[t:b,l:r]>=250);edge=None;corrected=False
if len(xx):
 edge=[int(xx.min())+l,int(yy.min())+t,int(xx.max()-xx.min()+1),int(yy.max()-yy.min()+1)]
 if all(abs(a-b)<=1 for a,b in zip(edge,box)):x,y,w,h=edge;corrected=True
actual=full[y:y+h,x:x+w].astype(np.float64)
expected=gray(source.resize((w,h),Image.Resampling.BILINEAR)).astype(np.float64)
ink=expected<210;ys,xs=np.where(ink);assert len(xs)>0
xl,yt,xr,yb=max(0,int(xs.min())-3),max(0,int(ys.min())-3),min(w,int(xs.max())+4),min(h,int(ys.max())+4)
a=actual[yt:yb,xl:xr].ravel();s=expected[yt:yb,xl:xr].ravel();corr=float(np.corrcoef(s,a)[0,1]) if np.std(s)>0 and np.std(a)>0 else 0.0
mae=float(np.abs(actual-expected).mean());ink_count=int(ink.sum());passed=ink_count>=20 and mae<=10 and corr>=.65
result={'raw_report':str(report),'source_raster':str(source_path),'capture':str(capture_path),'declared_rect':box,'observed_nearby_white_bounds':edge,'paper_edge_correction':corrected,'evaluated_rect':[x,y,w,h],'source_ink_pixels':ink_count,'source_ink_witness':[xl,yt,xr-xl,yb-yt],'mean_gray_error':mae,'ink_correlation':corr,'pass':passed,'method':'Pillow bilinear source resize; NumPy independent gray/ink Pearson calculation; no offset/content/correlation search; inherited thresholds 20 ink, MAE<=10, corr>=.65; fixture-specific'}
output_path.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result));sys.exit(0 if passed else 1)
