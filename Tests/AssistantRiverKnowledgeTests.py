"""Source-and-runtime contract for River/Waterfall knowledge in Living Core.

This test deliberately exercises the same knowledge bridge used by packaged
Living Core, not just presence of Markdown in a directory.
"""
import argparse
import importlib.util
import json
import sqlite3
import sys
from pathlib import Path

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--repo',type=Path,required=True)
    parser.add_argument('--db',type=Path,required=True)
    args=parser.parse_args()
    repo=args.repo.resolve()
    sys.path.insert(0,str(repo/'Tools'/'assistant_knowledge'))
    import knowledge
    import bridge_server
    args.db.parent.mkdir(parents=True,exist_ok=True)
    info=knowledge.build(repo,args.db,repo/'Tools'/'assistant_knowledge'/'facts.json',
                         repo/'Tools'/'assistant_knowledge'/'legacy_guides.json')
    assert info['verified_facts']>=4,info
    with sqlite3.connect(args.db) as db:
        present=dict(db.execute('SELECT id,authority FROM facts WHERE id IN (?,?,?,?)',(
            'river-v2-current-controls','river-v2-terrain-width','waterfall-current-controls','river-v2-draw-and-link')))
    assert present=={
        'river-v2-current-controls':'source_verified',
        'river-v2-terrain-width':'source_verified',
        'waterfall-current-controls':'source_verified',
        'river-v2-draw-and-link':'source_verified',
    },present
    cases={
        'Tell me about rivers in Renegade':'editable spline rivers',
        'What can I adjust on a river?':'node-by-node width',
        'Why does my river have gaps in the terrain?':'reported pre-alpha defect',
        'What are waterfalls in Renegade?':'ADD WATERFALL',
        'How do I make a river?':'LINK / UPDATE BED',
        'How do tributaries work?':'tributary',
    }
    for question,needle in cases.items():
        result=bridge_server.process_chat(
            {'messages':[{'role':'user','content':question}]},repo,args.db,qwen_timeout=1)
        assert result['route']=='local_verified_knowledge',(question,result)
        assert needle.casefold() in result['answer'].casefold(),(question,result)
        assert "I don't have a verified Renegade answer" not in result['answer']
    print(f"PASS: {len(cases)} real assistant routes, 4 source-verified river/waterfall facts")

if __name__=='__main__':
    main()
