




import os
import json
import numpy as np
import sys


sys.path.append(os.path.dirname(os.path.abspath(__file__)))

from surrogate_optimizer_data1 import SurrogateOptimizerData1


def load_split_datasets(data_dir: str):

    split_dir = os.path.join(data_dir, "split_datasets")


    with open(
        os.path.join(split_dir, "train_dataset.json"), "r", encoding="utf-8"
    ) as f:
        train_data = json.load(f)


    with open(os.path.join(split_dir, "val_dataset.json"), "r", encoding="utf-8") as f:
        val_data = json.load(f)


    with open(os.path.join(split_dir, "test_dataset.json"), "r", encoding="utf-8") as f:
        test_data = json.load(f)

    print(f"训练集: {len(train_data['X'])} 个配置")
    print(f"验证集: {len(val_data['X'])} 个配置")
    print(f"测试集: {len(test_data['X'])} 个配置")

    return train_data, val_data, test_data


def train_surrogate_model(train_data, val_data, test_data):

    print("\n开始训练代理模型...")


    optimizer = SurrogateOptimizerData1(
        enable_online_training=False, min_training_samples=100
    )


    X_train = np.array(train_data["X"])
    y_train = np.array(train_data["y"])
    feature_names = train_data["feature_names"]

    print(f"训练数据形状: X={X_train.shape}, y={y_train.shape}")
    print(f"特征数量: {len(feature_names)}")


    for i in range(len(X_train)):
        config = dict(zip(feature_names, X_train[i]))
        optimizer.add_training_data(config, y_train[i])


    optimizer._train_model()

    if optimizer.model_ready:
        print("模型训练成功！")


        model_name = "surrogate_model_new_dataset"
        optimizer.save_model(model_name)
        print(f"模型已保存为: {model_name}")


        evaluate_model_performance(
            optimizer, train_data, val_data, test_data, feature_names
        )

        return optimizer
    else:
        print("模型训练失败！")
        return None


def evaluate_model_performance(
    optimizer, train_data, val_data, test_data, feature_names
):

    print("\n正在评估模型性能...")


    train_predictions = []
    train_confidences = []

    for i in range(len(train_data["X"])):
        config = dict(zip(feature_names, train_data["X"][i]))
        pred, conf = optimizer.predict_fitness(config)
        train_predictions.append(pred)
        train_confidences.append(conf)

    train_predictions = np.array(train_predictions)
    train_confidences = np.array(train_confidences)


    val_predictions = []
    val_confidences = []

    for i in range(len(val_data["X"])):
        config = dict(zip(feature_names, val_data["X"][i]))
        pred, conf = optimizer.predict_fitness(config)
        val_predictions.append(pred)
        val_confidences.append(conf)

    val_predictions = np.array(val_predictions)
    val_confidences = np.array(val_confidences)


    test_predictions = []
    test_confidences = []

    for i in range(len(test_data["X"])):
        config = dict(zip(feature_names, test_data["X"][i]))
        pred, conf = optimizer.predict_fitness(config)
        test_predictions.append(pred)
        test_confidences.append(conf)

    test_predictions = np.array(test_predictions)
    test_confidences = np.array(test_confidences)


    train_metrics = calculate_metrics(np.array(train_data["y"]), train_predictions)
    val_metrics = calculate_metrics(np.array(val_data["y"]), val_predictions)
    test_metrics = calculate_metrics(np.array(test_data["y"]), test_predictions)


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


    save_evaluation_results(
        train_metrics,
        val_metrics,
        test_metrics,
        train_confidences,
        val_confidences,
        test_confidences,
    )


    feature_importance = optimizer.get_feature_importance()
    top_features = optimizer.get_top_features(top_k=10)

    print("\n=== 特征重要性 (前10名) ===")
    for feature, importance in top_features:
        print(f"{feature}: {importance:.4f}")


    save_feature_importance(feature_importance, top_features)


def calculate_metrics(y_true: np.ndarray, y_pred: np.ndarray):

    from sklearn.metrics import mean_squared_error, r2_score, mean_absolute_error

    return {
        "r2": r2_score(y_true, y_pred),
        "rmse": np.sqrt(mean_squared_error(y_true, y_pred)),
        "mae": mean_absolute_error(y_true, y_pred),
        "mse": mean_squared_error(y_true, y_pred),
    }


def save_evaluation_results(
    train_metrics,
    val_metrics,
    test_metrics,
    train_confidences,
    val_confidences,
    test_confidences,
):

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

    output_file = "model_evaluation_results.json"
    with open(output_file, "w", encoding="utf-8") as f:
        json.dump(evaluation_results, f, indent=2, ensure_ascii=False)

    print(f"评估结果已保存到: {output_file}")


def save_feature_importance(feature_importance, top_features):

    importance_data = {
        "feature_importance": feature_importance,
        "top_features": [(f, float(imp)) for f, imp in top_features],
    }

    output_file = "feature_importance_results.json"
    with open(output_file, "w", encoding="utf-8") as f:
        json.dump(importance_data, f, indent=2, ensure_ascii=False)

    print(f"特征重要性已保存到: {output_file}")


def create_algorithm_integration_script():

    integration_script =

    with open("ga_sa_cache_integration.py", "w", encoding="utf-8") as f:
        f.write(integration_script)

    print(f"算法集成脚本已保存到: ga_sa_cache_integration.py")


def main():


    data_dir = "/home/hush/workspace/AMP/GA/optimize/ga_sa_cache_optimize/proxy_models/data/5_300_1600_data_5000"

    print("=" * 60)
    print("使用分割后的数据集训练代理模型")
    print("=" * 60)

    try:

        train_data, val_data, test_data = load_split_datasets(data_dir)


        optimizer = train_surrogate_model(train_data, val_data, test_data)

        if optimizer:

            create_algorithm_integration_script()

            print("\n" + "=" * 60)
            print("代理模型训练和集成完成！")
            print("=" * 60)
            print("输出文件:")
            print("  - 训练好的模型: surrogate_model_new_dataset")
            print("  - 评估结果: model_evaluation_results.json")
            print("  - 特征重要性: feature_importance_results.json")
            print("  - 算法集成脚本: ga_sa_cache_integration.py")
            print("\n使用方法:")
            print("  1. 在GA+SA+Cache算法中导入 ga_sa_cache_integration.py")
            print("  2. 创建 GA_SA_Cache_Surrogate_Integrator 实例")
            print("  3. 使用 predict_config_fitness() 预测配置适应度")
            print("  4. 使用 should_use_surrogate() 判断是否使用代理模型")

    except Exception as e:
        print(f"流程执行失败: {e}")
        raise


if __name__ == "__main__":
    main()
