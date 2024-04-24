import pickle
from pathlib import Path

from cereggii import AtomicDict


stored_hashes = Path(__file__).parent / "keys_for_hash_for_log_size.pickle"

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

    with open(stored_hashes, "wb") as f:
        pickle.dump(keys_for_hash_for_log_size, f)
else:
    with open(stored_hashes, "rb") as f:
        keys_for_hash_for_log_size = pickle.load(f)  # noqa: S301
