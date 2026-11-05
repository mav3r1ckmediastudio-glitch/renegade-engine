"""Git line-ending conversion must not revoke a guide; other source edits must."""
import hashlib
from contextlib import closing
import json
from pathlib import Path
import sqlite3
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'Tools/assistant_knowledge'))
import knowledge
import workflow_answers

class WorkflowLineEndingTests(unittest.TestCase):
    def test_gitless_workflow_preserves_line_endings_but_rejects_other_edits(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / 'source.cpp'
            source.write_bytes(b'control\r\noriginal behavior\r\n')
            manual = root / 'manual.md'
            manual.write_text('Curated test guide', encoding='utf-8')
            status = 'source_checked_native_service_tests_passed_ui_pending'
            (root / 'receipt.json').write_text(json.dumps({'id': 'toy-guide',
                'end_to_end_acceptance': {'status': status}}))
            database = root / 'knowledge.sqlite'
            commit = 'a' * 40
            with closing(sqlite3.connect(database)) as db:
                db.execute('CREATE TABLE metadata(key TEXT,value TEXT)')
                db.execute('INSERT INTO metadata VALUES(?,?)', ('commit', commit))
                db.execute('CREATE TABLE manual_guides(id TEXT,status TEXT,manual_path TEXT,manual_hash TEXT,evidence TEXT,issues TEXT)')
                db.execute('INSERT INTO manual_guides VALUES(?,?,?,?,?,?)',
                    ('toy-guide', 'draft_source_matched', 'manual.md', knowledge.sha(manual), '[]', '[]'))
                db.commit()
            (root / knowledge.PACKAGED_SOURCE_IDENTITY).write_text(json.dumps({
                'schema': 1, 'kind': 'renegade_gitless_package', 'commit': commit,
                'database_sha256': knowledge.sha(database)}))
            registry = root / 'workflows.json'
            registry.write_text(json.dumps({'schema': 1, 'source_hash_format': 'utf8-lf-v1',
                'workflows': [{'id': 'toy', 'guide_id': 'toy-guide', 'label': 'test workflow',
                    'match': ['test workflow'], 'prerequisites': 'test prerequisite',
                    'steps': ['Perform test operation'], 'follow_up': 'test follow up',
                    'limit': 'test only', 'provenance': ['source.cpp'],
                    'receipt_path': 'receipt.json', 'acceptance_status': status,
                    'evidence': [{'path': 'source.cpp', 'needle': 'control',
                        'sha256': hashlib.sha256(b'control\noriginal behavior\n').hexdigest()}]}]}))
            before = workflow_answers.verified_workflow('test workflow', root, database, registry)
            self.assertIsNotNone(before)
            source.write_bytes(b'control\noriginal behavior\n')
            self.assertEqual(workflow_answers.verified_workflow('test workflow', root, database, registry), before)
            # The label survives; changed behavior elsewhere must still revoke the answer.
            source.write_bytes(b'control\nchanged behavior\n')
            self.assertIsNone(workflow_answers.verified_workflow('test workflow', root, database, registry))

if __name__ == '__main__':
    unittest.main()
