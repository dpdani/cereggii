from pathlib import Path

import yaml
from cereggii import AtomicDict


stored_hashes = Path(__file__).parent / "keys_for_hash_for_log_size.yml"

if not stored_hashes.exists():
    d = AtomicDict()

    max_search_log_size = 10
    keys_for_hash_for_log_size = {}

    for log_size in range(6, max_search_log_size):
        keys_for_hash_for_log_size[log_size] = {}

        for pos in range(1 << log_size):
            keys_for_hash_for_log_size[log_size][pos] = []

            for _ in range((1 << log_size) * 32):
                h = d.rehash(_) >> (64 - log_size)
                if h == pos:
                    keys_for_hash_for_log_size[log_size][pos].append(_)

    with open(stored_hashes, "w") as f:
        yaml.dump(keys_for_hash_for_log_size, f)
else:
    with open(stored_hashes, "r") as f:
        keys_for_hash_for_log_size = yaml.load(f, yaml.SafeLoader)
