




import os
import json
import copy
from typing import Dict, Any, List, Tuple
from core.genetic_optimizer import GeneticOptimizer
from core.group_search_engine import GroupSearchEngine


class SimpleGroupOptimizer:


    def __init__(
        self,
        config_dir: str,
        work_dir: str,
        population_size: int = 5,
        generations: int = 5,
        enable_group_search: bool = False,
        grouped_dir: str = None,
        use_surrogate: bool = True,
        surrogate_confidence_threshold: float = 0.8,
        enable_online_training: bool = False,
        skip_evaluation_threshold: float = 0.2,
        force_evaluation_threshold: float = 0.05,
    ):

        self.config_dir = config_dir
        self.work_dir = work_dir
        self.population_size = population_size
        self.generations = generations
        self.enable_group_search = enable_group_search
        self.grouped_dir = grouped_dir
        self.use_surrogate = use_surrogate
        self.surrogate_confidence_threshold = surrogate_confidence_threshold
        self.enable_online_training = enable_online_training
        self.skip_evaluation_threshold = skip_evaluation_threshold
        self.force_evaluation_threshold = force_evaluation_threshold


        if self.enable_group_search and self.grouped_dir:
            self.group_search_engine = GroupSearchEngine(
                grouped_dir=self.grouped_dir,
                enable_group_search=self.enable_group_search,
            )
        else:
            self.group_search_engine = None

    def optimize(self) -> Tuple[Dict[str, Any], float]:

        if self.enable_group_search:
            return self._run_group_search()
        else:
            return self._run_single_optimization()

    def _run_group_search(self) -> Tuple[Dict[str, Any], float]:

        print("\n=== Starting Simplified Group Search Mode ===")
        if self.group_search_engine:
            self.group_search_engine.print_group_info()

        best_configs = []


        for group_file in self.group_search_engine.get_group_files():
            print(f"\n=== Starting Group Search: {group_file} ===")


            sct = self.surrogate_confidence_threshold
            optimizer_params = {
                "config_dir": self.config_dir,
                "work_dir": self.work_dir,
                "population_size": 3,
                "generations": 3,
                "use_surrogate": self.use_surrogate,
                "surrogate_confidence_threshold": sct,
                "enable_online_training": self.enable_online_training,
                "skip_evaluation_threshold": self.skip_evaluation_threshold,
                "force_evaluation_threshold": self.force_evaluation_threshold,
                "enable_group_search": True,
                "grouped_dir": self.grouped_dir,
                "group_file": group_file,
            }

            try:

                optimizer = SimpleGeneticOptimizer(**optimizer_params)


                best_config, best_fitness = optimizer.optimize()

                if best_config:
                    best_configs.append(
                        {
                            "group_file": group_file,
                            "config": best_config,
                            "fitness": best_fitness,
                        }
                    )
                    print(
                        f"✓ {group_file} search completed, "
                        f"best fitness: {best_fitness:.4f}"
                    )
                else:
                    prefix = f"⚠️ {group_file} search found no valid "
                    msg = prefix + "configuration"
                    print(msg)

            except Exception as e:
                print(f"⚠️ {group_file} search failed: {e}")
                continue


        if best_configs:

            configs_dir = os.path.join(self.work_dir, "optimize", "configs")
            self._save_group_search_results(best_configs, configs_dir)


            best_result = max(best_configs, key=lambda x: abs(x["fitness"]))
            return best_result["config"], best_result["fitness"]
        else:
            return None, 0.0

    def _run_single_optimization(self) -> Tuple[Dict[str, Any], float]:

        optimizer_params = {
            "config_dir": self.config_dir,
            "work_dir": self.work_dir,
            "population_size": self.population_size,
            "generations": self.generations,
            "use_surrogate": True,
            "enable_group_search": False,
        }

        optimizer = GeneticOptimizer(**optimizer_params)
        return optimizer.optimize()

    def _save_group_search_results(
        self, best_configs: List[Dict[str, Any]], configs_dir: str
    ):

        if not best_configs:
            return

        os.makedirs(configs_dir, exist_ok=True)

        for result in best_configs:
            group_name = result["group_file"].replace(".json", "")
            config_file = os.path.join(
                configs_dir, f"group_search_{group_name}_best.json"
            )

            try:
                with open(config_file, "w", encoding="utf-8") as f:
                    json.dump(
                        result["config"],
                        f,
                        indent=4,
                        ensure_ascii=False,
                    )
                print("✓ Saved group search result: ")
                print(config_file)
            except Exception as e:
                err_prefix = "⚠️ Failed to save group search result "
                print(err_prefix)
                print(f"{config_file}: {e}")


class SimpleGeneticOptimizer(GeneticOptimizer):


    def __init__(self, **kwargs):


        super().__init__(**kwargs)

    def optimize(self) -> Tuple[Dict[str, Any], float]:
        
        print("Starting Simple GA optimization (with surrogate, no SA)...")
        print(f"Population size: {self.population_size}")
        print(f"Number of generations: {self.generations}")
        num_vars = len(self.config_manager.get_variables())
        print(f"Number of variables: {num_vars}")


        population = self.config_deduplication.create_initial_population(
            self.config_manager.get_initial_configs(), self.population_size
        )

        best_fitness = float("-inf")
        best_config = None


        surrogate_predictions = 0
        actual_evaluations = 0

        for generation in range(self.generations):
            print(f"\n=== Generation {generation + 1} ===")


            self.current_generation = generation


            fitness_values = []

            for i, individual in enumerate(population):

                individual_for_eval = individual
                if (
                    self.enable_group_search
                    and hasattr(self, "group_search_engine")
                    and self.group_file
                ):
                    individual_for_eval = (
                        self.group_search_engine.adjust_config_by_group(
                            individual, self.group_file
                        )
                    )


                evaluation_strategy = self._should_use_surrogate(individual_for_eval)

                if evaluation_strategy == "skip":
                    print(
                        f"Skip evaluation of individual {i} "
                        f"(predicted score too poor)"
                    )
                    fitness = 0.0
                    surrogate_predictions += 1
                elif evaluation_strategy == "surrogate":
                    print(f"Using surrogate model to evaluate individual {i}")
                    fitness = self._evaluate_with_surrogate(individual_for_eval)
                    surrogate_predictions += 1
                else:
                    print(f"Using actual evaluation for individual {i}")
                    fitness = self.fitness_evaluator.evaluate_fitness(
                        individual_for_eval, f"gen{generation}_ind{i}"
                    )
                    actual_evaluations += 1


                    if self.surrogate_available:
                        features = self._extract_config_features(individual_for_eval)
                        if features:
                            self.surrogate_optimizer.add_training_data(
                                features, fitness
                            )

                fitness_values.append(fitness)


                if fitness > best_fitness:
                    best_fitness = fitness
                    best_config = copy.deepcopy(individual_for_eval)


            if generation < self.generations - 1:
                from core.evolution_engine import EvolutionEngine

                evolution_engine = EvolutionEngine(
                    mutation_rate=0.1, crossover_rate=0.8
                )
                population = evolution_engine.evolve_population(
                    population, fitness_values, self.population_size
                )

        print(f"\nOptimization completed! Best fitness: {best_fitness:.4f}")
        print(f"Surrogate model predictions: {surrogate_predictions}")
        print(f"Actual evaluations: {actual_evaluations}")
        return best_config, best_fitness
