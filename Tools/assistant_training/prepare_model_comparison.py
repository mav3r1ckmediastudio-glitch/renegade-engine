"""Download a pinned official model snapshot for an isolated local comparison."""
import argparse,json,os,shutil
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--lab',type=Path,required=True);p.add_argument('--repo',default='Qwen/Qwen3-4B-Instruct-2507');p.add_argument('--manifest',type=Path,required=True);a=p.parse_args();os.environ['HF_HOME']=str(a.lab/'hf-cache')
from huggingface_hub import HfApi,snapshot_download
info=HfApi().model_info(a.repo,files_metadata=True)
size=sum(s.size or 0 for s in info.siblings if s.rfilename.endswith(('.safetensors','.json','.txt')))
if shutil.disk_usage(a.lab).free<size+2*1024**3:raise RuntimeError('Insufficient space for isolated snapshot')
print('SNAPSHOT',a.repo,info.sha,'bytes',size,flush=True)
folder=snapshot_download(a.repo,revision=info.sha,allow_patterns=['*.safetensors','*.json','*.txt'])
a.manifest.write_text(json.dumps({'model':a.repo,'revision':info.sha,'snapshot':folder,'download_bytes':size,'live_deploy':False},indent=2),encoding='utf-8')
print('DOWNLOAD_COMPLETE',folder,flush=True)
