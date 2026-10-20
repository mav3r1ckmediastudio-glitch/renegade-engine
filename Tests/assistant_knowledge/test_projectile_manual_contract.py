"""Source contract for the beginner projectile manual.

Runs read-only; checks UI labels, setup and persistence in the actual Studio
source. This is NOT a substitute for a native visual/gameplay walkthrough.
"""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]
STUDIO = (ROOT / "Studio/src/PlayerProjectileEditor.cpp").read_text(encoding="utf-8")
EQUIPMENT = (ROOT / "Studio/src/PlayerEquipmentEditor.cpp").read_text(encoding="utf-8")
MANUAL = (ROOT / "docs/manual/PROJECTILES_FIRST_WEAPON.md").read_text(encoding="utf-8")
UX = (ROOT / "docs/P3_PROJECTILE_UX_CLEANUP.md").read_text(encoding="utf-8")


class ProjectileManualContract(unittest.TestCase):
    def test_controls_mentioned_by_manual_exist(self):
        for label in (
            "WEAPON FIRING", "Weapon hand", "Launch on", "Fire mode",
            "Projectile / travelling", "NEW", "EDIT COPY",
            "Spawn point", "Selected spawn point", "Alternate between two",
            "Both together", "Fire at", "Animation start", "After a delay",
            "SET FIRE TIME IN ANIMATION...", "MARK CURRENT POSE",
            "USE FIRE TIME", "APPLY TO THIS PLAYER", "SHOW ADVANCED"
        ):
            with self.subTest(label=label):
                self.assertIn(label, MANUAL)
                self.assertIn(label, STUDIO + EQUIPMENT)

    def test_new_starts_as_bullet_then_arrow_is_available(self):
        self.assertIn("MakeProjectilePreset(bridge::ProjectilePreset::Bullet)", STUDIO)
        self.assertIn("projectilePreset_.AddItem(bridge::ProjectilePresetName", STUDIO)
        self.assertIn("bridge::ProjectilePreset::Arrow", STUDIO)
        self.assertIn("NEW initially opens the Bullet preset", MANUAL)

    def test_saving_projectile_is_distinct_from_player_assignment(self):
        self.assertIn("bridge::SaveProjectileAsset", STUDIO)
        self.assertIn("bridge::SaveEquipmentAsset", STUDIO)
        self.assertIn("SetPlayerControllerSettingsCommand", STUDIO)
        self.assertIn("Saving the projectile alone does not assign it", MANUAL)

    def test_one_point_firing_policy_is_hidden(self):
        self.assertIn("const bool multiple=projectileAvailableSocketCount_>1;", STUDIO)
        self.assertIn("projectileSocketPolicy_.SetVisible(policyVisible);", STUDIO)
        self.assertIn("dropdown is normally hidden", MANUAL)

    def test_timing_marker_bounds(self):
        self.assertIn("t>=projectileTimingDuration_", STUDIO)
        self.assertIn("t>5", STUDIO)
        self.assertIn("before the end", MANUAL)

    def test_native_manual_screenshots_exist(self):
        images = (
            "01-player-start.png", "02-weapon-firing.png",
            "03-arrow-preset.png", "04-advanced-projectile.png",
            "05-animation-timing.png", "06-projectile-saved.png",
            "07-reopened-weapon-firing.png", "08-reopened-project-equipment.png",
            "09-testlevel-release.png", "10-testlevel-impact.png",
            "11-testlevel-retained.png"
        )
        for image in images:
            with self.subTest(image=image):
                self.assertIn("images/" + image, MANUAL)
                self.assertTrue((ROOT / "docs/manual/images" / image).is_file())

    def test_native_e2e_receipt_is_kept_separate_from_publication(self):
        import json
        record = json.loads((ROOT / "Tools/assistant_knowledge/projectile_guide.json")
                            .read_text(encoding="utf-8"))
        receipt = ROOT / record["end_to_end_acceptance"]["receipt"]
        self.assertTrue(receipt.is_file())
        self.assertEqual(record["end_to_end_acceptance"]["status"],
                         "native_workflow_passed_existing_bow_fixture")
        self.assertFalse(record["end_to_end_acceptance"]["publication_approved"])
        self.assertIn("original", receipt.read_text(encoding="utf-8").lower())

    def test_editor_safety_scope(self):
        self.assertIn("draft", MANUAL.lower())
        self.assertIn("Publication gate", MANUAL)
        self.assertIn("not committed", (ROOT/"Tools/assistant_knowledge/README.md").read_text(encoding="utf-8"))


if __name__ == "__main__":
    unittest.main()
