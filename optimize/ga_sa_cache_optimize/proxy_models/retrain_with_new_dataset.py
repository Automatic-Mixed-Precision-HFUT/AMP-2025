




import os
import json
import numpy as np
import random
from typing import Dict, List, Tuple, Any
from sklearn.model_selection import train_test_split
from sklearn.metrics import mean_squared_error, r2_score, mean_absolute_error
import matplotlib.pyplot as plt
import seaborn as sns


from .surrogate_optimizer_data1 import SurrogateOptimizerData1
from .xgboost_model_manager_data1 import XGBoostModelManagerData1


class NewDatasetRetrainer:


    def __init__(self, data_dir: str):

        self.data_dir = data_dir
        self.overall_index_file = os.path.join(data_dir, "overall_index.json")
        self.feature_names = [
            "double_ratio",
            "float_ratio",
            "half_ratio",
            "int_ratio",
            "pointer_ratio",
            "dgemm_ratio",
            "dtrsm_ratio",
            "dlange_ratio",
            "dgemv_ratio",
            "copy_mat_ratio",
            "dgetrf_nopiv_ratio",
            "dgetrf2_nopiv_ratio",
            "rotmat_ratio",
            "gmres_ratio",
            "total_variables",
            "total_functions",
            "config_complexity",
        ]


        self.all_features = []
        self.all_fitness = []
        self.all_configs = []


        self.train_features = None
        self.train_fitness = None
        self.val_features = None
        self.val_fitness = None
        self.test_features = None
        self.test_fitness = None


        self.optimizer = None

    def load_dataset(self):

        print("正在加载数据集...")

        try:
            with open(self.overall_index_file, "r", encoding="utf-8") as f:
                overall_data = json.load(f)

            total_configs = 0

            for folder_info in overall_data["folders"]:
                batch_dir = os.path.join(self.data_dir, folder_info["folder_name"])
                batch_index_file = os.path.join(batch_dir, "index.json")

                if os.path.exists(batch_index_file):
                    with open(batch_index_file, "r", encoding="utf-8") as f:
                        batch_data = json.load(f)

                    for config_info in batch_data["configs"]:
                        config_dir = os.path.join(batch_dir, config_info["config_dir"])
                        metadata_file = os.path.join(config_dir, "metadata.json")

                        if os.path.exists(metadata_file):
                            with open(metadata_file, "r", encoding="utf-8") as f:
                                metadata = json.load(f)


                            features = metadata["features"]
                            fitness = metadata["estimated_fitness"]


                            feature_vector = [
                                features.get(name, 0.0) for name in self.feature_names
                            ]

                            self.all_features.append(feature_vector)
                            self.all_fitness.append(fitness)
                            self.all_configs.append(
                                {
                                    "batch": folder_info["folder_name"],
                                    "config_id": config_info["config_id"],
                                    "metadata": metadata,
                                }
                            )

                            total_configs += 1

                            if total_configs % 100 == 0:
                                print(f"已加载 {total_configs} 个配置...")

            self.all_features = np.array(self.all_features)
            self.all_fitness = np.array(self.all_fitness)

            print(f"数据集加载完成！总共 {len(self.all_features)} 个配置")
            print(f"特征维度: {self.all_features.shape}")
            print(
                f"适应度范围: {np.min(self.all_fitness):.2f} - {np.max(self.all_fitness):.2f}"
            )
            print(f"平均适应度: {np.mean(self.all_fitness):.2f}")

        except Exception as e:
            print(f"加载数据集失败: {e}")
            raise

    def split_dataset(
        self, test_size: int = 500, val_size: int = 500, random_state: int = 42
    ):

        print(f"正在分割数据集...")
        print(f"总配置数: {len(self.all_features)}")
        print(f"测试集大小: {test_size}")
        print(f"验证集大小: {val_size}")
        print(f"训练集大小: {len(self.all_features) - test_size - val_size}")


        remaining_features, self.test_features, remaining_fitness, self.test_fitness = (
            train_test_split(
                self.all_features,
                self.all_fitness,
                test_size=test_size,
                random_state=random_state,
                stratify=None,
            )
        )


        self.train_features, self.val_features, self.train_fitness, self.val_fitness = (
            train_test_split(
                remaining_features,
                remaining_fitness,
                test_size=val_size,
                random_state=random_state,
            )
        )

        print(f"数据集分割完成！")
        print(f"训练集: {len(self.train_features)} 个配置")
        print(f"验证集: {len(self.val_features)} 个配置")
        print(f"测试集: {len(self.test_features)} 个配置")


        self._save_split_datasets()

    def _save_split_datasets(self):

        output_dir = os.path.join(self.data_dir, "split_datasets")
        os.makedirs(output_dir, exist_ok=True)


        train_data = {
            "X": self.train_features.tolist(),
            "y": self.train_fitness.tolist(),
            "feature_names": self.feature_names,
            "dataset_type": "train",
            "size": len(self.train_features),
        }

        with open(
            os.path.join(output_dir, "train_dataset.json"), "w", encoding="utf-8"
        ) as f:
            json.dump(train_data, f, indent=2, ensure_ascii=False)


        val_data = {
            "X": self.val_features.tolist(),
            "y": self.val_fitness.tolist(),
            "feature_names": self.feature_names,
            "dataset_type": "validation",
            "size": len(self.val_features),
        }

        with open(
            os.path.join(output_dir, "val_dataset.json"), "w", encoding="utf-8"
        ) as f:
            json.dump(val_data, f, indent=2, ensure_ascii=False)


        test_data = {
            "X": self.test_features.tolist(),
            "y": self.test_fitness.tolist(),
            "feature_names": self.feature_names,
            "dataset_type": "test",
            "size": len(self.test_features),
        }

        with open(
            os.path.join(output_dir, "test_dataset.json"), "w", encoding="utf-8"
        ) as f:
            json.dump(test_data, f, indent=2, ensure_ascii=False)


        summary = {
            "total_configs": len(self.all_features),
            "train_size": len(self.train_features),
            "val_size": len(self.val_features),
            "test_size": len(self.test_features),
            "feature_names": self.feature_names,
            "fitness_statistics": {
                "train": {
                    "min": float(np.min(self.train_fitness)),
                    "max": float(np.max(self.train_fitness)),
                    "mean": float(np.mean(self.train_fitness)),
                    "std": float(np.std(self.train_fitness)),
                },
                "validation": {
                    "min": float(np.min(self.val_fitness)),
                    "max": float(np.max(self.val_fitness)),
                    "mean": float(np.mean(self.val_fitness)),
                    "std": float(np.std(self.val_fitness)),
                },
                "test": {
                    "min": float(np.min(self.test_fitness)),
                    "max": float(np.max(self.test_fitness)),
                    "mean": float(np.mean(self.test_fitness)),
                    "std": float(np.std(self.test_fitness)),
                },
            },
        }

        with open(
            os.path.join(output_dir, "dataset_summary.json"), "w", encoding="utf-8"
        ) as f:
            json.dump(summary, f, indent=2, ensure_ascii=False)

        print(f"分割后的数据集已保存到: {output_dir}")

    def train_model(self):

        print("正在训练代理模型...")


        self.optimizer = SurrogateOptimizerData1(
            enable_online_training=False,
            min_training_samples=100,
        )


        X_train = self.train_features
        y_train = self.train_fitness

        print(f"训练数据形状: X={X_train.shape}, y={y_train.shape}")


        try:

            for i in range(len(X_train)):
                config = dict(zip(self.feature_names, X_train[i]))
                self.optimizer.add_training_data(config, y_train[i])


            self.optimizer._train_model()

            if self.optimizer.model_ready:
                print("模型训练成功！")


                model_name = "surrogate_model_new_dataset"
                self.optimizer.save_model(model_name)
                print(f"模型已保存为: {model_name}")


                self._evaluate_model()
            else:
                print("模型训练失败！")

        except Exception as e:
            print(f"训练模型时出错: {e}")
            raise

    def _evaluate_model(self):

        print("正在评估模型性能...")


        train_predictions = []
        train_confidences = []

        for i in range(len(self.train_features)):
            config = dict(zip(self.feature_names, self.train_features[i]))
            pred, conf = self.optimizer.predict_fitness(config)
            train_predictions.append(pred)
            train_confidences.append(conf)

        train_predictions = np.array(train_predictions)
        train_confidences = np.array(train_confidences)


        val_predictions = []
        val_confidences = []

        for i in range(len(self.val_features)):
            config = dict(zip(self.feature_names, self.val_features[i]))
            pred, conf = self.optimizer.predict_fitness(config)
            val_predictions.append(pred)
            val_confidences.append(conf)

        val_predictions = np.array(val_predictions)
        val_confidences = np.array(val_confidences)


        test_predictions = []
        test_confidences = []

        for i in range(len(self.test_features)):
            config = dict(zip(self.feature_names, self.test_features[i]))
            pred, conf = self.optimizer.predict_fitness(config)
            test_predictions.append(pred)
            test_confidences.append(conf)

        test_predictions = np.array(test_predictions)
        test_confidences = np.array(test_confidences)


        train_metrics = self._calculate_metrics(self.train_fitness, train_predictions)
        val_metrics = self._calculate_metrics(self.val_fitness, val_predictions)
        test_metrics = self._calculate_metrics(self.test_fitness, test_predictions)


        print("\n=== 模型性能评估 ===")
        print(
            f"训练集 - R²: {train_metrics['r2']:.4f}, RMSE: {train_metrics['rmse']:.4f}, MAE: {train_metrics['mae']:.4f}"
        )
        print(
            f"验证集 - R²: {val_metrics['r2']:.4f}, RMSE: {val_metrics['rmse']:.4f}, MAE: {val_metrics['mae']:.4f}"
        )
        print(
            f"测试集 - R²: {test_metrics['r2']:.4f}, RMSE: {test_metrics['rmse']:.4f}, MAE: {test_metrics['mae']:.4f}"
        )


        evaluation_results = {
            "train_metrics": train_metrics,
            "validation_metrics": val_metrics,
            "test_metrics": test_metrics,
            "confidence_statistics": {
                "train": {
                    "mean": float(np.mean(train_confidences)),
                    "std": float(np.std(train_confidences)),
                    "min": float(np.min(train_confidences)),
                    "max": float(np.max(train_confidences)),
                },
                "validation": {
                    "mean": float(np.mean(val_confidences)),
                    "std": float(np.std(val_confidences)),
                    "min": float(np.min(val_confidences)),
                    "max": float(np.max(val_confidences)),
                },
                "test": {
                    "mean": float(np.mean(test_confidences)),
                    "std": float(np.std(test_confidences)),
                    "min": float(np.min(test_confidences)),
                    "max": float(np.max(test_confidences)),
                },
            },
        }

        output_dir = os.path.join(self.data_dir, "split_datasets")
        with open(
            os.path.join(output_dir, "model_evaluation.json"), "w", encoding="utf-8"
        ) as f:
            json.dump(evaluation_results, f, indent=2, ensure_ascii=False)


        self._generate_evaluation_plots(
            train_predictions, val_predictions, test_predictions
        )


        feature_importance = self.optimizer.get_feature_importance()
        top_features = self.optimizer.get_top_features(top_k=10)

        print("\n=== 特征重要性 (前10名) ===")
        for feature, importance in top_features:
            print(f"{feature}: {importance:.4f}")


        importance_data = {
            "feature_importance": feature_importance,
            "top_features": [(f, float(imp)) for f, imp in top_features],
        }

        with open(
            os.path.join(output_dir, "feature_importance.json"), "w", encoding="utf-8"
        ) as f:
            json.dump(importance_data, f, indent=2, ensure_ascii=False)

    def _calculate_metrics(
        self, y_true: np.ndarray, y_pred: np.ndarray
    ) -> Dict[str, float]:

        return {
            "r2": r2_score(y_true, y_pred),
            "rmse": np.sqrt(mean_squared_error(y_true, y_pred)),
            "mae": mean_absolute_error(y_true, y_pred),
            "mse": mean_squared_error(y_true, y_pred),
        }

    def _generate_evaluation_plots(self, train_pred, val_pred, test_pred):

        output_dir = os.path.join(self.data_dir, "split_datasets")
        os.makedirs(output_dir, exist_ok=True)


        plt.rcParams["font.sans-serif"] = ["SimHei", "DejaVu Sans"]
        plt.rcParams["axes.unicode_minus"] = False


        fig, axes = plt.subplots(2, 2, figsize=(15, 12))
        fig.suptitle("代理模型性能评估", fontsize=16, fontweight="bold")


        axes[0, 0].scatter(self.train_fitness, train_pred, alpha=0.6, color="blue")
        axes[0, 0].plot(
            [self.train_fitness.min(), self.train_fitness.max()],
            [self.train_fitness.min(), self.train_fitness.max()],
            "r--",
            lw=2,
        )
        axes[0, 0].set_xlabel("真实适应度值")
        axes[0, 0].set_ylabel("预测适应度值")
        axes[0, 0].set_title("训练集: 预测 vs 真实值")
        axes[0, 0].grid(True, alpha=0.3)


        axes[0, 1].scatter(self.val_fitness, val_pred, alpha=0.6, color="green")
        axes[0, 1].plot(
            [self.val_fitness.min(), self.val_fitness.max()],
            [self.val_fitness.min(), self.val_fitness.max()],
            "r--",
            lw=2,
        )
        axes[0, 1].set_xlabel("真实适应度值")
        axes[0, 1].set_ylabel("预测适应度值")
        axes[0, 1].set_title("验证集: 预测 vs 真实值")
        axes[0, 1].grid(True, alpha=0.3)


        axes[1, 0].scatter(self.test_fitness, test_pred, alpha=0.6, color="orange")
        axes[1, 0].plot(
            [self.test_fitness.min(), self.test_fitness.max()],
            [self.test_fitness.min(), self.test_fitness.max()],
            "r--",
            lw=2,
        )
        axes[1, 0].set_xlabel("真实适应度值")
        axes[1, 0].set_ylabel("预测适应度值")
        axes[1, 0].set_title("测试集: 预测 vs 真实值")
        axes[1, 0].grid(True, alpha=0.3)


        train_residuals = train_pred - self.train_fitness
        val_residuals = val_pred - self.val_fitness
        test_residuals = test_pred - self.test_fitness

        axes[1, 1].scatter(
            train_pred, train_residuals, alpha=0.6, color="blue", label="训练集"
        )
        axes[1, 1].scatter(
            val_pred, val_residuals, alpha=0.6, color="green", label="验证集"
        )
        axes[1, 1].scatter(
            test_pred, test_residuals, alpha=0.6, color="orange", label="测试集"
        )
        axes[1, 1].axhline(y=0, color="r", linestyle="--", lw=2)
        axes[1, 1].set_xlabel("预测适应度值")
        axes[1, 1].set_ylabel("残差")
        axes[1, 1].set_title("残差分析")
        axes[1, 1].legend()
        axes[1, 1].grid(True, alpha=0.3)

        plt.tight_layout()
        plt.savefig(
            os.path.join(output_dir, "model_evaluation_plots.png"),
            dpi=300,
            bbox_inches="tight",
        )
        plt.close()

        print(
            f"评估图表已保存到: {os.path.join(output_dir, 'model_evaluation_plots.png')}"
        )

    def create_integration_script(self):

        output_dir = os.path.join(self.data_dir, "split_datasets")

        integration_script =

        with open(
            os.path.join(output_dir, "integration_script.py"), "w", encoding="utf-8"
        ) as f:
            f.write(integration_script)

        print(f"集成脚本已保存到: {os.path.join(output_dir, 'integration_script.py')}")

    def run_full_pipeline(self):

        print("=" * 60)
        print("开始新数据集重新训练流程")
        print("=" * 60)

        try:

            self.load_dataset()


            self.split_dataset(test_size=500, val_size=500)


            self.train_model()


            self.create_integration_script()

            print("\n" + "=" * 60)
            print("新数据集重新训练流程完成！")
            print("=" * 60)
            print("输出文件:")
            print(f"  - 分割数据集: {os.path.join(self.data_dir, 'split_datasets/')}")
            print(f"  - 训练好的模型: surrogate_model_new_dataset")
            print(f"  - 集成脚本: integration_script.py")
            print(f"  - 评估结果: model_evaluation.json")
            print(f"  - 特征重要性: feature_importance.json")
            print(f"  - 可视化图表: model_evaluation_plots.png")

        except Exception as e:
            print(f"流程执行失败: {e}")
            raise


def main():


    data_dir = "/home/hush/workspace/AMP/GA/optimize/ga_sa_cache_optimize/proxy_models/data/5_300_1600_data_5000"


    retrainer = NewDatasetRetrainer(data_dir)


    retrainer.run_full_pipeline()


if __name__ == "__main__":
    main()
