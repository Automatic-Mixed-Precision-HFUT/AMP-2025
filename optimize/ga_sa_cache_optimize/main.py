import sys
import os


sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from core.genetic_optimizer import GeneticOptimizer
from core.simple_group_optimizer import SimpleGroupOptimizer


WORK_DIR = "/root/workspace/AMP"


GA_SA_IMPROVED_DIR = os.path.join(WORK_DIR, "optimize")


INITIAL_LL_PATH = os.path.join(GA_SA_IMPROVED_DIR, "hpllink.ll")

LIBMIX_PATH = os.path.join(GA_SA_IMPROVED_DIR, "libMix.so.17")


OUTPUT_DIR = "GSC_improved/gasacache_output"


CONFIG_DIR = os.path.join(GA_SA_IMPROVED_DIR, "configs")


def main():

    if len(sys.argv) < 2:
        print("Usage: python main.py <config_dir> [population_size]")
        print("       [generations]")
        print("    [--clear-cache] [--show-details] [--export-details]")
        print("    [--no-surrogate] [--surrogate-confidence <threshold>]")
        print(
            "    [--enable-online-training] "
            "[--early-termination-threshold <threshold>]"
        )
        print("    [--early-termination-confidence <confidence>]")
        print("    [--skip-evaluation-threshold <threshold>]")
        print("    [--force-evaluation-threshold <threshold>]")
        print("    [--enable-group-search]")
        print("Example: python main.py firstconfigs 7 10")
        print("Example: python main.py firstconfigs 7 10 --clear-cache")
        print("Example: python main.py firstconfigs 7 10 --show-details")
        print("Example: python main.py firstconfigs 7 10 --export-details")
        print("Example: python main.py firstconfigs 7 10 --no-surrogate")
        print("Example: python main.py firstconfigs 7 10 " "--surrogate-confidence 0.9")
        print("Example: python main.py firstconfigs 7 10 " "--enable-online-training")
        print(
            "Example: python main.py firstconfigs 7 10 "
            "--early-termination-threshold 0.3 "
            "--early-termination-confidence 0.9"
        )
        print(
            "Example: python main.py firstconfigs 7 10 "
            "--skip-evaluation-threshold 0.3 "
            "--force-evaluation-threshold 0.1"
        )
        print("Example: python main.py firstconfigs 7 10 --enable-group-search")
        sys.exit(1)

    positional_args = []
    option_args = []

    i = 1
    while i < len(sys.argv):
        if sys.argv[i].startswith("--"):

            if i + 1 < len(sys.argv) and not sys.argv[i + 1].startswith("--"):

                option_args.extend([sys.argv[i], sys.argv[i + 1]])
                i += 2
            else:

                option_args.append(sys.argv[i])
                i += 1
        else:

            positional_args.append(sys.argv[i])
            i += 1

    if len(positional_args) < 1:
        print("Error: Configuration directory must be specified")
        sys.exit(1)

    config_dir = positional_args[0]
    population_size = int(positional_args[1]) if len(positional_args) > 1 else 7
    generations = int(positional_args[2]) if len(positional_args) > 2 else 10

    clear_cache = "--clear-cache" in option_args
    show_details = "--show-details" in option_args
    export_details = "--export-details" in option_args
    use_surrogate = "--no-surrogate" not in option_args
    enable_online_training = "--enable-online-training" in option_args
    enable_group_search = "--enable-group-search" in option_args

    surrogate_confidence = 0.81
    if "--surrogate-confidence" in option_args:
        try:
            idx = option_args.index("--surrogate-confidence")
            if idx + 1 < len(option_args):
                surrogate_confidence = float(option_args[idx + 1])
        except (ValueError, IndexError):
            print("Warning: Invalid confidence threshold, using default value 0.8")

    early_termination_threshold = 0.2
    early_termination_confidence = 0.8

    if "--early-termination-threshold" in option_args:
        try:
            idx = option_args.index("--early-termination-threshold")
            if idx + 1 < len(option_args):
                early_termination_threshold = float(option_args[idx + 1])
        except (ValueError, IndexError):
            print(
                "Warning: Invalid early termination threshold, using default value 0.2"
            )

    if "--early-termination-confidence" in option_args:
        try:
            idx = option_args.index("--early-termination-confidence")
            if idx + 1 < len(option_args):
                early_termination_confidence = float(option_args[idx + 1])
        except (ValueError, IndexError):
            print(
                "Warning: Invalid early termination confidence, using default value 0.8"
            )

    skip_evaluation_threshold = 0.2
    force_evaluation_threshold = 0.05

    if "--skip-evaluation-threshold" in option_args:
        try:
            idx = option_args.index("--skip-evaluation-threshold")
            if idx + 1 < len(option_args):
                skip_evaluation_threshold = float(option_args[idx + 1])
        except (ValueError, IndexError):
            print("Warning: Invalid skip evaluation threshold, using default value 0.2")

    if "--force-evaluation-threshold" in option_args:
        try:
            idx = option_args.index("--force-evaluation-threshold")
            if idx + 1 < len(option_args):
                force_evaluation_threshold = float(option_args[idx + 1])
        except (ValueError, IndexError):
            print(
                "Warning: Invalid force evaluation threshold, using default value 0.05"
            )

    if not enable_group_search:
        print(f"Surrogate Model: {'Enabled' if use_surrogate else 'Disabled'}")
        if use_surrogate:
            print(f"Confidence Threshold: {surrogate_confidence}")
            print(
                f"Online Training: {'Enabled' if enable_online_training else 'Disabled'}"
            )
            print(
                f"Early Termination Threshold: {early_termination_threshold} "
                f"({early_termination_threshold*100:.0f}%)"
            )
            print(f"Early Termination Confidence: {early_termination_confidence}")
            print(
                f"Skip Evaluation Threshold: {skip_evaluation_threshold} ({skip_evaluation_threshold*100:.0f}%)"
            )
            print(
                f"Force Evaluation Threshold: {force_evaluation_threshold} ({force_evaluation_threshold*100:.0f}%)"
            )

    os.environ["GA_SA_WORK_DIR"] = WORK_DIR
    os.environ["GA_SA_GA_SA_IMPROVED_DIR"] = GA_SA_IMPROVED_DIR
    os.environ["GA_SA_INITIAL_LL"] = INITIAL_LL_PATH
    os.environ["GA_SA_LIBMIX_PATH"] = LIBMIX_PATH
    os.environ["GA_SA_OUTPUT_DIR"] = OUTPUT_DIR
    os.environ["GA_SA_CONFIG_DIR"] = CONFIG_DIR

    grouped_dir = os.path.join(
        GA_SA_IMPROVED_DIR, "Loop-Hierarchy Partitioning", "grouped"
    )

    if enable_group_search:
        print("\n=== Starting Simplified Group Search Mode ===")

        simple_optimizer = SimpleGroupOptimizer(
            config_dir=config_dir,
            work_dir=WORK_DIR,
            population_size=population_size,
            generations=generations,
            enable_group_search=True,
            grouped_dir=grouped_dir,
            use_surrogate=use_surrogate,
            surrogate_confidence_threshold=surrogate_confidence,
            enable_online_training=enable_online_training,
            skip_evaluation_threshold=skip_evaluation_threshold,
            force_evaluation_threshold=force_evaluation_threshold,
        )
        best_config, best_fitness = simple_optimizer.optimize()
    else:

        optimizer = GeneticOptimizer(
            config_dir=config_dir,
            population_size=population_size,
            generations=generations,
            work_dir=WORK_DIR,
            output_dir=OUTPUT_DIR,
            use_surrogate=use_surrogate,
            surrogate_confidence_threshold=surrogate_confidence,
            early_termination_threshold=early_termination_threshold,
            early_termination_confidence=early_termination_confidence,
            enable_online_training=enable_online_training,
            skip_evaluation_threshold=skip_evaluation_threshold,
            force_evaluation_threshold=force_evaluation_threshold,
            enable_group_search=False,
            grouped_dir=grouped_dir,
        )

        if clear_cache:
            optimizer.clear_cache()

        cache_info = optimizer.get_cache_info()
        print("\n=== Cache info ===")
        print(f"Cache file: {cache_info['cache_file']}")
        print(f"Cache file exists: {cache_info['cache_exists']}")
        print(
            "Number of tested configurations: {}".format(
                cache_info["tested_configs_count"]
            )
        )
        if cache_info["cache_exists"]:
            print(f"Cache file size: {cache_info['cache_size_bytes']} bytes")
        print("================\n")

        if show_details:
            optimizer.print_cache_details(limit=10)

        if export_details:

            print("Export functionality not yet implemented")
            return

        best_config, best_fitness = optimizer.optimize()

    print("\nOptimization complete!")
    print(f"Best fitness: {-best_fitness:.4f}")

    if enable_group_search:
        print("Group Search Mode Completed!")
        print("Results saved to configs/ directory")
    else:
        print(
            "Best configuration saved to: {}".format(
                os.path.join(optimizer.output_base, "best_config.json")
            )
        )

        final_cache_info = optimizer.get_cache_info()
        print(
            "Final number of tested configurations: {}".format(
                final_cache_info["tested_configs_count"]
            )
        )

        if use_surrogate:
            stats = optimizer.get_optimization_statistics()
            print("\n=== Optimization Statistics ===")
            print(f"Surrogate Model Predictions: {stats['surrogate_predictions']}")
            print(f"Actual Evaluations: {stats['actual_evaluations']}")
            print(f"Surrogate Model Usage Ratio: {stats['surrogate_usage_ratio']:.2%}")
            print(f"Cache Hits: {stats['cache_hits']}")
            print(f"Best Fitness: {-stats['best_fitness']:.4f}")
            print(f"Best Generation: {stats['best_generation']}")

        print("\nTip: Use --show-details to view details of tested configurations")
        print("Tip: Use --export-details to export configuration details to file")
        print("Tip: Use --no-surrogate to disable surrogate model")
        print("Tip: Use --surrogate-confidence <value> to adjust confidence threshold")
        print("Tip: Use --skip-evaluation-threshold <value> to adjust skip threshold")
        print(
            "Tip: Use --force-evaluation-threshold <value> to adjust force evaluation threshold"
        )


if __name__ == "__main__":
    main()
