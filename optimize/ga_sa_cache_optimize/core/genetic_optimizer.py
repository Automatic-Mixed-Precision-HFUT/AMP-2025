

import os
import copy
import statistics
from typing import List, Dict, Any, Tuple


import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from cache.cache_manager import CacheManager
from cache.config_deduplication import ConfigDeduplication
from config.config_manager import ConfigManager
from config.conversion_steps import ConversionSteps
from evaluation.fitness_evaluator import FitnessEvaluator
from evaluation.performance_parser import PerformanceParser
from core.simulated_annealing import SimulatedAnnealing
from core.evolution_engine import EvolutionEngine
from core.sa_patch import SAPatch
from core.group_search_engine import GroupSearchEngine
from proxy_models.surrogate_optimizer_data1 import SurrogateOptimizerData1
import numpy as np




class GeneticOptimizer:


    def __init__(
        self,
        config_dir: str = "firstconfigs",
        population_size: int = 20,
        generations: int = 10,
        mutation_rate: float = 0.3,
        crossover_rate: float = 0.7,
        work_dir: str = None,
        output_dir: str = "gasacache_output",
        use_surrogate: bool = True,
        surrogate_confidence_threshold: float = 0.8,
        early_termination_threshold: float = 0.2,
        early_termination_confidence: float = 0.8,
        enable_online_training: bool = False,
        skip_evaluation_threshold: float = 0.2,
        force_evaluation_threshold: float = 0.05,
        enable_group_search: bool = False,
        grouped_dir: str = None,
        group_file: str = None,
    ):

        self.population_size = population_size
        self.generations = generations
        self.use_surrogate = use_surrogate
        self.early_termination_threshold = early_termination_threshold
        self.early_termination_confidence = early_termination_confidence
        self.enable_online_training = False
        self.skip_evaluation_threshold = skip_evaluation_threshold
        self.force_evaluation_threshold = (
            force_evaluation_threshold
        )


        self.enable_group_search = enable_group_search
        self.grouped_dir = grouped_dir
        self.group_file = group_file


        if work_dir is None:

            work_dir = os.path.expanduser("~")
        self.work_dir = work_dir


        self.output_base = os.path.join(work_dir, output_dir)
        os.makedirs(self.output_base, exist_ok=True)


        self.config_manager = ConfigManager(config_dir)
        self.cache_manager = CacheManager(
            os.path.join(self.output_base, "tested_configs_cache.json")
        )
        self.config_deduplication = ConfigDeduplication(self.cache_manager)
        self.conversion_steps = ConversionSteps()
        self.performance_parser = PerformanceParser()
        self.evolution_engine = EvolutionEngine(mutation_rate, crossover_rate)
        self.simulated_annealing = SimulatedAnnealing()
        self.sa_patch = SAPatch(self.simulated_annealing)


        self.group_search_engine = GroupSearchEngine(
            grouped_dir=self.grouped_dir, enable_group_search=self.enable_group_search
        )


        if self.use_surrogate:
            try:
                print("Initializing surrogate model...")


                proxy_models_path = os.path.join(
                    os.path.dirname(os.path.abspath(__file__)), "..", "proxy_models"
                )
                if proxy_models_path not in sys.path:
                    sys.path.insert(0, proxy_models_path)
                    print(f"Added proxy_models path: {proxy_models_path}")

                self.surrogate_optimizer = SurrogateOptimizerData1(
                    confidence_threshold=surrogate_confidence_threshold
                )


                try:
                    self.surrogate_optimizer.load_model("surrogate_model_new_dataset")
                    print("✓ Pre-trained surrogate model loaded successfully")
                    self.surrogate_available = True
                except Exception as model_load_error:
                    print(f"⚠️ Failed to load pre-trained model: {model_load_error}")

                    self.surrogate_available = False

            except Exception as e:
                print(f"⚠️ Surrogate model initialization failed: {e}")
                print("Will use traditional evaluation method")
                self.surrogate_available = False
        else:
            self.surrogate_available = False


        self.fitness_evaluator = FitnessEvaluator(
            self.config_manager,
            self.cache_manager,
            self.conversion_steps,
            self.performance_parser,
            work_dir,
            self.output_base,
        )


        self.best_config = None
        self.best_fitness = float("inf")
        self.best_generation = None
        self.best_index = None


        self.surrogate_predictions = 0
        self.actual_evaluations = 0


        self.current_generation = 0

    def optimize(self) -> Tuple[Dict[str, Any], float]:

        print("Starting GA+SA+Cache optimization...")
        print(f"Population size: {self.population_size}")
        print(f"Number of generations: {self.generations}")
        print(f"Number of variables: {len(self.config_manager.get_variables())}")

        self.cache_manager.print_cache_info()

        population = self.config_deduplication.create_initial_population(
            self.config_manager.get_initial_configs(), self.population_size
        )

        best_fitness_history = []

        for generation in range(self.generations):
            print(f"\n=== Generation {generation + 1} ===")

            self.current_generation = generation

            fitness_values = []
            early_termination_triggered = False

            for i, individual in enumerate(population):

                evaluation_strategy = self._should_use_surrogate(individual)

                if evaluation_strategy == "skip":
                    print(
                        f"Skip evaluation of individual {i} (predicted score too poor)"
                    )


                    fitness = 0.0
                    self.surrogate_predictions += 1
                elif evaluation_strategy == "surrogate":
                    print(
                        f"Using surrogate model prediction for individual {i} (no actual evaluation)"
                    )
                    fitness = self._evaluate_with_surrogate(individual)

                else:
                    print(f"Using actual evaluation for individual {i}")
                    try:
                        fitness = self.fitness_evaluator.evaluate_fitness(
                            individual, f"gen{generation}_ind{i}"
                        )
                    except Exception as eval_error:
                        print(
                            f"ERROR: Actual evaluation failed for individual {i}: {eval_error}"
                        )
                        fitness = 0.0
                    self.actual_evaluations += 1




                if not isinstance(fitness, (int, float)) or not np.isfinite(fitness):
                    fitness = 0.0
                fitness_values.append(fitness)


                if not np.isinf(fitness) and (
                    self.best_fitness == float("inf")
                    or abs(fitness) > abs(self.best_fitness)
                ):
                    self.best_fitness = fitness
                    self.best_config = copy.deepcopy(individual)
                    self.best_generation = generation
                    self.best_index = i
                    print(f"Found new best configuration! Fitness: {fitness:.4f}")


                if self._check_early_termination(fitness, self.best_fitness):
                    print(f"Early termination condition met, stopping optimization")
                    early_termination_triggered = True
                    break


            if early_termination_triggered:
                remaining_count = len(population) - len(fitness_values)
                if remaining_count > 0:
                    print(
                        f"Filling default fitness values for remaining {remaining_count} individuals"
                    )

                    default_fitness = (
                        self.best_fitness if self.best_fitness != float("inf") else 0.0
                    )
                    fitness_values.extend([default_fitness] * remaining_count)


            population, fitness_values = self.sa_patch.optimize_best_individual_only(
                population,
                fitness_values,
                generation,
                self.fitness_evaluator.evaluate_fitness,
                self.evolution_engine.mutate,
            )

            fitness_values = [
                f if isinstance(f, (int, float)) and np.isfinite(f) else 0.0
                for f in fitness_values
            ]


            try:
                candidates = [
                    (idx, val)
                    for idx, val in enumerate(fitness_values)
                    if isinstance(val, (int, float)) and not np.isinf(val)
                ]
                if candidates:
                    best_idx_after_sa, best_val_after_sa = max(
                        candidates, key=lambda x: abs(x[1])
                    )
                    if self.best_fitness == float("inf") or abs(
                        best_val_after_sa
                    ) > abs(self.best_fitness):
                        self.best_fitness = best_val_after_sa
                        self.best_config = copy.deepcopy(population[best_idx_after_sa])
                        self.best_generation = generation
                        self.best_index = best_idx_after_sa
                        print(
                            f"[SA] Updated global best from SA: {best_val_after_sa:.4f} (index {best_idx_after_sa})"
                        )
            except Exception as e:
                print(f"WARNING: Failed to sync SA best: {e}")


            finite_fitness_values = [f for f in fitness_values if not np.isinf(f)]
            if finite_fitness_values:
                best_fitness = max(
                    finite_fitness_values, key=abs
                )
                worst_fitness = min(
                    finite_fitness_values, key=abs
                )
                avg_fitness = statistics.mean(finite_fitness_values)
            else:

                best_fitness = 0.0
                worst_fitness = 0.0
                avg_fitness = 0.0
            best_fitness_history.append(best_fitness)

            print(f"Best fitness: {best_fitness:.4f}")
            print(f"Average fitness: {avg_fitness:.4f}")
            print(f"Worst fitness: {worst_fitness:.4f}")
            print(
                f"Total tested configurations: "
                f"{self.cache_manager.get_tested_configs_count()}"
            )


            if generation < self.generations - 1:
                population = self.evolution_engine.evolve_population(
                    population, fitness_values, self.population_size
                )


                if self.enable_group_search and self.group_file:
                    print(f"Applying group adjustment: {self.group_file}")
                    adjusted_population = []
                    for individual in population:
                        adjusted_individual = (
                            self.group_search_engine.adjust_config_by_group(
                                individual, self.group_file
                            )
                        )
                        adjusted_population.append(adjusted_individual)
                    population = adjusted_population


        if self.best_config:
            best_config_file = os.path.join(self.output_base, "best_config.json")
            self.config_manager.save_config(self.best_config, best_config_file)
            print(f"\nBest configuration saved to: {best_config_file}")


        self._save_optimization_history(best_fitness_history)


        self.cache_manager.save_cache()


        self.sa_patch.print_sa_statistics()

        return self.best_config, self.best_fitness

    def _save_optimization_history(self, best_fitness_history: List[float]):

        import json

        history_file = os.path.join(self.output_base, "optimization_history.json")
        history_data = {
            "best_fitness_history": best_fitness_history,
            "final_best_fitness": self.best_fitness,
            "parameters": {
                "population_size": self.population_size,
                "generations": self.generations,
                "mutation_rate": self.evolution_engine.mutation_rate,
                "crossover_rate": self.evolution_engine.crossover_rate,
            },
        }

        with open(history_file, "w", encoding="utf-8") as f:
            json.dump(history_data, f, indent=4, ensure_ascii=False)

        print(f"Optimization history saved to: {history_file}")

    def get_cache_info(self):

        return self.cache_manager.get_cache_info()

    def clear_cache(self):

        self.cache_manager.clear_cache()
        print("Cache cleared")

    def print_cache_details(self, limit: int = 10):

        self.cache_manager.print_cache_details(limit)

    def _extract_config_features(self, config: Dict[str, Any]) -> Dict[str, float]:

        if not self.surrogate_available:
            return {}

        try:

            double_count = 0
            float_count = 0
            half_count = 0
            total_variables = 0


            function_counts = {}

            for var in config.get("localVar", []):
                total_variables += 1
                var_type = var.get("type", "")
                function_name = var.get("function", "")

                if "double" in var_type:
                    double_count += 1
                elif "float" in var_type:
                    float_count += 1
                elif "half" in var_type:
                    half_count += 1

                if function_name not in function_counts:
                    function_counts[function_name] = 0
                function_counts[function_name] += 1


            features = {
                "double_ratio": double_count / max(total_variables, 1),
                "float_ratio": float_count / max(total_variables, 1),
                "half_ratio": half_count / max(total_variables, 1),
                "total_variables": total_variables,
                "function_diversity": len(function_counts) / max(total_variables, 1),
                "pointer_ratio": 0.0,
                "array_ratio": 0.0,
            }


            for func_name in [
                "dgemm",
                "dtrsm",
                "dlange",
                "dgemv",
                "copy_mat",
                "dgetrf_nopiv",
            ]:
                features[f"{func_name}_ratio"] = function_counts.get(
                    func_name, 0
                ) / max(total_variables, 1)

            return features

        except Exception as e:
            print(f"Feature extraction failed: {e}")
            return {}

    def _should_use_surrogate(self, config: Dict[str, Any]) -> str:


        if hasattr(self, "current_generation") and self.current_generation == 0:
            return "actual"

        if not self.surrogate_available or not self.use_surrogate:
            return "actual"

        try:
            features = self._extract_config_features(config)
            if not features:
                return "actual"


            predicted_fitness, confidence = self.surrogate_optimizer.predict_fitness(
                features
            )


            if not self._is_predicted_fitness_reasonable(predicted_fitness):
                print(
                    f"警告：代理模型预测分数 {predicted_fitness} 不合理，强制实际评估"
                )
                return "actual"


            if self.best_fitness != float("inf") and not np.isinf(self.best_fitness):

                predicted_abs = abs(predicted_fitness)
                best_abs = abs(self.best_fitness)

                if best_abs > 0:

                    relative_gap = (best_abs - predicted_abs) / best_abs
                    if relative_gap > self.skip_evaluation_threshold:
                        print(
                            f"DEBUG: Predicted fitness {predicted_fitness:.4f} is {relative_gap:.1%} worse than best {self.best_fitness:.4f}, skipping evaluation"
                        )
                        return "skip"


                    if (
                        relative_gap <= self.force_evaluation_threshold
                        and confidence >= self.surrogate_optimizer.confidence_threshold
                    ):
                        print(
                            f"DEBUG: Predicted fitness {predicted_fitness:.4f} is close to best {self.best_fitness:.4f} (gap: {relative_gap:.1%}), forcing actual evaluation"
                        )
                        return "actual"
                else:

                    print("DEBUG: Best fitness is 0, forcing actual evaluation")
                    return "actual"


                if confidence >= self.surrogate_optimizer.confidence_threshold:
                    print(
                        f"DEBUG: Using surrogate model - confidence {confidence:.3f} >= threshold {self.surrogate_optimizer.confidence_threshold}"
                    )
                    return "surrogate"
                else:
                    print(
                        f"DEBUG: Confidence {confidence:.3f} < threshold {self.surrogate_optimizer.confidence_threshold}, forcing actual evaluation"
                    )
                    return "actual"
            else:

                if confidence >= self.surrogate_optimizer.confidence_threshold:
                    print(
                        f"DEBUG: First generation, using surrogate model - confidence {confidence:.3f} >= threshold {self.surrogate_optimizer.confidence_threshold}"
                    )
                    return "surrogate"
                else:
                    print(
                        f"DEBUG: First generation, confidence {confidence:.3f} < threshold {self.surrogate_optimizer.confidence_threshold}, forcing actual evaluation"
                    )
                    return "actual"

        except Exception as e:
            print(f"Surrogate model prediction failed: {e}")
            return "actual"

    def _should_skip_evaluation(self, config: Dict[str, Any]) -> bool:

        if not self.surrogate_available or not self.use_surrogate:
            return False

        try:
            features = self._extract_config_features(config)
            if not features:
                return False


            predicted_fitness, confidence = self.surrogate_optimizer.predict_fitness(
                features
            )


            if self.best_fitness != float("inf") and not np.isinf(self.best_fitness):
                fitness_gap = predicted_fitness - self.best_fitness
                relative_gap = (
                    fitness_gap / abs(self.best_fitness)
                    if self.best_fitness != 0
                    else 0
                )


                skip_threshold = (
                    self.skip_evaluation_threshold
                )
                return relative_gap > skip_threshold

        except Exception as e:
            print(f"Skip evaluation check failed: {e}")
            return False

    def _should_force_actual_evaluation(self, config: Dict[str, Any]) -> bool:

        if not self.surrogate_available or not self.use_surrogate:
            return True

        try:
            features = self._extract_config_features(config)
            if not features:
                return True


            predicted_fitness, confidence = self.surrogate_optimizer.predict_fitness(
                features
            )


            if self.best_fitness != float("inf") and not np.isinf(self.best_fitness):
                fitness_gap = predicted_fitness - self.best_fitness
                relative_gap = (
                    fitness_gap / abs(self.best_fitness)
                    if self.best_fitness != 0
                    else 0
                )


                force_eval_threshold = (
                    self.force_evaluation_threshold
                )
                confidence_threshold = self.surrogate_optimizer.confidence_threshold

                return (
                    relative_gap <= force_eval_threshold
                    and confidence >= confidence_threshold
                )

        except Exception as e:
            print(f"Force evaluation check failed: {e}")
            return True

    def _evaluate_with_surrogate(self, config: Dict[str, Any]) -> float:

        try:
            features = self._extract_config_features(config)
            predicted_fitness, confidence = self.surrogate_optimizer.predict_fitness(
                features
            )


            if not self._is_predicted_fitness_reasonable(predicted_fitness):
                print(
                    f"WARNING: Surrogate model predicted unreasonable fitness {predicted_fitness}, falling back to actual evaluation"
                )

                try:
                    return self.fitness_evaluator.evaluate_fitness(
                        config, "surrogate_fallback_eval"
                    )
                except Exception as fallback_error:
                    print(f"ERROR: Fallback actual evaluation failed: {fallback_error}")
                    return 0.0


            print(
                f"INFO: Using surrogate model prediction: {predicted_fitness:.4f} (confidence: {confidence:.3f})"
            )




            self.surrogate_predictions += 1


            return predicted_fitness

        except Exception as e:
            print(f"ERROR: Surrogate model prediction failed: {e}")

            try:
                return self.fitness_evaluator.evaluate_fitness(
                    config, "surrogate_fallback_eval"
                )
            except Exception as fallback_error:
                print(f"ERROR: Fallback actual evaluation failed: {fallback_error}")
                return 0.0

    def _is_predicted_fitness_reasonable(self, predicted_fitness: float) -> bool:


        if not isinstance(predicted_fitness, (int, float)):
            return False


        if not np.isfinite(predicted_fitness):
            return False


        MAX_REASONABLE_FITNESS = 200.0

        if abs(predicted_fitness) > MAX_REASONABLE_FITNESS:
            print(
                f"警告：预测适应度值 {predicted_fitness} 绝对值超过200，丢弃预测，强制实际评估"
            )
            return False

        return True

    def _check_early_termination(
        self, current_fitness: float, best_fitness: float
    ) -> bool:

        if not self.surrogate_available or not self.use_surrogate:
            return False

        try:

            if np.isinf(current_fitness) or np.isinf(best_fitness):
                return False


            if best_fitness == 0:
                return False

            relative_gap = abs(current_fitness - best_fitness) / abs(best_fitness)


            if relative_gap <= self.early_termination_threshold:

                return True

        except Exception as e:
            print(f"Early termination check failed: {e}")

        return False

    def get_optimization_statistics(self) -> Dict[str, Any]:

        return {
            "surrogate_predictions": self.surrogate_predictions,
            "actual_evaluations": self.actual_evaluations,
            "surrogate_usage_ratio": self.surrogate_predictions
            / max(self.surrogate_predictions + self.actual_evaluations, 1),
            "cache_hits": self.cache_manager.get_tested_configs_count(),
            "best_fitness": self.best_fitness,
            "best_generation": self.best_generation,
        }

    def clear_cache(self):

        self.cache_manager.clear_cache()

    def print_cache_details(self, limit: int = 5):

        self.cache_manager.print_config_details(limit)

    def get_sa_statistics(self):
        
        return self.sa_patch.get_sa_statistics()
