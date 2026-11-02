"""Run the complete offline assistant suite with a freshly indexed isolated DB."""
import argparse
import os
from pathlib import Path
import subprocess
import sys

parser = argparse.ArgumentParser()
parser.add_argument('--repo', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
repo = args.repo.resolve()
args.output.mkdir(parents=True, exist_ok=True)
database = args.output.resolve() / 'assistant-workflow-suite.sqlite'
subprocess.run([sys.executable, str(repo / 'Tools/assistant_knowledge/knowledge.py'),
    'build', '--repo', str(repo), '--db', str(database)], check=True, cwd=repo)
env = dict(os.environ, RENEGADE_KNOWLEDGE_TEST_DB=str(database))
result = subprocess.run([sys.executable, '-m', 'unittest', 'discover',
    '-s', str(repo / 'Tests/assistant_knowledge'), '-p', 'test_*.py', '-v'],
    cwd=repo, env=env)
raise SystemExit(result.returncode)
