import unittest
import asyncio
import time
from unittest.mock import AsyncMock, patch
from supervisory.app.orchestrator import Orchestrator

class TestWarmupAutoTransition(unittest.TestCase):
    def setUp(self):
        self.orchestrator = Orchestrator()
        self.orchestrator.set_state = AsyncMock()

    def test_auto_transition_when_all_zones_satisfied(self):
        async def run_test():
            now = time.time()
            data = {
                "state": 1, # WARMUP
                "sp": {"feed_pre": 100.0, "liq_reac": 150.0, "gas_reac": 200.0},
                "sensors": {
                    "t_feed_pre": 98.0,  # Within ±5°C (100 - 98 = 2)
                    "t_liq_reac": 153.0, # Within ±5°C (153 - 150 = 3)
                    "t_gas_reac_int": 200.0,
                    "t_gas_reac_ext": 200.0
                }
            }

            # First tick: start stability timer
            await self.orchestrator.handle_telemetry(data)
            self.assertIsNotNone(self.orchestrator.warmup_stable_start)
            self.orchestrator.set_state.assert_not_called()

            # Fast forward stability start time by 5.1s
            self.orchestrator.warmup_stable_start = now - 5.1

            # Second tick: trigger transition
            await self.orchestrator.handle_telemetry(data)
            self.orchestrator.set_state.assert_called_once_with(2)
            self.assertIsNone(self.orchestrator.warmup_stable_start)

        asyncio.run(run_test())

    def test_zero_setpoint_treated_as_disabled_and_satisfied(self):
        async def run_test():
            now = time.time()
            data = {
                "state": 1, # WARMUP
                "sp": {"feed_pre": 0.0, "liq_reac": 0.0, "gas_reac": 200.0}, # Zones 0 and 1 disabled
                "sensors": {
                    "t_feed_pre": 25.0,  # Ambient
                    "t_liq_reac": 25.0,  # Ambient
                    "t_gas_reac_int": 196.0, # Within ±5°C of 200
                    "t_gas_reac_ext": 196.0
                }
            }

            await self.orchestrator.handle_telemetry(data)
            self.assertIsNotNone(self.orchestrator.warmup_stable_start)

            # Fast forward stability start
            self.orchestrator.warmup_stable_start = now - 5.1
            await self.orchestrator.handle_telemetry(data)
            self.orchestrator.set_state.assert_called_once_with(2)

        asyncio.run(run_test())

    def test_no_transition_if_zone_outside_tolerance(self):
        async def run_test():
            data = {
                "state": 1, # WARMUP
                "sp": {"feed_pre": 100.0, "liq_reac": 150.0, "gas_reac": 200.0},
                "sensors": {
                    "t_feed_pre": 90.0,  # Outside ±5°C (100 - 90 = 10)
                    "t_liq_reac": 150.0,
                    "t_gas_reac_int": 200.0,
                    "t_gas_reac_ext": 200.0
                }
            }

            await self.orchestrator.handle_telemetry(data)
            self.assertIsNone(self.orchestrator.warmup_stable_start)
            self.orchestrator.set_state.assert_not_called()

        asyncio.run(run_test())

    def test_no_transition_during_active_ramp(self):
        async def run_test():
            # Active ramp to 150.0 while current setpoint is 100.0
            self.orchestrator.ramps[0] = {"target": 150.0, "rate_per_sec": 1.0, "last_update": time.time()}
            data = {
                "state": 1, # WARMUP
                "sp": {"feed_pre": 100.0, "liq_reac": 150.0, "gas_reac": 200.0},
                "sensors": {
                    "t_feed_pre": 100.0,
                    "t_liq_reac": 150.0,
                    "t_gas_reac_int": 200.0,
                    "t_gas_reac_ext": 200.0
                }
            }

            await self.orchestrator.handle_telemetry(data)
            self.assertIsNone(self.orchestrator.warmup_stable_start)
            self.orchestrator.set_state.assert_not_called()

        asyncio.run(run_test())

if __name__ == "__main__":
    unittest.main()
