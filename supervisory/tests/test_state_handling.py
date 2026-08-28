import unittest
import asyncio
import time
from unittest.mock import AsyncMock, MagicMock
from supervisory.app.orchestrator import Orchestrator

class TestStateHandling(unittest.TestCase):
    def setUp(self):
        self.orchestrator = Orchestrator()
        self.orchestrator.set_state = AsyncMock()

    def test_auto_recovery_to_warmup_on_unknown_state(self):
        async def run_test():
            # Telemetry arrives with unknown state code 4 (FAULT/UNKNOWN) and NO valid halt reason
            data = {
                "state": 4, # Unknown/Fault state code
                "sp": {"feed_pre": 100.0, "liq_reac": 150.0, "gas_reac": 200.0},
                "sensors": {
                    "t_feed_pre": 50.0,
                    "t_liq_reac": 50.0,
                    "t_gas_reac_int": 50.0,
                    "t_gas_reac_ext": 50.0,
                    "status": 0
                }
            }

            await self.orchestrator.handle_telemetry(data)
            # Should automatically issue set_state(1) to recover into WARMUP mode
            self.orchestrator.set_state.assert_called_once_with(1)

        asyncio.run(run_test())

    def test_transition_to_standby_on_firmware_safety_error(self):
        async def run_test():
            # Telemetry arrives with firmware safety trip message
            data = {
                "state": 2, # Currently in WORKING
                "fw_error": "THERMAL_SAFETY_LIMIT_EXCEEDED",
                "sensors": {"status": 0}
            }

            await self.orchestrator.handle_telemetry(data)
            # Should issue set_state(0) to transition to STANDBY cleanly
            self.orchestrator.set_state.assert_called_once_with(0)
            self.assertEqual(len(self.orchestrator.active_alarms), 1)
            self.assertEqual(self.orchestrator.active_alarms[0]["code"], "ERR_FIRMWARE_SAFETY")

        asyncio.run(run_test())

    def test_transition_to_standby_on_persistent_tc_fault(self):
        async def run_test():
            now = time.time()
            # Feedstock Preheater TC is faulty (nan reading)
            data = {
                "state": 2, # WORKING
                "sensors": {
                    "t_feed_pre": float("nan"),
                    "t_liq_reac": 150.0,
                    "t_gas_reac_int": 200.0,
                    "t_gas_reac_ext": 200.0,
                    "status": 0x02
                }
            }

            # Set offline start to 65 seconds ago (exceeds 60s limit)
            self.orchestrator.tc_offline_start["t_feed_pre"] = now - 65.0

            await self.orchestrator.handle_telemetry(data)
            # Valid halt reason present -> transition to STANDBY (State 0)
            self.orchestrator.set_state.assert_called_once_with(0)
            self.assertTrue(any(a["code"] == "ERR_TC_FAULT_PRE" for a in self.orchestrator.active_alarms))

        asyncio.run(run_test())

    def test_normal_operating_states_unmodified(self):
        async def run_test():
            # Normal STANDBY telemetry
            data = {
                "state": 0,
                "sensors": {"t_feed_pre": 25.0, "status": 0}
            }

            await self.orchestrator.handle_telemetry(data)
            self.orchestrator.set_state.assert_not_called()

        asyncio.run(run_test())

if __name__ == "__main__":
    unittest.main()
