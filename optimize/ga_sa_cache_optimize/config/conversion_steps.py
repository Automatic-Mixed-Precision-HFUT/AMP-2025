import copy
from typing import List, Dict, Any


class ConversionSteps:

    def get_conversion_steps(
        self, current_config: Dict[str, Any], target_config: Dict[str, Any]
    ) -> List[Dict[str, Any]]:

        steps = []
        current_vars = {
            f"{var['function']}.{var['name']}": var
            for var in current_config.get("localVar", [])
        }
        target_vars = {
            f"{var['function']}.{var['name']}": var
            for var in target_config.get("localVar", [])
        }

        conversion_needed = {}
        for var_key, target_var in target_vars.items():
            if var_key in current_vars:
                current_type = current_vars[var_key]["type"]
                target_type = target_var["type"]

                if current_type != target_type:
                    conversion_needed[var_key] = {
                        "current": current_type,
                        "target": target_type,
                        "var": target_var,
                    }

        if not conversion_needed:

            return [target_config]

        precision_order = {"double": 0, "float": 1, "half": 2}

        step1_config = copy.deepcopy(current_config)
        step1_vars = {
            f"{var['function']}.{var['name']}": var
            for var in step1_config.get("localVar", [])
        }

        for var_key, conversion in conversion_needed.items():
            current_type = conversion["current"]
            target_type = conversion["target"]

            is_double_to_lower = (
                current_type == "double" and target_type in ["float", "half"]
            ) or (current_type == "double*" and target_type in ["float*", "half*"])

            if is_double_to_lower:

                if target_type in ["half", "half*"]:

                    if current_type.endswith("*"):
                        step1_vars[var_key]["type"] = "float*"
                    else:
                        step1_vars[var_key]["type"] = "float"
                else:

                    step1_vars[var_key]["type"] = target_type

        if step1_config != current_config:
            steps.append(step1_config)

        step2_config = copy.deepcopy(step1_config)
        step2_vars = {
            f"{var['function']}.{var['name']}": var
            for var in step2_config.get("localVar", [])
        }

        for var_key, conversion in conversion_needed.items():
            target_type = conversion["target"]

            if target_type in ["half", "half*"]:
                step2_vars[var_key]["type"] = target_type

        if step2_config != step1_config:
            steps.append(step2_config)

        if not steps:
            return [target_config]

        return steps
