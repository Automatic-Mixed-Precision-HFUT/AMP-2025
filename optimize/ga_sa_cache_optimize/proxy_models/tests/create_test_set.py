




import os
import sys
import json
import numpy as np
import random
from pathlib import Path
from typing import List, Dict, Any


sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from proxy_models.prepare_train_data1 import TrainData1Preprocessor


def load_all_configs(train_data1_dir: str) -> List[Dict[str, Any]]:

    print(f"加载train_data1数据: {train_data1_dir}")

    preprocessor = TrainData1Preprocessor(train_data1_dir)
    all_data = preprocessor.load_all_data()


    valid_configs = []
    for data in all_data:
        fitness = data.get("final_score", 0.0)
        if np.isfinite(fitness):
            valid_configs.append(data)

    print(f"总共找到 {len(all_data)} 个样本")
    print(f"有效配置数量: {len(valid_configs)}")

    return valid_configs


def create_test_set(
    configs: List[Dict[str, Any]],
    num_samples: int = 1000,
    output_file: str = "data/test_configs_1000.json",
) -> None:

    print(f"创建包含 {num_samples} 个样本的测试集...")


    if len(configs) <= num_samples:
        selected_configs = configs
        print(f"配置总数少于请求数量，使用所有 {len(configs)} 个配置")
    else:
        selected_configs = random.sample(configs, num_samples)
        print(f"随机选择了 {num_samples} 个配置")


    os.makedirs(os.path.dirname(output_file), exist_ok=True)

    with open(output_file, "w", encoding="utf-8") as f:
        json.dump(selected_configs, f, indent=2, ensure_ascii=False)

    print(f"测试集已保存到: {output_file}")


    fitness_values = [config.get("final_score", 0.0) for config in selected_configs]
    print(f"\n=== 测试集统计信息 ===")
    print(f"样本数量: {len(selected_configs)}")
    print(f"适应度值范围: [{min(fitness_values):.4f}, {max(fitness_values):.4f}]")
    print(f"适应度值均值: {np.mean(fitness_values):.4f}")
    print(f"适应度值标准差: {np.std(fitness_values):.4f}")


    var_counts = []
    for config in selected_configs:
        local_vars = config.get("config", {}).get("localVar", [])
        var_counts.append(len(local_vars))

    print(f"局部变量数量范围: [{min(var_counts)}, {max(var_counts)}]")
    print(f"局部变量数量均值: {np.mean(var_counts):.2f}")


def create_test_set_with_predictions(
    configs: List[Dict[str, Any]],
    num_samples: int = 1000,
    output_file: str = "data/test_configs_with_predictions_1000.json",
) -> None:

    print(f"创建包含预测结果的测试集...")


    if len(configs) <= num_samples:
        selected_configs = configs
    else:
        selected_configs = random.sample(configs, num_samples)


    from proxy_models.direct_predict import predict_config_fitness


    test_set_with_predictions = []

    for i, config in enumerate(selected_configs):
        if i % 50 == 0:
            print(f"处理进度: {i}/{len(selected_configs)}")

        try:

            temp_config_file = f"temp_config_{i}.json"
            with open(temp_config_file, "w", encoding="utf-8") as f:
                json.dump(config.get("config", {}), f, indent=2, ensure_ascii=False)


            predicted_fitness = predict_config_fitness(temp_config_file)


            os.remove(temp_config_file)


            config_with_prediction = {
                "config": config.get("config", {}),
                "actual_fitness": float(config.get("final_score", 0.0)),
                "predicted_fitness": float(predicted_fitness),
                "prediction_error": float(
                    abs(predicted_fitness - config.get("final_score", 0.0))
                ),
                "relative_error": float(
                    abs(predicted_fitness - config.get("final_score", 0.0))
                    / abs(config.get("final_score", 0.0))
                    if config.get("final_score", 0.0) != 0
                    else float("inf")
                ),
            }

            test_set_with_predictions.append(config_with_prediction)

        except Exception as e:
            print(f"配置 {i} 预测失败: {e}")
            continue


    os.makedirs(os.path.dirname(output_file), exist_ok=True)

    with open(output_file, "w", encoding="utf-8") as f:
        json.dump(test_set_with_predictions, f, indent=2, ensure_ascii=False)

    print(f"包含预测结果的测试集已保存到: {output_file}")


    valid_predictions = [
        item
        for item in test_set_with_predictions
        if np.isfinite(item["predicted_fitness"])
        and np.isfinite(item["actual_fitness"])
    ]

    if valid_predictions:
        prediction_errors = [item["prediction_error"] for item in valid_predictions]
        relative_errors = [
            item["relative_error"]
            for item in valid_predictions
            if item["relative_error"] != float("inf")
        ]

        print(f"\n=== 预测误差统计 ===")
        print(f"有效预测数量: {len(valid_predictions)}")
        print(f"平均绝对误差: {np.mean(prediction_errors):.4f}")
        print(f"绝对误差标准差: {np.std(prediction_errors):.4f}")
        print(f"最大绝对误差: {np.max(prediction_errors):.4f}")
        print(f"最小绝对误差: {np.min(prediction_errors):.4f}")

        if relative_errors:
            print(f"平均相对误差: {np.mean(relative_errors):.4f}")
            print(f"相对误差标准差: {np.std(relative_errors):.4f}")


def main():

    import argparse

    parser = argparse.ArgumentParser(description="从train_data1创建测试集")
    parser.add_argument("--num-samples", type=int, default=1000, help="测试集样本数量")
    parser.add_argument(
        "--with-predictions", action="store_true", help="是否包含预测结果"
    )
    parser.add_argument(
        "--train-data-dir",
        type=str,
        default="data/train_data1",
        help="train_data1目录路径",
    )
    parser.add_argument("--output-file", type=str, default=None, help="输出文件路径")

    args = parser.parse_args()


    random.seed(42)
    np.random.seed(42)

    try:

        configs = load_all_configs(args.train_data_dir)

        if not configs:
            print("没有找到有效的配置")
            sys.exit(1)


        if args.with_predictions:
            output_file = (
                args.output_file
                or f"data/test_configs_with_predictions_{args.num_samples}.json"
            )
            create_test_set_with_predictions(configs, args.num_samples, output_file)
        else:
            output_file = (
                args.output_file or f"data/test_configs_{args.num_samples}.json"
            )
            create_test_set(configs, args.num_samples, output_file)

        print("\n✅ 测试集创建完成!")

    except Exception as e:
        print(f"创建测试集失败: {e}")
        import traceback

        traceback.print_exc()
        sys.exit(1)


if __name__ == "__main__":
    main()
