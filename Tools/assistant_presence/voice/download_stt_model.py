"""Explicit developer setup only. Runtime never downloads models."""
import argparse
from pathlib import Path
from huggingface_hub import snapshot_download

if __name__=="__main__":
    parser=argparse.ArgumentParser()
    parser.add_argument("--destination",type=Path,default=Path(__file__).resolve().parents[3]/"BUILD"/"assistant-stt-model")
    args=parser.parse_args()
    folder=snapshot_download("Systran/faster-whisper-base.en",revision="3d3d5dee26484f91867d81cb899cfcf72b96be6c",local_dir=str(args.destination),allow_patterns=["config.json","model.bin","tokenizer.json","vocabulary.*"])
    print("Local English speech model prepared:",folder)
