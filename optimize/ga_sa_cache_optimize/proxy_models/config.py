from pathlib import Path


CURRENT_DIR = Path(__file__).parent


PROXY_MODELS_DIR = CURRENT_DIR
MODELS_DIR = PROXY_MODELS_DIR / "data" / "models"
DATA_DIR = PROXY_MODELS_DIR / "data"
LOGS_DIR = PROXY_MODELS_DIR / "logs"


for directory in [MODELS_DIR, DATA_DIR, LOGS_DIR]:
    directory.mkdir(exist_ok=True)


MODEL_BASE_NAME = "surrogate_model"
FEATURE_ENGINE_SUFFIX = "_feature_engine.pkl"
XGBOOST_SUFFIX = "_xgboost.pkl"
MODEL_INFO_SUFFIX = "_info.pkl"


TRAINING_DATA_FILE = DATA_DIR / "training_data.json"
VALIDATION_DATA_FILE = DATA_DIR / "validation_data.json"


TRAINING_LOG_FILE = LOGS_DIR / "training.log"
PREDICTION_LOG_FILE = LOGS_DIR / "prediction.log"


MODEL_CONFIG = {
    "xgboost": {
        "n_estimators": 200,
        "max_depth": 6,
        "learning_rate": 0.1,
        "subsample": 0.8,
        "colsample_bytree": 0.8,
        "reg_alpha": 0.1,
        "reg_lambda": 1.0,
        "random_state": 42,
        "n_jobs": -1,
    },
    "feature_engineering": {
        "n_estimators": 100,
        "learning_rate": 0.1,
        "max_depth": 6,
        "subsample": 0.8,
        "random_state": 42,
    },
    "surrogate_optimizer": {
        "confidence_threshold": 0.8,
        "min_training_samples": 10,
    },
}


FEATURE_CONFIG = {
    "precision_features": ["double_ratio", "float_ratio", "half_ratio"],
    "optimization_features": ["loop_unroll_factor", "vectorization_level"],
    "interaction_features": True,
    "leaf_features": True,
}


def get_model_path(base_name: str = MODEL_BASE_NAME) -> dict:

    return {
        "feature_engine": MODELS_DIR / f"{base_name}{FEATURE_ENGINE_SUFFIX}",
        "xgboost": MODELS_DIR / f"{base_name}{XGBOOST_SUFFIX}",
        "model_info": MODELS_DIR / f"{base_name}{MODEL_INFO_SUFFIX}",
    }


def get_data_path(data_type: str = "training") -> Path:

    if data_type == "training":
        return TRAINING_DATA_FILE
    elif data_type == "validation":
        return VALIDATION_DATA_FILE
    else:
        raise ValueError(f"未知的数据类型: {data_type}")


def get_log_path(log_type: str = "training") -> Path:

    if log_type == "training":
        return TRAINING_LOG_FILE
    elif log_type == "prediction":
        return PREDICTION_LOG_FILE
    else:
        raise ValueError(f"未知的日志类型: {log_type}")


if __name__ == "__main__":
    print("=== 代理模型系统配置 ===")
    print(f"代理模型目录: {PROXY_MODELS_DIR}")
    print(f"模型存储目录: {MODELS_DIR}")
    print(f"数据存储目录: {DATA_DIR}")
    print(f"日志存储目录: {LOGS_DIR}")
    print(f"训练数据文件: {TRAINING_DATA_FILE}")
    print(f"验证数据文件: {VALIDATION_DATA_FILE}")
    print(f"训练日志文件: {TRAINING_LOG_FILE}")
    print(f"预测日志文件: {PREDICTION_LOG_FILE}")
