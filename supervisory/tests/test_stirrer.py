import unittest
import asyncio
import os

# Set development env for testing fallback mock relay
os.environ["APP_ENV"] = "development"

from supervisory.app.config import settings
from supervisory.app.orchestrator import Orchestrator

class TestStirrerControl(unittest.TestCase):
    def setUp(self):
        self.orchestrator = Orchestrator()

    def test_stirrer_default_off(self):
        self.assertFalse(self.orchestrator.stirrer_state)
        if self.orchestrator.stirrer_relay:
            self.assertFalse(self.orchestrator.stirrer_relay.is_active)

    def test_stirrer_toggle(self):
        res = self.orchestrator.set_stirrer(True)
        self.assertTrue(res)
        self.assertTrue(self.orchestrator.stirrer_state)
        if self.orchestrator.stirrer_relay:
            self.assertTrue(self.orchestrator.stirrer_relay.is_active)

        res = self.orchestrator.set_stirrer(False)
        self.assertFalse(res)
        self.assertFalse(self.orchestrator.stirrer_state)
        if self.orchestrator.stirrer_relay:
            self.assertFalse(self.orchestrator.stirrer_relay.is_active)

    def test_telemetry_includes_stirrer(self):
        telemetry = {}
        asyncio.run(self.orchestrator.handle_telemetry(telemetry))
        self.assertIn("stirrer", telemetry)
        self.assertFalse(telemetry["stirrer"])

        self.orchestrator.set_stirrer(True)
        asyncio.run(self.orchestrator.handle_telemetry(telemetry))
        self.assertTrue(telemetry["stirrer"])

if __name__ == "__main__":
    unittest.main()
