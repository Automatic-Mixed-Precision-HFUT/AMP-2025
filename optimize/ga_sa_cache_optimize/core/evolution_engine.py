

import random
import copy
from typing import List, Dict, Any, Tuple


class EvolutionEngine:


    def __init__(self, mutation_rate: float = 0.3, crossover_rate: float = 0.7):

        self.mutation_rate = mutation_rate
        self.crossover_rate = crossover_rate
        self.scalar_types = ["double", "float", "half"]
        self.pointer_types = ["double*", "float*", "half*"]

    def tournament_selection(
        self,
        population: List[Dict[str, Any]],
        fitness_values: List[float],
        tournament_size: int = 3,
    ) -> Dict[str, Any]:


        if len(population) != len(fitness_values):
            raise ValueError(
                f"Population size ({len(population)}) and fitness values size "
                f"({len(fitness_values)}) must be equal"
            )


        tournament_size = min(tournament_size, len(population))

        tournament_indices = random.sample(range(len(population)), tournament_size)
        tournament_fitness = [fitness_values[i] for i in tournament_indices]


        winner_index = tournament_indices[
            tournament_fitness.index(min(tournament_fitness))
        ]
        return copy.deepcopy(population[winner_index])

    def crossover(
        self, parent1: Dict[str, Any], parent2: Dict[str, Any]
    ) -> Tuple[Dict[str, Any], Dict[str, Any]]:

        if random.random() > self.crossover_rate:
            return copy.deepcopy(parent1), copy.deepcopy(parent2)

        child1 = copy.deepcopy(parent1)
        child2 = copy.deepcopy(parent2)


        for i, var in enumerate(child1.get("localVar", [])):
            if random.random() < 0.5:

                child1["localVar"][i]["type"] = parent2["localVar"][i]["type"]
                child2["localVar"][i]["type"] = parent1["localVar"][i]["type"]

        return child1, child2

    def mutate(self, individual: Dict[str, Any]) -> Dict[str, Any]:

        if random.random() > self.mutation_rate:
            return individual

        mutated = copy.deepcopy(individual)
        for var in mutated.get("localVar", []):
            if random.random() < 0.1:
                if var["type"].endswith("*"):
                    var["type"] = random.choice(self.pointer_types)
                else:
                    var["type"] = random.choice(self.scalar_types)

        return mutated

    def evolve_population(
        self,
        population: List[Dict[str, Any]],
        fitness_values: List[float],
        population_size: int,
    ) -> List[Dict[str, Any]]:
        
        new_population = []


        elite_size = max(1, population_size // 5)
        elite_indices = sorted(
            range(len(fitness_values)), key=lambda i: fitness_values[i]
        )[:elite_size]

        for idx in elite_indices:
            new_population.append(copy.deepcopy(population[idx]))


        while len(new_population) < population_size:

            parent1 = self.tournament_selection(population, fitness_values)
            parent2 = self.tournament_selection(population, fitness_values)


            child1, child2 = self.crossover(parent1, parent2)


            child1 = self.mutate(child1)
            child2 = self.mutate(child2)

            new_population.extend([child1, child2])


        return new_population[:population_size]
