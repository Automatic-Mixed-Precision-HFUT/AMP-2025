

import copy
from typing import List, Dict, Any, Tuple
from .simulated_annealing import SimulatedAnnealing


class SAPatch:


    def __init__(self, sa_optimizer: SimulatedAnnealing = None):

        self.sa_optimizer = sa_optimizer or SimulatedAnnealing()
        self.best_individual_history = []
        self.sa_improvement_history = []

    def optimize_best_individual_only(
        self,
        population: List[Dict[str, Any]],
        fitness_values: List[float],
        generation: int,
        evaluate_fitness_func,
        mutate_func,
    ) -> Tuple[List[Dict[str, Any]], List[float]]:


        best_index = self._find_best_individual(fitness_values)
        best_individual = population[best_index]
        best_fitness = fitness_values[best_index]

        print(f"\n=== SA Optimization for Generation {generation} ===")
        print(f"Best individual index: {best_index}")
        print(f"Best fitness before SA: {best_fitness:.4f}")


        original_best = copy.deepcopy(best_individual)
        original_fitness = best_fitness


        improved_individual, improved_fitness = self.sa_optimizer.local_search(
            best_individual,
            f"gen{generation}_best_sa",
            evaluate_fitness_func,
            mutate_func,
        )


        if abs(improved_fitness) > abs(best_fitness):

            population[best_index] = improved_individual
            fitness_values[best_index] = improved_fitness

            improvement = abs(improved_fitness) - abs(best_fitness)
            print(f"SA improved best individual!")
            print(f"Fitness improvement: {improvement:.4f}")
            print(f"New best fitness: {improved_fitness:.4f}")


            self.sa_improvement_history.append(
                {
                    "generation": generation,
                    "best_index": best_index,
                    "original_fitness": original_fitness,
                    "improved_fitness": improved_fitness,
                    "improvement": improvement,
                    "improvement_percentage": (
                        (improvement / abs(original_fitness)) * 100
                        if abs(original_fitness) > 0
                        else 0.0
                    ),
                }
            )
        else:
            print(f"SA did not improve best individual")
            print(f"Best fitness remains: {best_fitness:.4f}")


            self.sa_improvement_history.append(
                {
                    "generation": generation,
                    "best_index": best_index,
                    "original_fitness": original_fitness,
                    "improved_fitness": best_fitness,
                    "improvement": 0.0,
                    "improvement_percentage": 0.0,
                }
            )


        self.best_individual_history.append(
            {
                "generation": generation,
                "best_index": best_index,
                "fitness": fitness_values[best_index],
                "was_improved_by_sa": abs(improved_fitness) > abs(best_fitness),
            }
        )

        print("=== SA Optimization Complete ===\n")

        return population, fitness_values

    def _find_best_individual(self, fitness_values: List[float]) -> int:

        return fitness_values.index(max(fitness_values, key=abs))

    def get_sa_statistics(self) -> Dict[str, Any]:

        if not self.sa_improvement_history:
            return {
                "total_generations": 0,
                "improvements_count": 0,
                "improvement_rate": 0.0,
                "average_improvement": 0.0,
                "total_improvement": 0.0,
            }

        total_generations = len(self.sa_improvement_history)
        improvements = [h for h in self.sa_improvement_history if h["improvement"] > 0]
        improvements_count = len(improvements)

        if improvements:
            total_improvement = sum(h["improvement"] for h in improvements)
            average_improvement = total_improvement / improvements_count
        else:
            total_improvement = 0.0
            average_improvement = 0.0

        improvement_rate = (improvements_count / total_generations) * 100

        return {
            "total_generations": total_generations,
            "improvements_count": improvements_count,
            "improvement_rate": improvement_rate,
            "average_improvement": average_improvement,
            "total_improvement": total_improvement,
            "improvement_history": self.sa_improvement_history,
            "best_individual_history": self.best_individual_history,
        }

    def print_sa_statistics(self):

        stats = self.get_sa_statistics()

        print("\n" + "=" * 50)
        print("SA OPTIMIZATION STATISTICS")
        print("=" * 50)
        print(f"Total generations: {stats['total_generations']}")
        print(f"Improvements count: {stats['improvements_count']}")
        print(f"Improvement rate: {stats['improvement_rate']:.2f}%")
        print(f"Average improvement: {stats['average_improvement']:.4f}")
        print(f"Total improvement: {stats['total_improvement']:.4f}")

        if stats["improvement_history"]:
            print("\nImprovement details:")
            for record in stats["improvement_history"]:
                if record["improvement"] > 0:
                    print(
                        f"  Gen {record['generation']}: +{record['improvement']:.4f} "
                        f"({record['improvement_percentage']:.2f}%)"
                    )

        print("=" * 50)

    def reset_history(self):
        
        self.best_individual_history.clear()
        self.sa_improvement_history.clear()
