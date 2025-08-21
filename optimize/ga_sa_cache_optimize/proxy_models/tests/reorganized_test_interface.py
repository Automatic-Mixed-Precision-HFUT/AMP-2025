




import os
import sys
import json
import numpy as np
from pathlib import Path
from typing import Dict, Optional, Any
from datetime import datetime
import matplotlib.pyplot as plt


sys.path.insert(
    0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
)

from proxy_models.surrogate_optimizer_data1 import SurrogateOptimizerData1
from proxy_models.prepare_train_data1 import TrainData1Preprocessor
from .direct_predict import predict_config_fitness


class ReorganizedTestInterface:


    def __init__(self, test_set_path: str, model_dir: str = None):

        self.test_set_path = Path(test_set_path)
        self.model_dir = Path(model_dir) if model_dir else Path("data/models")


        if not self.test_set_path.exists():
            raise FileNotFoundError(f"测试集路径不存在: {test_set_path}")


        self.overall_index_file = self.test_set_path / "overall_index.json"
        if not self.overall_index_file.exists():
            raise FileNotFoundError(f"总体索引文件不存在: {self.overall_index_file}")

        with open(self.overall_index_file, "r", encoding="utf-8") as f:
            self.overall_index = json.load(f)


        self.optimizer = SurrogateOptimizerData1(model_dir=str(self.model_dir))

        self.preprocessor = TrainData1Preprocessor("data/train_data1")

        try:
            self.optimizer.load_model("surrogate_model_new_dataset")
        except Exception as e:
            print(f"⚠️ 预训练模型加载失败，将在预测时报错: {e}")


        self.test_results = {
            "overall_stats": {},
            "batch_results": {},
            "detailed_results": [],
        }

    def get_test_set_info(self) -> Dict[str, Any]:

        return {
            "total_configs": self.overall_index["total_configs"],
            "configs_per_folder": self.overall_index["configs_per_folder"],
            "num_folders": self.overall_index["num_folders"],
            "folders": self.overall_index["folders"],
        }

    def test_single_batch(
        self, batch_number: int, use_surrogate: bool = True
    ) -> Dict[str, Any]:


        batch_info = None
        for folder in self.overall_index["folders"]:
            if folder["batch_number"] == batch_number:
                batch_info = folder
                break

        if not batch_info:
            raise ValueError(f"批次 {batch_number} 不存在")

        batch_path = self.test_set_path / batch_info["folder_name"]
        batch_index_file = batch_path / "index.json"

        if not batch_index_file.exists():
            raise FileNotFoundError(f"批次索引文件不存在: {batch_index_file}")


        with open(batch_index_file, "r", encoding="utf-8") as f:
            batch_index = json.load(f)

        print(f"\n{'='*60}")
        print(f"测试批次 {batch_number}: {batch_info['folder_name']}")
        print(f"配置数量: {batch_info['configs_count']}")
        print(f"{'='*60}")


        batch_results = {
            "batch_number": batch_number,
            "batch_name": batch_info["folder_name"],
            "configs_count": batch_info["configs_count"],
            "predictions": [],
            "errors": [],
            "success_count": 0,
            "failure_count": 0,
        }


        for i, config_info in enumerate(batch_index["configs"]):
            config_id = config_info["config_id"]
            config_file = batch_path / config_info["config_file"]

            actual_fitness = config_info.get("actual_fitness")
            if actual_fitness is None:
                meta_path = batch_path / config_info["metadata_file"]
                with open(meta_path, "r", encoding="utf-8") as mf:
                    meta = json.load(mf)
                actual_fitness = meta.get("actual_fitness")
                if actual_fitness is None:
                    actual_fitness = meta.get("final_score")
                if actual_fitness is None:
                    actual_fitness = meta.get("estimated_fitness")
                if actual_fitness is None:
                    raise KeyError("actual_fitness")

            print(f"\n测试配置 {i+1}/{batch_info['configs_count']}: {config_id}")
            print(f"实际适应度: {actual_fitness:.4f}")

            try:
                if use_surrogate:

                    with open(config_file, "r", encoding="utf-8") as f:
                        raw_config = json.load(f)
                    features_dict = self.preprocessor.extract_features_from_data(
                        {"config": raw_config}
                    )
                    predicted_fitness, confidence = self.optimizer.predict_fitness(
                        features_dict
                    )
                else:

                    predicted_fitness = predict_config_fitness(str(config_file))
                    confidence = 1.0


                prediction_error = abs(predicted_fitness - actual_fitness)
                relative_error = (
                    prediction_error / abs(actual_fitness)
                    if actual_fitness != 0
                    else float("inf")
                )


                result = {
                    "config_id": config_id,
                    "config_file": str(config_file),
                    "actual_fitness": float(actual_fitness),
                    "predicted_fitness": float(predicted_fitness),
                    "confidence": float(confidence),
                    "prediction_error": float(prediction_error),
                    "relative_error": float(relative_error),
                    "prediction_success": True,
                }

                batch_results["predictions"].append(result)
                batch_results["errors"].append(prediction_error)
                batch_results["success_count"] += 1

                print(f"预测适应度: {predicted_fitness:.4f}")
                print(f"置信度: {confidence:.4f}")
                print(f"绝对误差: {prediction_error:.4f}")
                print(f"相对误差: {relative_error:.4f}")

            except Exception as e:
                print(f"预测失败: {e}")
                result = {
                    "config_id": config_id,
                    "config_file": str(config_file),
                    "actual_fitness": float(actual_fitness),
                    "predicted_fitness": None,
                    "confidence": None,
                    "prediction_error": None,
                    "relative_error": None,
                    "prediction_success": False,
                    "error_message": str(e),
                }

                batch_results["predictions"].append(result)
                batch_results["failure_count"] += 1


        if batch_results["success_count"] > 0:
            errors = [
                r["prediction_error"]
                for r in batch_results["predictions"]
                if r["prediction_success"]
            ]
            batch_results["stats"] = {
                "mean_error": np.mean(errors),
                "std_error": np.std(errors),
                "max_error": np.max(errors),
                "min_error": np.min(errors),
                "median_error": np.median(errors),
                "success_rate": batch_results["success_count"]
                / batch_info["configs_count"],
            }
        else:
            batch_results["stats"] = {
                "mean_error": None,
                "std_error": None,
                "max_error": None,
                "min_error": None,
                "median_error": None,
                "success_rate": 0.0,
            }

        print(f"\n批次 {batch_number} 测试完成:")
        print(f"成功: {batch_results['success_count']}")
        print(f"失败: {batch_results['failure_count']}")
        if batch_results["stats"]["mean_error"] is not None:
            print(f"平均误差: {batch_results['stats']['mean_error']:.4f}")
            print(f"成功率: {batch_results['stats']['success_rate']:.2%}")

        return batch_results

    def test_all_batches(
        self, use_surrogate: bool = True, max_batches: Optional[int] = None
    ) -> Dict[str, Any]:

        print(f"\n{'='*80}")
        print(f"开始测试所有批次")
        print(f"总批次数: {self.overall_index['num_folders']}")
        print(f"每批次配置数: {self.overall_index['configs_per_folder']}")
        print(f"总配置数: {self.overall_index['total_configs']}")
        print(f"使用代理模型: {use_surrogate}")
        print(f"{'='*80}")

        all_results = {
            "test_info": {
                "test_time": datetime.now().isoformat(),
                "use_surrogate": use_surrogate,
                "total_batches": self.overall_index["num_folders"],
                "total_configs": self.overall_index["total_configs"],
            },
            "batch_results": {},
            "overall_stats": {},
        }


        batches_to_test = self.overall_index["folders"]
        if max_batches:
            batches_to_test = batches_to_test[:max_batches]


        for folder_info in batches_to_test:
            batch_number = folder_info["batch_number"]
            try:
                batch_result = self.test_single_batch(batch_number, use_surrogate)
                all_results["batch_results"][batch_number] = batch_result
            except Exception as e:
                print(f"批次 {batch_number} 测试失败: {e}")
                all_results["batch_results"][batch_number] = {
                    "batch_number": batch_number,
                    "error": str(e),
                }


        all_results["overall_stats"] = self._calculate_overall_stats(all_results)

        return all_results

    def _calculate_overall_stats(self, all_results: Dict[str, Any]) -> Dict[str, Any]:

        all_predictions = []
        all_errors = []
        total_success = 0
        total_failure = 0

        for batch_result in all_results["batch_results"].values():
            if "predictions" in batch_result:
                for pred in batch_result["predictions"]:
                    if pred["prediction_success"]:
                        all_predictions.append(pred)
                        all_errors.append(pred["prediction_error"])
                        total_success += 1
                    else:
                        total_failure += 1

        if len(all_errors) == 0:
            return {
                "total_success": total_success,
                "total_failure": total_failure,
                "success_rate": 0.0,
                "mean_error": None,
                "std_error": None,
                "max_error": None,
                "min_error": None,
                "median_error": None,
            }

        return {
            "total_success": total_success,
            "total_failure": total_failure,
            "success_rate": total_success / (total_success + total_failure),
            "mean_error": np.mean(all_errors),
            "std_error": np.std(all_errors),
            "max_error": np.max(all_errors),
            "min_error": np.min(all_errors),
            "median_error": np.median(all_errors),
        }

    def save_results(
        self, results: Dict[str, Any], output_dir: str = "test_results"
    ) -> str:

        output_path = Path(output_dir)
        output_path.mkdir(parents=True, exist_ok=True)


        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")


        detailed_file = output_path / f"detailed_results_{timestamp}.json"
        with open(detailed_file, "w", encoding="utf-8") as f:
            json.dump(results, f, indent=2, ensure_ascii=False)


        summary_file = output_path / f"summary_{timestamp}.json"
        summary = {
            "test_info": results["test_info"],
            "overall_stats": results["overall_stats"],
            "batch_summary": {},
        }

        for batch_num, batch_result in results["batch_results"].items():
            if "stats" in batch_result:
                summary["batch_summary"][batch_num] = batch_result["stats"]

        with open(summary_file, "w", encoding="utf-8") as f:
            json.dump(summary, f, indent=2, ensure_ascii=False)

        print(f"\n测试结果已保存:")
        print(f"详细结果: {detailed_file}")
        print(f"总体统计: {summary_file}")

        return str(output_path)

    def generate_plots(
        self, results: Dict[str, Any], output_dir: str = "test_results"
    ) -> None:

        output_path = Path(output_dir)
        output_path.mkdir(parents=True, exist_ok=True)

        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")


        all_predictions = []
        for batch_result in results["batch_results"].values():
            if "predictions" in batch_result:
                all_predictions.extend(batch_result["predictions"])

        if not all_predictions:
            print("没有有效的预测数据，跳过图表生成")
            return


        successful_predictions = [p for p in all_predictions if p["prediction_success"]]

        if not successful_predictions:
            print("没有成功的预测，跳过图表生成")
            return


        fig, axes = plt.subplots(2, 2, figsize=(15, 12))
        fig.suptitle(f"代理模型测试结果 - {timestamp}", fontsize=16)


        actual_values = [p["actual_fitness"] for p in successful_predictions]
        predicted_values = [p["predicted_fitness"] for p in successful_predictions]

        axes[0, 0].scatter(actual_values, predicted_values, alpha=0.6)
        axes[0, 0].plot(
            [min(actual_values), max(actual_values)],
            [min(actual_values), max(actual_values)],
            "r--",
            lw=2,
        )
        axes[0, 0].set_xlabel("实际适应度")
        axes[0, 0].set_ylabel("预测适应度")
        axes[0, 0].set_title("预测值 vs 实际值")
        axes[0, 0].grid(True, alpha=0.3)


        errors = [p["prediction_error"] for p in successful_predictions]
        axes[0, 1].hist(errors, bins=30, alpha=0.7, edgecolor="black")
        axes[0, 1].set_xlabel("预测误差")
        axes[0, 1].set_ylabel("频次")
        axes[0, 1].set_title("误差分布")
        axes[0, 1].grid(True, alpha=0.3)


        batch_errors = []
        batch_numbers = []
        for batch_num, batch_result in results["batch_results"].items():
            if (
                "stats" in batch_result
                and batch_result["stats"]["mean_error"] is not None
            ):
                batch_errors.append(batch_result["stats"]["mean_error"])
                batch_numbers.append(int(batch_num))

        if batch_errors:
            axes[1, 0].bar(batch_numbers, batch_errors, alpha=0.7)
            axes[1, 0].set_xlabel("批次编号")
            axes[1, 0].set_ylabel("平均误差")
            axes[1, 0].set_title("各批次平均误差")
            axes[1, 0].grid(True, alpha=0.3)


        confidences = [
            p["confidence"]
            for p in successful_predictions
            if p["confidence"] is not None
        ]
        if confidences:
            axes[1, 1].hist(confidences, bins=20, alpha=0.7, edgecolor="black")
            axes[1, 1].set_xlabel("置信度")
            axes[1, 1].set_ylabel("频次")
            axes[1, 1].set_title("置信度分布")
            axes[1, 1].grid(True, alpha=0.3)

        plt.tight_layout()


        plot_file = output_path / f"test_plots_{timestamp}.png"
        plt.savefig(plot_file, dpi=300, bbox_inches="tight")
        plt.close()

        print(f"测试图表已保存: {plot_file}")

    def print_summary(self, results: Dict[str, Any]) -> None:

        print(f"\n{'='*80}")
        print(f"测试结果摘要")
        print(f"{'='*80}")

        test_info = results["test_info"]
        overall_stats = results["overall_stats"]

        print(f"测试时间: {test_info['test_time']}")
        print(f"使用代理模型: {test_info['use_surrogate']}")
        print(f"测试批次数: {test_info['total_batches']}")
        print(f"总配置数: {test_info['total_configs']}")

        print(f"\n总体统计:")
        print(f"成功预测: {overall_stats['total_success']}")
        print(f"失败预测: {overall_stats['total_failure']}")
        print(f"成功率: {overall_stats['success_rate']:.2%}")

        if overall_stats["mean_error"] is not None:
            print(f"平均误差: {overall_stats['mean_error']:.4f}")
            print(f"误差标准差: {overall_stats['std_error']:.4f}")
            print(f"最大误差: {overall_stats['max_error']:.4f}")
            print(f"最小误差: {overall_stats['min_error']:.4f}")
            print(f"中位数误差: {overall_stats['median_error']:.4f}")

        print(f"{'='*80}")
