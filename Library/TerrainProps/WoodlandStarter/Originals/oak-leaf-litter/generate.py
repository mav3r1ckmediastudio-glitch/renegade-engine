"""Requires Python, NumPy and Pillow."""
from pathlib import Path
import argparse, json, math, struct
import numpy as np
from PIL import Image
def encode(doc,data):
    doc['buffers']=[{'byteLength':len(data)}]
    js=json.dumps(doc,separators=(',',':')).encode();js+=b' '*(-len(js)%4)
    raw=bytes(data)+b'\0'*(-len(data)%4)
    return struct.pack('<4sII',b'glTF',2,28+len(js)+len(raw))+struct.pack('<I4s',len(js),b'JSON')+js+struct.pack('<I4s',len(raw),b'BIN\0')+raw
def decode(blob):
    n=struct.unpack_from('<I',blob,12)[0]
    return json.loads(blob[20:20+n]),blob[28+n:]
class Model:
    def __init__(self):
        self.d={'asset':{'version':'2.0','generator':'Renegade woodland starter authoring'},'scene':0,'scenes':[{'nodes':[]}],'nodes':[],'meshes':[],'materials':[],'images':[],'textures':[],'samplers':[{'wrapS':10497,'wrapT':10497,'minFilter':9987,'magFilter':9729}],'accessors':[],'bufferViews':[]}
        self.buf=bytearray()
    def view(self,b,target=None):
        self.buf.extend(b'\0'*(-len(self.buf)%4));v={'buffer':0,'byteOffset':len(self.buf),'byteLength':len(b)}
        if target:v['target']=target
        self.buf.extend(b);self.d['bufferViews'].append(v);return len(self.d['bufferViews'])-1
    def tex(self,p):
        data=p.read_bytes();i=len(self.d['images'])
        self.d['images'].append({'name':p.stem,'bufferView':self.view(data),'mimeType':'image/png' if data.startswith(b'\x89PNG') else 'image/jpeg'})
        self.d['textures'].append({'source':i,'sampler':0});return {'index':i}
    def accessor(self,arr,extent=False):
        floating=arr.dtype.kind=='f';arr=np.ascontiguousarray(arr,dtype='<f4' if floating else '<u4');a={'bufferView':self.view(arr.tobytes(),34962 if floating else 34963),'componentType':5126 if floating else 5125,'count':len(arr),'type':'SCALAR' if arr.ndim==1 else 'VEC'+str(arr.shape[1])}
        if extent:a.update(min=arr.min(axis=0).tolist(),max=arr.max(axis=0).tolist())
        self.d['accessors'].append(a);return len(self.d['accessors'])-1
    def mesh(self,name,attrs,indices,material):
        p={'mode':4,'material':material,'indices':self.accessor(indices),'attributes':{k:self.accessor(v,k=='POSITION') for k,v in attrs.items()}}
        i=len(self.d['meshes']);self.d['meshes'].append({'name':name,'primitives':[p]})
        self.d['scenes'][0]['nodes'].append(len(self.d['nodes']));self.d['nodes'].append({'name':name,'mesh':i})
    def write(self):return encode(self.d,self.buf)
def leaf_litter():
    name='OakLeafLitter';rng=np.random.default_rng(44386);texture_dir=destination;texture_dir.mkdir(parents=True,exist_ok=True)
    model=Model();y,x=np.indices((256,160));t=y/255;cx=79.5+6*np.sin(4*t)
    for layer,palette in enumerate(((137,84,38),(156,107,50),(105,72,40))):
        # Lobed oak outline with a hand-defined profile, a rib, and lateral veins.
        widths=np.interp(t,np.linspace(0,1,22),[0,2,4,8,13,10,23,16,34,23,45,26,48,31,45,30,37,24,29,15,15,0])
        dx=x-cx;alpha=np.uint8(np.clip((widths-np.abs(dx))*3.1,0,1)*255)
        noise=rng.normal(0,4,(256,160))+5*np.sin(x*.115+y*.046)+3*np.cos(x*.041-y*.069)
        vein=10*np.exp(-np.abs(dx)/1.5)
        for stem in (29,52,80,107,134,159,185):
            vein+=6.5*np.exp(-np.abs(y-stem-.79*np.abs(dx))/1.1)
        shade=noise+vein+12*(1-np.abs(dx)/53)-11*t
        rgb=np.clip(np.array(palette)[None,None,:]+shade[:,:,None],0,255).astype('uint8')
        image=Image.fromarray(np.dstack([rgb,alpha]));texpath=texture_dir/f'fallen_oak_{layer}.png';image.save(texpath)
        model.d['materials'].append({'name':f'Dry oak {layer+1}','doubleSided':True,'alphaMode':'MASK','alphaCutoff':.48,'pbrMetallicRoughness':{'baseColorTexture':model.tex(texpath),'metallicFactor':0,'roughnessFactor':.94}})
    for layer in range(3):
        positions=[];uv=[];idx=[]
        for j in range(18):
            a=float(rng.uniform(-math.pi,math.pi));dist=.25*math.sqrt(float(rng.random()));x0=dist*math.cos(a);z0=.7*dist*math.sin(a)
            heading=float(rng.uniform(-math.pi,math.pi));length=float(rng.uniform(.074,.133));width=length*.67
            roll=float(rng.uniform(-.007,.007));curve=float(rng.uniform(.0015,.006));lift=float(rng.uniform(.001,.011));start=len(positions)
            for v in range(6):
                for u in range(3):
                    xx=(u/2-.5)*width;zz=(v/5-.5)*length;yy=lift+abs(u-1)*curve+roll*(v/5-.5)+.003*(v/5)**2
                    positions.append([x0+xx*math.cos(heading)-zz*math.sin(heading),yy,z0+xx*math.sin(heading)+zz*math.cos(heading)])
                    uv.append([u/2,v/5])
            for v in range(5):
                for u in range(2):
                    k=start+3*v+u;idx.extend([k,k+3,k+1,k+1,k+3,k+4])
        p=np.asarray(positions); ii=np.array(idx).reshape(-1,3);normal=np.zeros_like(p)
        face=np.cross(p[ii[:,1]]-p[ii[:,0]],p[ii[:,2]]-p[ii[:,0]])
        for k in range(3):np.add.at(normal,ii[:,k],face)
        normal/=np.maximum(np.linalg.norm(normal,axis=1)[:,None],1e-12)
        model.mesh('Curled fallen leaves '+str(layer+1),{'POSITION':p,'NORMAL':normal,'TEXCOORD_0':np.array(uv)},ii.flatten(),layer)

    positions=[]
    for mesh in model.d['meshes']:
        for primitive in mesh['primitives']:
            a=model.d['accessors'][primitive['attributes']['POSITION']]
            positions.extend([a['min'],a['max']])
    points=np.array(positions);low=points.min(0);high=points.max(0)
    dx,dy,dz=-(low+high)/2;dy=-low[1]
    scene=model.d['scenes'][0];root=len(model.d['nodes'])
    model.d['nodes'].append({'name':name+' grounded pivot','translation':[float(dx),float(dy),float(dz)],'children':scene['nodes']})
    scene['nodes']=[root]
    (destination/(name+'.glb')).write_bytes(model.write())

if __name__ == '__main__':
    parser=argparse.ArgumentParser(description='Build the original 54-leaf Renegade scatter and its three cutout textures.')
    parser.add_argument('output',type=Path,help='Output directory')
    destination=parser.parse_args().output
    destination.mkdir(parents=True,exist_ok=True)
    leaf_litter()
