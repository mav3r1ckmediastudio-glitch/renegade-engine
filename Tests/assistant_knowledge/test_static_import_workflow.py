"""Native static-model import guidance and adjacent-task exclusions."""
import json
from pathlib import Path
import sys
import tempfile
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/"Tools/assistant_knowledge"))
import bridge_server as bridge
from workflow_answers import verified_workflow, WORKFLOWS

class StaticImportWorkflowTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.repo=Path(__file__).resolve().parents[2]
        cls.db=cls.repo/"BUILD/assistant_knowledge.sqlite"

    def answer(self,q):
        return verified_workflow(q,self.repo,self.db)

    def test_import_phrasings(self):
        for q in ["How do I import a static model?",
                  "How do I import a GLB?",
                  "How do I import an FBX model?",
                  "How do I bring in a model?",
                  "How do I load a 3D object?",
                  "What steps to import a mesh?"]:
            with self.subTest(q=q):
                a=self.answer(q);self.assertIsNotNone(a)
                self.assertIn("**IMPORT MODEL...**",a)
                self.assertIn("**IMPORT ASSET**",a)
                self.assertIn("7. Drag that imported card",a)

    def test_characters_and_animations_are_separate(self):
        for q in ["How do I import a rigged model?",
                  "How do I import a character FBX?",
                  "How do I import an animation FBX?",
                  "How do I import a model and retarget its skeleton?",
                  "How do I import player arms?"]:
            with self.subTest(q=q):
                a=self.answer(q)
                if a is not None:
                    self.assertNotIn("import and place a static model",a.split("\n",1)[0])
                    self.assertTrue("rigged character" in a.split("\n",1)[0] or
                                    "external animation" in a.split("\n",1)[0])

    def test_unavailable_native_picker_formats_are_not_promoted(self):
        for q in ["How do I import an OBJ model?",
                  "How do I import a GLTF model?",
                  "How do I import a Blender file as a model?",
                  "How do I import a model from a Unitypackage?",
                  "How do I import a model ZIP?"]:
            with self.subTest(q=q):
                self.assertIsNone(self.answer(q))

    def test_texture_reimport_and_runtime_are_not_static_import(self):
        for q in ["How do I import a texture for a model?",
                  "How do I import an image for my model?",
                  "How do I reimport a model?",
                  "How do I move an imported model?",
                  "How do I load a model in runtime?",
                  "Automatically import a model and create an entire world"]:
            with self.subTest(q=q):
                self.assertIsNone(self.answer(q))

    def test_novice_requirements_and_outcome(self):
        a=self.answer("How do I import a static model?")
        for text in ["**General model**","**Import to**","**Content/Models/Props**",
                     "do not choose the folder","does not automatically add collision",
                     "placing it and saving the scene are separate","**Ctrl+S**"]:
            self.assertIn(text,a)
        self.assertLess(a.index("**Before you start:**"),a.index("\n1. "))

    def test_reveal_failure_does_not_advise_duplicate_import(self):
        a=self.answer("How do I import a model with textures?")
        self.assertIn("COMMITTED; REVEAL FAILED",a)
        self.assertIn("Do not import again",a)
        self.assertIn("self-contained GLB",a)
        self.assertIn("missing or changed textures",a)

    def test_stale_ui_source_abstains(self):
        data=json.loads(WORKFLOWS.read_text(encoding="utf-8"))
        task=next(t for t in data["workflows"] if t["id"]=="static-model-import-v1")
        task["evidence"][0]["sha256"]="0"*64
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)/"workflows.json";p.write_text(json.dumps(data),encoding="utf-8")
            self.assertIsNone(verified_workflow("How do I import a model?",self.repo,self.db,p))

    def test_topic_classification_stays_scoped(self):
        for q in ["How do I load a GLB?","How do I bring in a mesh?",
                  "How do I load a 3D object?"]:
            self.assertTrue(bridge.is_renegade_question(q))
        self.assertFalse(bridge.is_renegade_question("Did you enjoy that movie in 3D?"))

    def test_bridge_without_model(self):
        data=bridge.process_chat({"messages":[{"role":"user",
            "content":"How do I import a static model?"}]},self.repo,self.db,qwen_port=65530)
        self.assertEqual(data["route"],"local_verified_knowledge")
        self.assertFalse(data["editor_access"])
        self.assertIn("7. Drag that imported card",data["answer"])

    def test_verification_limit_is_explicit(self):
        a=self.answer("How do I import a model?")
        self.assertIn("generated static GLB".lower(),a.lower())
        self.assertIn("fresh beginner mouse-driven walkthrough remains pending",a)
        self.assertIn("cannot inspect or change your project",a)

if __name__=="__main__":
    unittest.main()
