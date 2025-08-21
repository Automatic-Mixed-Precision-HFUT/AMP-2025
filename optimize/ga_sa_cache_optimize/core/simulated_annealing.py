

import random
import copy
import math
from typing import List, Dict, Any, Tuple


class SimulatedAnnealing:


    def __init__(self, steps: int = 5, T0: float = 1.0, cooling: float = 0.9):

        self.steps = steps
        self.T0 = T0
        self.cooling = cooling

    def optimize_population(
        self,
        population: List[Dict[str, Any]],
        fitness_values: List[float],
        generation: int,
        evaluate_fitness_func,
        mutate_func,
    ) -> Tuple[List[Dict[str, Any]], List[float]]:

        for i, individual in enumerate(population):
            improved, improved_fitness = self.local_search(
                individual,
                f"gen{generation}_ind{i}_sa",
                evaluate_fitness_func,
                mutate_func,
            )
            if improved_fitness < fitness_values[i]:
                population[i] = improved
                fitness_values[i] = improved_fitness
                print(
                    f"SA improved individual {i}, "
                    f"new fitness: {-improved_fitness:.4f}"
                )

        return population, fitness_values

    def local_search(
        self,
        individual: Dict[str, Any],
        individual_id_prefix: str,
        evaluate_fitness_func,
        mutate_func,
    ) -> Tuple[Dict[str, Any], float]:
        
        current = copy.deepcopy(individual)
        current_fitness = evaluate_fitness_func(current, f"{individual_id_prefix}_0")
        best = copy.deepcopy(current)
        best_fitness = current_fitness
        T = self.T0

        for step in range(1, self.steps + 1):
            neighbor = mutate_func(current)
            neighbor_fitness = evaluate_fitness_func(
                neighbor, f"{individual_id_prefix}_{step}"
            )
            delta = neighbor_fitness - current_fitness

            if delta < 0 or random.random() < math.exp(-delta / T):
                current = neighbor
                current_fitness = neighbor_fitness
                if current_fitness < best_fitness:
                    best = copy.deepcopy(current)
                    best_fitness = current_fitness

            T *= self.cooling

        return best, best_fitness
