# Re-export from experiments/theory.py — the canonical location.
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))

from theory import *  # noqa: F401,F403
