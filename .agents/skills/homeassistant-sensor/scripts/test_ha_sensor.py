import tempfile
import unittest
from pathlib import Path
from unittest.mock import MagicMock, patch

from ha_sensor import fetch_sensor, load_env


class HaSensorTest(unittest.TestCase):
    def test_load_env(self):
        with tempfile.NamedTemporaryFile("w+", delete=False) as tf:
            tf.write("HA_URL=http://localhost:8123\nHA_TOKEN='secret'\n# comment\nHA_ENTITY_ID=\"sensor.temp\"\n")
            path = Path(tf.name)
        try:
            env = load_env(path)
            self.assertEqual(env["HA_URL"], "http://localhost:8123")
            self.assertEqual(env["HA_TOKEN"], "secret")
            self.assertEqual(env["HA_ENTITY_ID"], "sensor.temp")
        finally:
            path.unlink(missing_ok=True)

    @patch("urllib.request.urlopen")
    def test_fetch_sensor(self, mock_urlopen):
        mock_resp = MagicMock()
        mock_resp.read.return_value = b'{"entity_id": "sensor.out_temp", "state": "17.5"}'
        mock_resp.__enter__.return_value = mock_resp
        mock_urlopen.return_value = mock_resp

        data = fetch_sensor("http://example.local:8123", "token123", "sensor.out_temp")
        self.assertEqual(data["state"], "17.5")
        self.assertEqual(data["entity_id"], "sensor.out_temp")


if __name__ == "__main__":
    unittest.main()
