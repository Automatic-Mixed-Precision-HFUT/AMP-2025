

import json
import os
import copy
from typing import Dict, Any, List, Set


class GroupSearchEngine:


    def __init__(self, grouped_dir: str, enable_group_search: bool = False):
        
        self.grouped_dir = grouped_dir
        self.enable_group_search = enable_group_search
        self.group_files = [
            "group_depth_ge_1.json",
            "group_depth_ge_2.json",
            "group_depth_ge_3.json",
            "group_depth_ge_4.json",
            "group_by_function.json",
        ]
        self.precision_hierarchy = {
            "double": 3,
            "float": 2,
            "half": 1,
        }
        self.precision_hierarchy_reverse = {
            v: k for k, v in self.precision_hierarchy.items()
        }


        self.group_data = {}
        if self.enable_group_search:
            self._load_group_data()

    def _load_group_data(self):

        for group_file in self.group_files:
            file_path = os.path.join(self.grouped_dir, group_file)
            if os.path.exists(file_path):
                try:
                    with open(file_path, "r", encoding="utf-8") as f:
                        self.group_data[group_file] = json.load(f)
                    print(f"✓ Loaded group file: {group_file}")
                except Exception as e:
                    print(f"⚠️ Failed to load group file {group_file}: {e}")
            else:
                print(f"⚠️ Group file does not exist: {file_path}")

    def _get_variables_in_group(self, group_file: str) -> Set[tuple]:

        variables = set()
        if group_file in self.group_data:
            for function_name, var_list in self.group_data[group_file].items():
                for var in var_list:
                    variables.add((var["function"], var["name"]))
        return variables

    def _find_lowest_precision_in_function_group(
        self,
        config: Dict[str, Any],
        function_name: str,
        group_variables: Set[tuple],
    ) -> str:

        min_precision = 3

        for var in config.get("localVar", []):
            var_key = (var["function"], var["name"])
            if var_key in group_variables and var["function"] == function_name:
                current_precision = self.precision_hierarchy.get(var["type"], 3)
                min_precision = min(min_precision, current_precision)

        return self.precision_hierarchy_reverse.get(min_precision, "double")

    def adjust_config_by_group(
        self, config: Dict[str, Any], group_file: str
    ) -> Dict[str, Any]:

        if not self.enable_group_search:
            return config

        adjusted_config = copy.deepcopy(config)
        group_variables = self._get_variables_in_group(group_file)

        if not group_variables:
            return adjusted_config


        if group_file in self.group_data:
            for function_name, var_list in self.group_data[group_file].items():

                function_variables = set(
                    (var["function"], var["name"]) for var in var_list
                )
                lowest_precision = self._find_lowest_precision_in_function_group(
                    config,
                    function_name,
                    function_variables,
                )


                for var in adjusted_config.get("localVar", []):
                    var_key = (var["function"], var["name"])
                    if var_key in function_variables:

                        if var["type"].endswith("*"):
                            var["type"] = f"{lowest_precision}*"
                        else:
                            var["type"] = lowest_precision

        return adjusted_config

    def get_group_files(self) -> List[str]:

        return self.group_files

    def run_group_search(
        self,
        genetic_optimizer_class,
        optimizer_params: Dict[str, Any],
        output_base: str,
    ) -> List[Dict[str, Any]]:

        if not self.enable_group_search:
            print("Group search not enabled")
            return []

        best_configs = []

        for group_file in self.group_files:
            print(f"\n=== Starting Group Search: {group_file} ===")


            group_output_dir = os.path.join(
                output_base,
                f"group_search_{group_file.replace('.json', '')}",
            )


            group_optimizer_params = optimizer_params.copy()
            group_optimizer_params.update(
                {
                    "population_size": 3,
                    "generations": 3,
                    "output_dir": group_output_dir,
                    "group_file": group_file,
                    "use_surrogate": True,
                    "surrogate_confidence_threshold": 0.7,
                    "skip_evaluation_threshold": 0.3,
                    "force_evaluation_threshold": 0.1,
                }
            )

            try:

                optimizer = genetic_optimizer_class(**group_optimizer_params)


                best_config, best_fitness = optimizer.optimize()

                if best_config:
                    best_configs.append(
                        {
                            "group_file": group_file,
                            "config": best_config,
                            "fitness": best_fitness,
                            "surrogate_predictions": optimizer.surrogate_predictions,
                            "actual_evaluations": optimizer.actual_evaluations,
                        }
                    )
                    print(
                        f"✓ {group_file} search completed, "
                        f"best fitness: {best_fitness:.4f}"
                    )
                    print(
                        f"  Surrogate predictions: "
                        f"{optimizer.surrogate_predictions}"
                    )
                    print("  Actual evaluations: " f"{optimizer.actual_evaluations}")
                else:
                    print(f"⚠️ {group_file} search found no valid " f"configuration")

            except Exception as e:
                print(f"⚠️ {group_file} search failed: {e}")
                continue

            print(f"=== Group Search Completed: {group_file} ===\n")

        return best_configs

    def save_group_search_results(
        self,
        best_configs: List[Dict[str, Any]],
        configs_dir: str,
    ):

        if not best_configs:
            return

        for i, result in enumerate(best_configs):
            group_name = result["group_file"].replace(".json", "")
            config_file = os.path.join(
                configs_dir, f"group_search_{group_name}_best.json"
            )

            try:
                with open(config_file, "w", encoding="utf-8") as f:
                    json.dump(result["config"], f, indent=4, ensure_ascii=False)
                print(f"✓ Saved group search result: {config_file}")
            except Exception as e:
                print(f"⚠️ Failed to save group search result " f"{config_file}: {e}")

    def print_group_info(self):

        if not self.enable_group_search:
            print("Group search not enabled")
            return

        print("\n=== Group Search Information ===")
        for group_file in self.group_files:
            if group_file in self.group_data:
                var_count = sum(
                    len(vars) for vars in self.group_data[group_file].values()
                )
                print(f"{group_file}: {var_count} variables")
            else:
                print(f"{group_file}: not loaded")
        print("==================\n")
