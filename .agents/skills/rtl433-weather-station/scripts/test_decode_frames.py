import unittest

from decode_frames import temperature, valid_frame


class DecodeFramesTest(unittest.TestCase):
    def test_known_frames(self):
        frame = bytes.fromhex("a5d2fd305f0b8332")
        self.assertTrue(valid_frame(frame))
        self.assertAlmostEqual(temperature(frame, 20.0), 20.0)

    def test_rejects_bad_frame(self):
        self.assertFalse(valid_frame(bytes.fromhex("a5d2fd305f0b8333")))
        self.assertFalse(valid_frame(b"a5d2fd"))


if __name__ == "__main__":
    unittest.main()
