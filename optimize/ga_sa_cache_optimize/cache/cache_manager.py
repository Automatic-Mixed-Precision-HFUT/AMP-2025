import json
import os
from datetime import datetime
from typing import Dict, Any, Set, Optional


class CacheManager:


    def __init__(self, cache_file: str):

        self.cache_file = cache_file
        self.tested_configs: Set[str] = set()
        self.config_details: Dict[str, Dict[str, Any]] = {}
        self.load_cache()

    def get_config_hash(self, config: Dict[str, Any]) -> str:

        var_types = []
        for var in config.get("localVar", []):
            var_types.append(f"{var['function']}.{var['name']}:{var['type']}")


        var_types.sort()
        config_str = "|".join(var_types)


        import hashlib

        return hashlib.md5(config_str.encode()).hexdigest()

    def is_config_tested(self, config: Dict[str, Any]) -> bool:

        config_hash = self.get_config_hash(config)
        return config_hash in self.tested_configs

    def mark_config_as_tested(
        self,
        config: Dict[str, Any],
        fitness: float = None,
        evaluation_type: str = "actual",
    ):

        config_hash = self.get_config_hash(config)
        self.tested_configs.add(config_hash)


        self.config_details[config_hash] = {
            "config": config,
            "timestamp": datetime.now().isoformat(),
            "hash": config_hash,
            "fitness": fitness,
            "evaluation_type": evaluation_type,
        }


        if len(self.tested_configs) % 10 == 0:
            self.save_cache()

    def get_tested_configs_count(self) -> int:
        return len(self.tested_configs)

    def load_cache(self):

        if os.path.exists(self.cache_file):
            try:
                with open(self.cache_file, "r", encoding="utf-8") as f:
                    cache_data = json.load(f)


                if isinstance(cache_data, list):
                    self.tested_configs = set(cache_data)
                    print(
                        f"Loaded {len(self.tested_configs)} tested "
                        f"configuration cache (old format)"
                    )
                else:

                    self.tested_configs = set(cache_data.get("hashes", []))
                    self.config_details = cache_data.get("configs", {})
                    print(
                        f"Loaded {len(self.tested_configs)} tested "
                        f"configuration cache (new format)"
                    )
                    print(
                        f"Number of configuration details: "
                        f"{len(self.config_details)}"
                    )
            except json.JSONDecodeError:
                print(
                    f"Failed to load cache file {self.cache_file}, "
                    f"will restart testing"
                )
            except Exception as e:
                print(f"Failed to load cache file {self.cache_file}: {e}")
        else:
            print(
                f"Cache file {self.cache_file} does not exist, " f"will restart testing"
            )

    def save_cache(self):

        try:

            cache_data = {
                "hashes": list(self.tested_configs),
                "configs": self.config_details,
                "metadata": {
                    "total_configs": len(self.tested_configs),
                    "last_updated": datetime.now().isoformat(),
                    "version": "2.0",
                },
            }

            with open(self.cache_file, "w", encoding="utf-8") as f:
                json.dump(cache_data, f, indent=4, ensure_ascii=False)
            print(
                f"Saved {len(self.tested_configs)} tested "
                f"configuration cache to {self.cache_file}"
            )
        except Exception as e:
            print(f"Failed to save cache file {self.cache_file}: {e}")

    def clear_cache(self):

        try:
            if os.path.exists(self.cache_file):
                os.remove(self.cache_file)
                print(f"Cleared cache file: {self.cache_file}")
            self.tested_configs.clear()
            self.config_details.clear()
            print("Cleared tested configurations in memory")
        except Exception as e:
            print(f"Failed to clear cache file: {e}")

    def get_cache_info(self) -> Dict[str, Any]:
        cache_info = {
            "cache_file": self.cache_file,
            "cache_exists": os.path.exists(self.cache_file),
            "tested_configs_count": len(self.tested_configs),
            "cache_size_bytes": (
                os.path.getsize(self.cache_file)
                if os.path.exists(self.cache_file)
                else 0
            ),
        }
        return cache_info

    def get_config_by_hash(self, config_hash: str) -> Optional[Dict[str, Any]]:
        if config_hash in self.config_details:
            return self.config_details[config_hash]
        return None

    def get_config_fitness(self, config: Dict[str, Any]) -> Optional[float]:
        config_hash = self.get_config_hash(config)
        if config_hash in self.config_details:
            return self.config_details[config_hash].get("fitness")
        return None

    def is_config_evaluated_actually(self, config: Dict[str, Any]) -> bool:
        config_hash = self.get_config_hash(config)
        if config_hash in self.config_details:
            evaluation_type = self.config_details[config_hash].get(
                "evaluation_type", "unknown"
            )
            return evaluation_type == "actual"
        return False

    def print_cache_info(self):

        print(f"Number of tested configurations: " f"{self.get_tested_configs_count()}")
        print(f"Cache file: {self.cache_file}")
        if os.path.exists(self.cache_file):
            cache_size = os.path.getsize(self.cache_file)
            print("Cache file size:")
            print(str(cache_size) + " bytes")
