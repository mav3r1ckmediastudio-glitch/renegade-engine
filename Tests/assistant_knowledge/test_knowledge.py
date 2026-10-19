"""RA-01 knowledge index: source trust and retrieval regression tests."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

SCRIPT = Path(__file__).resolve().parents[2] / "Tools" / "assistant_knowledge" / "knowledge.py"
FACTS = SCRIPT.parent / "facts.json"
spec = importlib.util.spec_from_file_location("renegade_knowledge", SCRIPT)
kb = importlib.util.module_from_spec(spec)
spec.loader.exec_module(kb)


class KnowledgeTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.repo = self.root / "repo"
        self.db = self.root / "kb.sqlite"
        definitions=self.root / "definitions"
        definitions.mkdir()
        self.fixture_facts=definitions / "facts.json"
        self.fixture_facts.write_text(FACTS.read_text(encoding="utf-8"),encoding="utf-8")
        (definitions / "projectile_guide.json").write_text(
            (SCRIPT.parent / "projectile_guide.json").read_text(encoding="utf-8"),
            encoding="utf-8")
        sources = {}
        for fact in json.loads(FACTS.read_text(encoding="utf-8"))["facts"]:
            for item in fact["sources"]:
                sources.setdefault(item["path"], []).append(item["needle"])
        for relative, needles in sources.items():
            target = self.repo / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text("\n".join(dict.fromkeys(needles)), encoding="utf-8")
        planned = self.repo / "docs" / "FUTURE_PLANS.md"
        planned.write_text("# FUTURE PLANS\nNext year we might add unicorn rayguns.\n",
                           encoding="utf-8")
        self.legacy = self.root / "old_guides.json"
        self.legacy.write_text(json.dumps({
            "revision": "historic-revision",
            "guides": [{"id": "unicorn", "title": "Unicorn rayguns",
                        "tags": ["unicorn", "raygun"], "sources": []}]
        }), encoding="utf-8")

    def build(self):
        return kb.build(self.repo, self.db, self.fixture_facts, self.legacy)

    def test_facts_are_verified_by_checked_out_source(self):
        status = self.build()
        self.assertEqual(status["verified_facts"], 12)
        result = kb.search(self.db, "How can I make glass impact sounds?", self.repo)
        self.assertEqual(result["verified_facts"][0]["id"], "surface-type")
        self.assertEqual(result["verified_facts"][0]["authority"], "source_verified")

    def test_proposed_features_remain_reference_only(self):
        self.build()
        result = kb.search(self.db, "Do unicorn rayguns exist?", self.repo)
        self.assertFalse(result["verified_facts"])
        self.assertEqual(result["historical_guides"][0]["authority"],
                         "historical_needs_revalidation")
        self.assertTrue(all(x["authority"] == "reference_only_not_feature_proof"
                            for x in result["references"]))

    def test_stale_code_evidence_revokes_fact(self):
        self.build()
        path = self.repo / "EngineBridge/include/renegade/bridge/ImpactSurface.h"
        path.write_text(path.read_text(encoding="utf-8") + "\n// source changed",
                        encoding="utf-8")
        result = kb.search(self.db, "glass surface type", self.repo, strict=True)
        self.assertFalse(any(f["id"] == "surface-type" for f in result["verified_facts"]))

    def test_missing_required_source_marks_fact_unverified(self):
        path = self.repo / "EngineBridge/include/renegade/bridge/ImpactSurface.h"
        path.write_text("enum class ImpactSurfaceType", encoding="utf-8")
        status = self.build()
        self.assertEqual(status["verified_facts"], 11)
        self.assertFalse(any(f["id"] == "surface-type" for f in
                             kb.search(self.db, "glass", self.repo)["verified_facts"]))

    def test_changed_document_reference_is_omitted(self):
        self.build()
        path = self.repo / "docs/FUTURE_PLANS.md"
        path.write_text("# Old plan removed\n", encoding="utf-8")
        results = kb.search(self.db, "unicorn rayguns", self.repo)
        self.assertFalse(any("FUTURE_PLANS.md" in r["path"]
                             for r in results["references"]))

    def test_strict_mode_only_returns_verified_facts(self):
        self.build()
        result = kb.search(self.db, "unicorn rayguns", self.repo, strict=True)
        self.assertFalse(result["verified_facts"])
        self.assertFalse(result["historical_guides"])
        self.assertFalse(result["references"])

    def test_generic_product_name_cannot_verify_unrelated_claim(self):
        self.build()
        result = kb.search(self.db, "Can Renegade create my entire world automatically?",
                           self.repo, strict=True)
        self.assertFalse(result["verified_facts"])

    def test_manual_is_indexed_but_not_approved(self):
        manual = self.repo / "docs/manual/PROJECTILES_FIRST_WEAPON.md"
        manual.parent.mkdir(parents=True, exist_ok=True)
        manual.write_text("# Create your first projectile weapon\\n", encoding="utf-8")
        guide = json.loads((SCRIPT.parent / "projectile_guide.json").read_text(encoding="utf-8"))
        for fact in guide["facts"]:
            for source in fact["sources"]:
                target = self.repo / source["path"]
                target.parent.mkdir(parents=True, exist_ok=True)
                with target.open("a", encoding="utf-8") as output:
                    output.write("\\n" + source["needle"] + "\\n")
        result = self.build()
        self.assertEqual(result["manual_guides"], 1)
        query = kb.search(self.db, "create first projectile weapon", self.repo)
        self.assertEqual(query["manual_guides"][0]["authority"], "draft_source_matched")
        self.assertFalse(query["manual_guides"][0]["publication_approved"])
        manual.write_text(manual.read_text(encoding="utf-8") + "changed", encoding="utf-8")
        stale = kb.search(self.db, "create first projectile weapon", self.repo)
        self.assertEqual(stale["manual_guides"][0]["authority"],
                         "draft_stale_rebuild_required")

    def test_unrelated_facts_are_not_retrieved(self):
        self.build()
        result = kb.search(self.db, "inspect my selected object", self.repo, strict=True)
        ids = {f["id"] for f in result["verified_facts"]}
        self.assertIn("selected-entity-access", ids)
        self.assertNotIn("surface-type", ids)
        self.assertNotIn("projectile-spawn", ids)


if __name__ == "__main__":
    unittest.main()
