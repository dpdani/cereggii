import sys
from pathlib import Path

import cereggii

sys.path.append(str(Path(cereggii.__file__).parent))

from c_api._c_api import *

__version__ = "1.0"
