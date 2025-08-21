

import re
from typing import Tuple, Optional


class PerformanceParser:


    def parse_hplai_metrics_from_output(
        self, output: str
    ) -> Tuple[float, float, float, float]:

        try:
            lines = output.split("\n")
            pass_rate = 0.0
            min_flops = mean_flops = max_flops = 0.0

            for line in lines:
                if "Passing Rate" in line:
                    match = re.search(r"Passing Rate:\s*([\d.]+)%", line)
                    if match:
                        pass_rate = float(match.group(1)) / 100.0
                        print(f"Found passing rate: {pass_rate}")

                if "Smallest/Average/Largest Performance" in line:
                    match = re.search(
                        r"Smallest/Average/Largest Performance\s*=\s*"
                        r"([\d.]+)\s*Gflops,\s*([\d.]+)\s*Gflops,\s*([\d.]+)\s*Gflops",
                        line,
                    )
                    if match:
                        min_flops = float(match.group(1))
                        mean_flops = float(match.group(2))
                        max_flops = float(match.group(3))
                        print(
                            f"Found performance: min={min_flops}, mean={mean_flops}, max={max_flops}"
                        )


            if min_flops == 0.0 and mean_flops == 0.0 and max_flops == 0.0:
                performances = []
                for line in lines:
                    if "Performance =" in line and "Gflops" in line:
                        match = re.search(r"Performance\s*=\s*([\d.]+)\s*Gflops", line)
                        if match:
                            performances.append(float(match.group(1)))

                if performances:
                    min_flops = min(performances)
                    mean_flops = sum(performances) / len(performances)
                    max_flops = max(performances)
                    print(
                        f"Parsed from individual lines: min={min_flops}, mean={mean_flops}, max={max_flops}"
                    )

            return pass_rate, min_flops, mean_flops, max_flops
        except Exception as e:
            print(f"Error parsing HPL-AI metrics from output: {e}")
            print(f"Output preview: {output[:200]}...")
            return 0.0, 0.0, 0.0, 0.0

    def parse_gflops_from_output(self, output: str) -> Optional[float]:

        try:

            lines = output.split("\n")
            for line in lines:
                if "Average Performance" in line:


                    parts = line.split("=")
                    if len(parts) >= 2:
                        performance_part = parts[1].strip()

                        gflops_parts = performance_part.split(",")
                        if len(gflops_parts) >= 2:
                            avg_part = gflops_parts[1].strip()

                            match = re.search(r"(\d+\.\d+)", avg_part)
                            if match:
                                return float(match.group(1))


            for line in lines:
                if "Performance =" in line and "Gflops" in line:
                    match = re.search(r"Performance\s*=\s*(\d+\.\d+)\s*Gflops", line)
                    if match:
                        return float(match.group(1))

            return None
        except Exception as e:
            print(f"Error parsing Gflops from output: {e}")
            return None

    def calculate_fitness(
        self,
        pass_rate: float,
        min_flops: float,
        mean_flops: float,
        max_flops: float,
        baseline_T0: Tuple[float, float, float],
    ) -> float:
        
        min_T0, mean_T0, max_T0 = baseline_T0


        flops_marks = (
            100
            * (
                0.6 * max_flops / max_T0
                + 0.3 * mean_flops / mean_T0
                + 0.1 * min_flops / min_T0
            )
            / 2
        )
        final_marks = pass_rate * 100 * 0.4 + flops_marks * 0.6

        print(f"Fitness calculation:")
        print(f"  pass_rate: {pass_rate}, flops_marks: {flops_marks:.4f}")
        print(f"  final_marks: {final_marks:.4f}")

        return -final_marks
