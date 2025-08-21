




import os
import sys
import json
import numpy as np
import joblib


sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from proxy_models.prepare_train_data1 import TrainData1Preprocessor


def predict_config_fitness(config_file: str) -> float:

    print(f"加载配置文件: {config_file}")


    with open(config_file, "r", encoding="utf-8") as f:
        config = json.load(f)

    print(f"配置包含 {len(config.get('localVar', []))} 个局部变量")


    data_preprocessor = TrainData1Preprocessor("data/train_data1")


    print("提取特征...")
    features_dict = data_preprocessor.extract_features_from_data({"config": config})

    print(f"提取了 {len(features_dict)} 个特征")


    with open("data/feature_names_data1.json", "r", encoding="utf-8") as f:
        feature_names = json.load(f)


    features = [features_dict[name] for name in feature_names]
    print(f"转换为数值特征: {len(features)} 个")


    print("加载特征工程引擎...")
    feature_engine_path = "data/models/surrogate_model_data1_feature_engine.pkl"
    feature_engine = joblib.load(feature_engine_path)


    print("生成增强特征...")
    X = np.array([features]).reshape(1, -1)
    X_enhanced = feature_engine.transform(X)

    print(f"增强特征数量: {X_enhanced.shape[1]}")


    print("加载XGBoost模型...")
    model_path = "data/models/surrogate_model_data1_xgboost.pkl"
    xgb_model = joblib.load(model_path)


    print("使用XGBoost模型预测...")
    predictions = xgb_model.predict(X_enhanced)

    return predictions[0]


def main():

    import argparse

    parser = argparse.ArgumentParser(description="直接预测单个配置的适应度值")
    parser.add_argument("config_file", type=str, help="配置文件路径")

    args = parser.parse_args()

    if not os.path.exists(args.config_file):
        print(f"错误: 配置文件不存在: {args.config_file}")
        sys.exit(1)

    try:

        predicted_fitness = predict_config_fitness(args.config_file)

        print("\n=== 预测结果 ===")
        print(f"预测适应度值: {predicted_fitness:.4f}")
        print(f"预测性能分数: {-predicted_fitness:.4f} Gflops")

    except Exception as e:
        print(f"预测失败: {e}")
        import traceback

        traceback.print_exc()
        sys.exit(1)


if __name__ == "__main__":
    main()
