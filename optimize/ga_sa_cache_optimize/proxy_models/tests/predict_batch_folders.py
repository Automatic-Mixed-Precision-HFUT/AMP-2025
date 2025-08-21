




import os
import sys
import json
import numpy as np
from pathlib import Path


sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from proxy_models.direct_predict import predict_config_fitness


def predict_single_folder(folder_path: Path) -> dict:

    print(f"\n{'='*60}")
    print(f"开始预测文件夹: {folder_path.name}")
    print(f"{'='*60}")


    index_file = folder_path / "index.json"
    if not index_file.exists():
        print(f"错误: 索引文件不存在: {index_file}")
        return None

    with open(index_file, "r", encoding="utf-8") as f:
        index_data = json.load(f)

    batch_name = index_data["folder_name"]
    batch_number = index_data["batch_number"]
    configs_in_batch = index_data["configs_in_batch"]

    print(f"批次名称: {batch_name}")
    print(f"批次编号: {batch_number}")
    print(f"配置数量: {configs_in_batch}")


    prediction_results = []


    for i, config_info in enumerate(index_data["configs"]):
        config_id = config_info["config_id"]
        config_file = folder_path / config_info["config_file"]
        actual_fitness = config_info["actual_fitness"]
        local_index = config_info.get("local_index", i + 1)

        print(f"\n预测配置 {local_index}/{configs_in_batch}: {config_id}")
        print(f"配置文件: {config_file}")
        print(f"实际适应度: {actual_fitness:.4f}")

        try:

            predicted_fitness = predict_config_fitness(str(config_file))


            prediction_error = abs(predicted_fitness - actual_fitness)
            relative_error = (
                prediction_error / abs(actual_fitness)
                if actual_fitness != 0
                else float("inf")
            )


            result = {
                "config_id": config_id,
                "local_index": local_index,
                "config_file": str(config_file),
                "actual_fitness": float(actual_fitness),
                "predicted_fitness": float(predicted_fitness),
                "prediction_error": float(prediction_error),
                "relative_error": float(relative_error),
                "prediction_success": True,
            }

            print(f"预测适应度: {predicted_fitness:.4f}")
            print(f"绝对误差: {prediction_error:.4f}")
            print(f"相对误差: {relative_error:.4f}")

        except Exception as e:
            print(f"预测失败: {e}")
            result = {
                "config_id": config_id,
                "local_index": local_index,
                "config_file": str(config_file),
                "actual_fitness": float(actual_fitness),
                "predicted_fitness": None,
                "prediction_error": None,
                "relative_error": None,
                "prediction_success": False,
                "error_message": str(e),
            }

        prediction_results.append(result)


    successful_predictions = [r for r in prediction_results if r["prediction_success"]]

    if successful_predictions:
        prediction_errors = [r["prediction_error"] for r in successful_predictions]
        relative_errors = [
            r["relative_error"]
            for r in successful_predictions
            if r["relative_error"] != float("inf")
        ]

        folder_summary = {
            "batch_name": batch_name,
            "batch_number": batch_number,
            "total_configs": len(prediction_results),
            "successful_predictions": len(successful_predictions),
            "success_rate": len(successful_predictions) / len(prediction_results) * 100,
            "mean_absolute_error": float(np.mean(prediction_errors)),
            "std_absolute_error": float(np.std(prediction_errors)),
            "max_absolute_error": float(np.max(prediction_errors)),
            "min_absolute_error": float(np.min(prediction_errors)),
            "mean_relative_error": (
                float(np.mean(relative_errors)) if relative_errors else None
            ),
            "std_relative_error": (
                float(np.std(relative_errors)) if relative_errors else None
            ),
        }

        print(f"\n{'='*60}")
        print(f"文件夹 {batch_name} 预测总结")
        print(f"{'='*60}")
        print(f"总配置数: {folder_summary['total_configs']}")
        print(f"成功预测数: {folder_summary['successful_predictions']}")
        print(f"预测成功率: {folder_summary['success_rate']:.1f}%")
        print(f"平均绝对误差: {folder_summary['mean_absolute_error']:.4f}")
        print(f"绝对误差标准差: {folder_summary['std_absolute_error']:.4f}")
        print(f"最大绝对误差: {folder_summary['max_absolute_error']:.4f}")
        print(f"最小绝对误差: {folder_summary['min_absolute_error']:.4f}")

        if folder_summary["mean_relative_error"] is not None:
            print(f"平均相对误差: {folder_summary['mean_relative_error']:.4f}")
            print(f"相对误差标准差: {folder_summary['std_relative_error']:.4f}")
    else:
        folder_summary = {
            "batch_name": batch_name,
            "batch_number": batch_number,
            "total_configs": len(prediction_results),
            "successful_predictions": 0,
            "success_rate": 0.0,
            "mean_absolute_error": None,
            "std_absolute_error": None,
            "max_absolute_error": None,
            "min_absolute_error": None,
            "mean_relative_error": None,
            "std_relative_error": None,
        }

        print(f"\n{'='*60}")
        print(f"文件夹 {batch_name} 预测总结")
        print(f"{'='*60}")
        print("所有预测都失败了")


    results_file = folder_path / "prediction_results.json"
    with open(results_file, "w", encoding="utf-8") as f:
        json.dump(prediction_results, f, indent=2, ensure_ascii=False)


    summary_file = folder_path / "prediction_summary.json"
    with open(summary_file, "w", encoding="utf-8") as f:
        json.dump(folder_summary, f, indent=2, ensure_ascii=False)

    print(f"\n预测结果已保存到: {results_file}")
    print(f"预测总结已保存到: {summary_file}")

    return {"folder_summary": folder_summary, "prediction_results": prediction_results}


def predict_all_folders(base_dir: str) -> None:

    base_path = Path(base_dir)

    if not base_path.exists():
        print(f"错误: 基础目录不存在: {base_dir}")
        return


    overall_index_file = base_path / "overall_index.json"
    if not overall_index_file.exists():
        print(f"错误: 总体索引文件不存在: {overall_index_file}")
        return

    with open(overall_index_file, "r", encoding="utf-8") as f:
        overall_index = json.load(f)

    total_configs = overall_index["total_configs"]
    num_folders = overall_index["num_folders"]
    folders = overall_index["folders"]

    print(f"开始预测所有文件夹")
    print(f"总配置数: {total_configs}")
    print(f"文件夹数: {num_folders}")


    all_folder_summaries = []
    all_prediction_results = []


    for folder_info in folders:
        folder_name = folder_info["folder_name"]
        folder_path = base_path / folder_name

        if folder_path.exists():
            result = predict_single_folder(folder_path)
            if result:
                all_folder_summaries.append(result["folder_summary"])
                all_prediction_results.extend(result["prediction_results"])
        else:
            print(f"警告: 文件夹不存在: {folder_path}")


    generate_overall_summary(all_folder_summaries, all_prediction_results, base_path)


def generate_overall_summary(
    folder_summaries: list, all_results: list, base_path: Path
) -> None:

    print(f"\n{'='*80}")
    print(f"总体预测总结")
    print(f"{'='*80}")


    total_configs = len(all_results)
    successful_predictions = [r for r in all_results if r["prediction_success"]]
    total_successful = len(successful_predictions)

    if total_successful > 0:
        prediction_errors = [r["prediction_error"] for r in successful_predictions]
        relative_errors = [
            r["relative_error"]
            for r in successful_predictions
            if r["relative_error"] != float("inf")
        ]

        overall_summary = {
            "total_configs": total_configs,
            "successful_predictions": total_successful,
            "success_rate": total_successful / total_configs * 100,
            "mean_absolute_error": float(np.mean(prediction_errors)),
            "std_absolute_error": float(np.std(prediction_errors)),
            "max_absolute_error": float(np.max(prediction_errors)),
            "min_absolute_error": float(np.min(prediction_errors)),
            "mean_relative_error": (
                float(np.mean(relative_errors)) if relative_errors else None
            ),
            "std_relative_error": (
                float(np.std(relative_errors)) if relative_errors else None
            ),
            "folder_summaries": folder_summaries,
        }

        print(f"总配置数: {total_configs}")
        print(f"成功预测数: {total_successful}")
        print(f"预测成功率: {overall_summary['success_rate']:.1f}%")
        print(f"平均绝对误差: {overall_summary['mean_absolute_error']:.4f}")
        print(f"绝对误差标准差: {overall_summary['std_absolute_error']:.4f}")
        print(f"最大绝对误差: {overall_summary['max_absolute_error']:.4f}")
        print(f"最小绝对误差: {overall_summary['min_absolute_error']:.4f}")

        if overall_summary["mean_relative_error"] is not None:
            print(f"平均相对误差: {overall_summary['mean_relative_error']:.4f}")
            print(f"相对误差标准差: {overall_summary['std_relative_error']:.4f}")


        print(f"\n各文件夹统计:")
        print(
            f"{'文件夹':<12} {'配置数':<8} {'成功率':<8} {'平均误差':<10} {'最大误差':<10}"
        )
        print(f"{'-'*60}")

        for folder_summary in folder_summaries:
            folder_name = folder_summary["batch_name"]
            configs = folder_summary["total_configs"]
            success_rate = folder_summary["success_rate"]
            mean_error = folder_summary["mean_absolute_error"] or 0
            max_error = folder_summary["max_absolute_error"] or 0

            print(
                f"{folder_name:<12} {configs:<8} {success_rate:<7.1f}% {mean_error:<10.4f} {max_error:<10.4f}"
            )

    else:
        overall_summary = {
            "total_configs": total_configs,
            "successful_predictions": 0,
            "success_rate": 0.0,
            "mean_absolute_error": None,
            "std_absolute_error": None,
            "max_absolute_error": None,
            "min_absolute_error": None,
            "mean_relative_error": None,
            "std_relative_error": None,
            "folder_summaries": folder_summaries,
        }

        print("所有预测都失败了")


    overall_results_file = base_path / "overall_prediction_results.json"
    with open(overall_results_file, "w", encoding="utf-8") as f:
        json.dump(all_results, f, indent=2, ensure_ascii=False)

    overall_summary_file = base_path / "overall_prediction_summary.json"
    with open(overall_summary_file, "w", encoding="utf-8") as f:
        json.dump(overall_summary, f, indent=2, ensure_ascii=False)

    print(f"\n总体预测结果已保存到: {overall_results_file}")
    print(f"总体预测总结已保存到: {overall_summary_file}")

    print(f"\n✅ 所有文件夹预测完成!")


def main():

    import argparse

    parser = argparse.ArgumentParser(description="批量文件夹预测")
    parser.add_argument(
        "--base-dir", type=str, default="data/reorganized_test_set", help="基础目录路径"
    )

    args = parser.parse_args()

    if not os.path.exists(args.base_dir):
        print(f"错误: 基础目录不存在: {args.base_dir}")
        sys.exit(1)

    try:
        predict_all_folders(args.base_dir)
    except Exception as e:
        print(f"预测失败: {e}")
        import traceback

        traceback.print_exc()
        sys.exit(1)


if __name__ == "__main__":
    main()
