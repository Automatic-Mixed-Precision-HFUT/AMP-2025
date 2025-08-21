import random
import copy
from typing import List, Dict, Any


class ConfigDeduplication:

    def __init__(self, cache_manager):

        self.cache_manager = cache_manager
        self.scalar_types = ["double", "float", "half"]
        self.pointer_types = ["double*", "float*", "half*"]

    def create_random_individual(
        self, initial_configs: List[Dict[str, Any]]
    ) -> Dict[str, Any]:

        max_attempts = 1000

        for attempt in range(max_attempts):
            template_config = random.choice(initial_configs)
            individual = copy.deepcopy(template_config)

            for var in individual.get("localVar", []):
                if var["type"].endswith("*"):
                    var["type"] = random.choice(self.pointer_types)
                else:
                    var["type"] = random.choice(self.scalar_types)

            if not self.cache_manager.is_config_tested(individual):
                return individual
        template_config = random.choice(initial_configs)
        individual = copy.deepcopy(template_config)

        for var in individual.get("localVar", []):
            if random.random() < 0.3:
                if var["type"].endswith("*"):
                    current_type = var["type"]
                    other_types = [t for t in self.pointer_types if t != current_type]
                    if other_types:
                        var["type"] = random.choice(other_types)
                else:
                    current_type = var["type"]
                    other_types = [t for t in self.scalar_types if t != current_type]
                    if other_types:
                        var["type"] = random.choice(other_types)

        return individual

    def create_initial_population(
        self, initial_configs: List[Dict[str, Any]], population_size: int
    ) -> List[Dict[str, Any]]:

        population = []

        for i, config in enumerate(initial_configs):
            if not self.cache_manager.is_config_tested(config):
                population.append(copy.deepcopy(config))

                self.cache_manager.mark_config_as_tested(config)
                print(f"Added initial configuration {i+1} to population")
            else:
                print(f"Skipping duplicate initial configuration {i+1}")

        remaining_size = population_size - len(population)
        for i in range(remaining_size):
            individual = self.create_random_individual(initial_configs)
            population.append(individual)

            self.cache_manager.mark_config_as_tested(individual)
            print(f"Added random individual {i+1} to population")

        print(f"Initial population size: {len(population)}")
        return population

    def evolve_population_with_dedup(
        self,
        population: List[Dict[str, Any]],
        fitness_values: List[float],
        population_size: int,
        mutation_rate: float,
        crossover_rate: float,
        tournament_selection_func,
        crossover_func,
        mutate_func,
    ) -> List[Dict[str, Any]]:

        new_population = []

        elite_size = max(1, population_size // 5)
        elite_indices = sorted(
            range(len(fitness_values)), key=lambda i: fitness_values[i]
        )[:elite_size]

        for idx in elite_indices:
            new_population.append(copy.deepcopy(population[idx]))

        max_attempts = 1000
        attempts = 0

        while len(new_population) < population_size and attempts < max_attempts:
            attempts += 1

            parent1 = tournament_selection_func(population, fitness_values)
            parent2 = tournament_selection_func(population, fitness_values)

            child1, child2 = crossover_func(parent1, parent2, crossover_rate)

            child1 = mutate_func(child1, mutation_rate)
            child2 = mutate_func(child2, mutation_rate)

            valid_children = []
            if not self.cache_manager.is_config_tested(child1):
                valid_children.append(child1)
                self.cache_manager.mark_config_as_tested(child1)

            if not self.cache_manager.is_config_tested(child2):
                valid_children.append(child2)
                self.cache_manager.mark_config_as_tested(child2)

            new_population.extend(valid_children)

        while len(new_population) < population_size:
            random_individual = self.create_random_individual(
                [pop[0] for pop in population]
            )
            if not self.cache_manager.is_config_tested(random_individual):
                new_population.append(random_individual)
                self.cache_manager.mark_config_as_tested(random_individual)

        return new_population[:population_size]
