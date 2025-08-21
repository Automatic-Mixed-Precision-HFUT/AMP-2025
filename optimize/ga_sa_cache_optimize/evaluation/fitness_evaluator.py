import os
import json
import copy
import subprocess
from typing import Dict, Any


import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from cache.cache_manager import CacheManager
from config.config_manager import ConfigManager
from config.conversion_steps import ConversionSteps
from evaluation.performance_parser import PerformanceParser


class FitnessEvaluator:

    def __init__(
        self,
        config_manager: ConfigManager,
        cache_manager: CacheManager,
        conversion_steps: ConversionSteps,
        performance_parser: PerformanceParser,
        work_dir: str,
        output_base: str,
    ):

        self.config_manager = config_manager
        self.cache_manager = cache_manager
        self.conversion_steps = conversion_steps
        self.performance_parser = performance_parser
        self.work_dir = work_dir
        self.output_base = output_base

        self.test_num = 1
        self.size_num = 5
        self.min_size = 300
        self.max_size = 1600

        self.ga_sa_improved_dir = os.environ.get("GA_SA_GA_SA_IMPROVED_DIR", work_dir)
        self.initial_ll = os.environ.get(
            "GA_SA_INITIAL_LL",
            os.path.join(self.ga_sa_improved_dir, "hpllink.ll"),
        )

        self._baseline_T0 = None

    def evaluate_fitness(self, config: Dict[str, Any], individual_id: str) -> float:

        self.cache_manager.mark_config_as_tested(config)

        individual_dir = os.path.join(self.output_base, f"individual_{individual_id}")
        os.makedirs(individual_dir, exist_ok=True)

        arm64_output_dir = os.path.join(individual_dir, "arm64_output")
        os.makedirs(arm64_output_dir, exist_ok=True)

        exclude_file = os.path.join(individual_dir, "exclude.txt")
        include_file = os.path.join(individual_dir, "include.txt")
        if not os.path.exists(exclude_file):
            open(exclude_file, "w").close()
        if not os.path.exists(include_file):
            open(include_file, "w").close()

        try:

            current_ll = self.initial_ll
            current_config = copy.deepcopy(self.config_manager.get_baseline_config())

            target_config = config
            conversion_steps = self.conversion_steps.get_conversion_steps(
                current_config, target_config
            )

            steps_len = len(conversion_steps)
            print(f"Individual {individual_id} conversion steps: {steps_len}")

            for step_idx, step_config in enumerate(conversion_steps):

                step_config_file = os.path.join(
                    individual_dir, f"step_{step_idx}_config.json"
                )
                self.config_manager.save_config(step_config, step_config_file)

                working_config_file = os.path.join(individual_dir, "config.json")
                self.config_manager.save_config(step_config, working_config_file)

                output_ll = os.path.join(
                    arm64_output_dir, f"step_{step_idx}_optimized.ll"
                )

                libmix_path = os.environ["GA_SA_LIBMIX_PATH"]

                opt_cmd = [
                    "opt",
                    f"-load-pass-plugin={libmix_path}",
                    current_ll,
                    "-S",
                    "-o",
                    output_ll,
                    "-passes=pl",
                ]

                result = subprocess.run(
                    opt_cmd,
                    cwd=individual_dir,
                    capture_output=True,
                    text=True,
                )
                if result.returncode != 0:
                    print(
                        f"opt failed for individual {individual_id} "
                        f"step {step_idx}: {result.stderr}"
                    )
                    return float("inf")

                current_ll = output_ll
                current_config = step_config

            final_ll = os.path.join(arm64_output_dir, "hpllink_optimized.ll")
            if os.path.exists(current_ll):
                import shutil

                shutil.copy2(current_ll, final_ll)

            opt_o2_cmd = ["opt", final_ll, "-S", "-o", final_ll, "-O2"]

            result = subprocess.run(
                opt_o2_cmd,
                cwd=individual_dir,
                capture_output=True,
                text=True,
            )
            if result.returncode != 0:
                print(f"opt -O2 failed for individual {individual_id}: {result.stderr}")
                return float("inf")

            final_config_file = os.path.join(individual_dir, "config.json")
            self.config_manager.save_config(target_config, final_config_file)

            llc_cmd = [
                "llc",
                os.path.join(arm64_output_dir, "hpllink_optimized.ll"),
                "-o",
                os.path.join(arm64_output_dir, "hpllink_optimized.s"),
            ]

            result = subprocess.run(
                llc_cmd, cwd=individual_dir, capture_output=True, text=True
            )
            if result.returncode != 0:
                print(f"llc failed for individual {individual_id}: {result.stderr}")
                return float("inf")

            clang_cmd = [
                "clang",
                "--target=aarch64-linux-gnu",
                "-march=armv8.2-a+fp16",
                "-O2",
                "-static",
                os.path.join(arm64_output_dir, "hpllink_optimized.s"),
                "-o",
                os.path.join(arm64_output_dir, "hpl_exec_optimized"),
                "-lm",
            ]

            result = subprocess.run(
                clang_cmd,
                cwd=individual_dir,
                capture_output=True,
                text=True,
            )
            if result.returncode != 0:
                print(f"clang failed for individual {individual_id}: {result.stderr}")
                return float("inf")

            qemu_outputs = []

            for test_run in range(self.test_num):
                qemu_cmd = [
                    "qemu-aarch64",
                    "-L",
                    "/usr/aarch64-linux-gnu",
                    os.path.join(arm64_output_dir, "hpl_exec_optimized"),
                    str(self.size_num),
                    str(self.min_size),
                    str(self.max_size),
                ]

                result = subprocess.run(
                    qemu_cmd,
                    cwd=individual_dir,
                    capture_output=True,
                    text=True,
                )

                if result.returncode == 0:

                    qemu_output = {
                        "test_run": test_run + 1,
                        "stdout": result.stdout,
                        "stderr": result.stderr,
                        "returncode": result.returncode,
                    }
                    qemu_outputs.append(qemu_output)

                    qemu_output_file = os.path.join(
                        individual_dir, f"qemu_output_{test_run + 1}.json"
                    )
                    with open(qemu_output_file, "w") as f:
                        json.dump(qemu_output, f, indent=2)
                else:
                    print(
                        f"qemu failed for individual {individual_id}: {result.stderr}"
                    )
                    return float("inf")

            if qemu_outputs:

                last_output = qemu_outputs[-1]["stdout"]
                performance_file = os.path.join(
                    individual_dir, "optimized_performance.txt"
                )
                with open(performance_file, "w") as f:
                    f.write(last_output)

                qemu_summary = {
                    "individual_id": individual_id,
                    "test_num": self.test_num,
                    "qemu_outputs": qemu_outputs,
                }

                qemu_summary_file = os.path.join(individual_dir, "qemu_summary.json")
                with open(qemu_summary_file, "w") as f:
                    json.dump(qemu_summary, f, indent=2)

            if qemu_outputs:

                output = qemu_outputs[-1]["stdout"]
                pass_rate, min_flops, mean_flops, max_flops = (
                    self.performance_parser.parse_hplai_metrics_from_output(output)
                )

                if self._baseline_T0 is None:

                    min_T0 = 4.5471
                    mean_T0 = 7.5516
                    max_T0 = 9.6609
                    self._baseline_T0 = (min_T0, mean_T0, max_T0)
                    print("=== Baseline configuration performance ===")
                    print(f"Baseline min performance: {min_T0:.4f} Gflops")
                    print(f"Baseline mean performance: {mean_T0:.4f} Gflops")
                    print(f"Baseline max performance: {max_T0:.4f} Gflops")
                    print("=========================")

                fitness = self.performance_parser.calculate_fitness(
                    pass_rate, min_flops, mean_flops, max_flops, self._baseline_T0
                )

                self.cache_manager.mark_config_as_tested(
                    config, fitness=fitness, evaluation_type="actual"
                )

                print(f"Individual {individual_id} final_marks: {-fitness:.4f}")
                return fitness
            else:
                return float("inf")

        except Exception as e:
            print(f"Error evaluating individual {individual_id}: {e}")
            return float("inf")
