

import json
import os
from typing import List, Dict, Any


class ConfigManager:


    def __init__(self, config_dir: str = "firstconfigs"):

        self.config_dir = config_dir
        self.initial_configs = self.load_initial_configs(config_dir)
        self.initial_config = self._get_baseline_config()
        self.variables = self.initial_config.get("localVar", [])

    def load_initial_configs(self, config_dir: str) -> List[Dict[str, Any]]:

        configs = []


        possible_paths = [
            config_dir,
            os.path.join(os.getcwd(), config_dir),
            os.path.join(os.path.dirname(__file__), "..", "..", config_dir),
            os.path.join(os.path.dirname(__file__), "..", "..", "configs"),
            os.environ.get('GA_SA_CONFIG_DIR', ''),
        ]

        config_dir_path = None
        for path in possible_paths:
            if os.path.exists(path):
                config_dir_path = path
                break

        if config_dir_path is None:
            print(f"Warning: Configuration directory {config_dir} not found")
            print(f"Tried paths: {possible_paths}")
            return configs


        json_files = []
        for filename in os.listdir(config_dir_path):
            if filename.endswith(".json"):
                json_files.append(filename)


        json_files.sort()


        for filename in json_files:
            config_file = os.path.join(config_dir_path, filename)
            try:
                with open(config_file, "r", encoding="utf-8") as f:
                    config = json.load(f)
                    configs.append(config)
            except Exception as e:
                print(f"Failed to load configuration file {filename}: {e}")


        return configs

    def load_config(self, config_file: str) -> Dict[str, Any]:

        with open(config_file, "r", encoding="utf-8") as f:
            return json.load(f)

    def save_config(self, config: Dict[str, Any], filename: str):

        with open(filename, "w", encoding="utf-8") as f:
            json.dump(config, f, indent=4, ensure_ascii=False)

    def _get_baseline_config(self) -> Dict[str, Any]:

        if not self.initial_configs:
            raise ValueError(
                f"In directory {self.config_dir} no configuration files were found"
            )


        alldouble_config_path = os.path.join(self.config_dir, "alldouble.json")

        if os.path.exists(alldouble_config_path):
            baseline_config = self.load_config(alldouble_config_path)
            print("Using alldouble.json as baseline configuration")
        else:

            baseline_config = self.initial_configs[0]
            print("alldouble.json not found, using first configuration as baseline")

        return baseline_config

    def get_initial_configs(self) -> List[Dict[str, Any]]:

        return self.initial_configs

    def get_baseline_config(self) -> Dict[str, Any]:

        return self.initial_config

    def get_variables(self) -> List[Dict[str, Any]]:
        
        return self.variables
