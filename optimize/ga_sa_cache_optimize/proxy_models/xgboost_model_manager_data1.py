import numpy as np
import joblib
import os
from typing import Dict, List, Tuple, Optional
import xgboost as xgb
from sklearn.model_selection import cross_val_score
from sklearn.metrics import mean_squared_error, r2_score
from proxy_models.feature_engineering import FeatureEngineeringEngine
from proxy_models.config import get_model_path, MODEL_CONFIG


class XGBoostModelManagerData1:

    def __init__(
        self,
        n_estimators: int = None,
        max_depth: int = None,
        learning_rate: float = None,
        subsample: float = None,
        colsample_bytree: float = None,
        reg_alpha: float = None,
        reg_lambda: float = None,
        random_state: int = None,
        n_jobs: int = None,
        model_dir: str = None,
    ):

        xg_cfg = MODEL_CONFIG["xgboost"]
        self.n_estimators = n_estimators or xg_cfg["n_estimators"]
        self.max_depth = max_depth or xg_cfg["max_depth"]
        self.learning_rate = learning_rate or xg_cfg["learning_rate"]
        self.subsample = subsample or xg_cfg["subsample"]
        self.colsample_bytree = colsample_bytree or xg_cfg["colsample_bytree"]
        self.reg_alpha = reg_alpha or xg_cfg["reg_alpha"]
        self.reg_lambda = reg_lambda or xg_cfg["reg_lambda"]
        self.random_state = random_state or xg_cfg["random_state"]
        self.n_jobs = n_jobs or xg_cfg["n_jobs"]
        self.model_dir = model_dir or str(get_model_path()["feature_engine"].parent)

        os.makedirs(self.model_dir, exist_ok=True)

        self.feature_engine = FeatureEngineeringEngine()
        self.xgboost_model = xgb.XGBRegressor(
            n_estimators=self.n_estimators,
            max_depth=self.max_depth,
            learning_rate=self.learning_rate,
            subsample=self.subsample,
            colsample_bytree=self.colsample_bytree,
            reg_alpha=self.reg_alpha,
            reg_lambda=self.reg_lambda,
            random_state=self.random_state,
            n_jobs=self.n_jobs,
            enable_categorical=False,
        )

        self.is_trained = False
        self.feature_names = None
        self.enhanced_feature_names = None

        self.training_X_enhanced_mean: Optional[np.ndarray] = None
        self.training_X_enhanced_std: Optional[np.ndarray] = None
        self.training_rmse: Optional[float] = None

        self.uncertainty_num_samples: int = 20
        self.uncertainty_noise_scale: float = 0.1

    def train(
        self,
        X: np.ndarray,
        y: np.ndarray,
        feature_names: List[str] = None,
        cv_folds: int = 5,
    ) -> Dict[str, float]:

        print("Starting XGBoost model training...")
        print(f"Feature matrix shape: {X.shape}")
        print(f"Target values shape: {y.shape}")

        if feature_names is None:
            feature_names = [f"feature_{i}" for i in range(X.shape[1])]

        self.feature_names = feature_names

        print("Training feature engineering engine...")
        self.feature_engine.fit(X, y, feature_names)

        print("Generating enhanced features...")
        X_enhanced = self.feature_engine.transform(X)
        self.enhanced_feature_names = self.feature_engine.get_enhanced_feature_names()

        print(f"Enhanced feature matrix shape: {X_enhanced.shape}")

        print("Training XGBoost model...")
        self.xgboost_model.fit(X_enhanced, y)

        print("Performing cross-validation evaluation...")
        cv_scores = cross_val_score(
            self.xgboost_model, X_enhanced, y, cv=cv_folds, scoring="r2"
        )

        y_pred = self.xgboost_model.predict(X_enhanced)
        mse = mean_squared_error(y, y_pred)
        r2 = r2_score(y, y_pred)

        self.training_rmse = float(np.sqrt(mse))

        self.training_X_enhanced_mean = np.mean(X_enhanced, axis=0)
        self.training_X_enhanced_std = np.std(X_enhanced, axis=0) + 1e-8

        feature_importance = self.xgboost_model.feature_importances_

        self.is_trained = True

        results = {
            "mse": mse,
            "r2": r2,
            "cv_r2_mean": cv_scores.mean(),
            "cv_r2_std": cv_scores.std(),
            "feature_importance": feature_importance.tolist(),
            "n_features": X.shape[1],
            "n_enhanced_features": X_enhanced.shape[1],
            "rmse": self.training_rmse,
        }

        print("Training completed!")
        print(f"MSE: {mse:.4f}")
        print(f"R²: {r2:.4f}")
        print("Cross-validation R²: " f"{cv_scores.mean():.4f} ± {cv_scores.std():.4f}")
        print(f"RMSE: {self.training_rmse:.4f}")

        return results

    def predict(self, X: np.ndarray) -> Tuple[np.ndarray, np.ndarray]:

        if not self.is_trained:
            raise ValueError("模型尚未训练")

        X_enhanced = self.feature_engine.transform(X)

        predictions = self.xgboost_model.predict(X_enhanced)

        pred_std = self._estimate_pred_std_mc(X_enhanced)

        rmse = self.training_rmse if self.training_rmse is not None else 1.0
        confidence = 1.0 / (1.0 + (pred_std / (rmse + 1e-8)))
        confidence = np.clip(confidence, 0.0, 1.0)

        return predictions, confidence

    def save_model(self, model_name: str = "surrogate_model_data1"):

        if not self.is_trained:
            raise ValueError("模型尚未训练")

        feature_engine_path = os.path.join(
            self.model_dir, f"{model_name}_feature_engine.pkl"
        )
        joblib.dump(self.feature_engine, feature_engine_path)

        xgboost_path = os.path.join(
            self.model_dir,
            f"{model_name}_xgboost.pkl",
        )
        joblib.dump(self.xgboost_model, xgboost_path)

        model_info = {
            "feature_names": self.feature_names,
            "enhanced_feature_names": self.enhanced_feature_names,
            "is_trained": self.is_trained,
            "model_params": {
                "n_estimators": self.n_estimators,
                "max_depth": self.max_depth,
                "learning_rate": self.learning_rate,
                "subsample": self.subsample,
                "colsample_bytree": self.colsample_bytree,
                "reg_alpha": self.reg_alpha,
                "reg_lambda": self.reg_lambda,
                "random_state": self.random_state,
            },
            "training_rmse": self.training_rmse,
            "training_X_enhanced_mean": self.training_X_enhanced_mean,
            "training_X_enhanced_std": self.training_X_enhanced_std,
            "uncertainty_num_samples": self.uncertainty_num_samples,
            "uncertainty_noise_scale": self.uncertainty_noise_scale,
        }

        model_info_path = os.path.join(
            self.model_dir,
            f"{model_name}_info.pkl",
        )
        joblib.dump(model_info, model_info_path)

        print(f"模型已保存到: {self.model_dir}")
        print(f"特征工程引擎: {feature_engine_path}")
        print(f"XGBoost模型: {xgboost_path}")
        print(f"模型信息: {model_info_path}")

    def load_model(self, model_name: str = "surrogate_model_data1"):

        feature_engine_path = os.path.join(
            self.model_dir, f"{model_name}_feature_engine.pkl"
        )
        self.feature_engine = joblib.load(feature_engine_path)

        xgboost_path = os.path.join(
            self.model_dir,
            f"{model_name}_xgboost.pkl",
        )
        self.xgboost_model = joblib.load(xgboost_path)

        model_info_path = os.path.join(
            self.model_dir,
            f"{model_name}_info.pkl",
        )
        model_info = joblib.load(model_info_path)

        self.feature_names = model_info["feature_names"]
        self.enhanced_feature_names = model_info["enhanced_feature_names"]
        self.is_trained = model_info["is_trained"]

        self.training_rmse = model_info.get("training_rmse", None)
        self.training_X_enhanced_mean = model_info.get("training_X_enhanced_mean", None)
        self.training_X_enhanced_std = model_info.get("training_X_enhanced_std", None)
        self.uncertainty_num_samples = model_info.get("uncertainty_num_samples", 20)
        self.uncertainty_noise_scale = model_info.get("uncertainty_noise_scale", 0.1)

        print(f"模型已从 {self.model_dir} 加载")
        print(f"特征数量: {len(self.feature_names)}")
        print(f"增强特征数量: {len(self.enhanced_feature_names)}")

    def get_feature_importance(self) -> Dict[str, float]:

        if not self.is_trained:
            raise ValueError("模型尚未训练")

        importance = self.xgboost_model.feature_importances_
        feature_importance = {}

        if self.enhanced_feature_names:
            for name, imp in zip(self.enhanced_feature_names, importance):
                feature_importance[name] = float(imp)
        else:
            for i, imp in enumerate(importance):
                feature_importance[f"feature_{i}"] = float(imp)

        return feature_importance

    def get_top_features(self, top_k: int = 10) -> List[Tuple[str, float]]:

        feature_importance = self.get_feature_importance()
        sorted_features = sorted(
            feature_importance.items(), key=lambda x: x[1], reverse=True
        )
        return sorted_features[:top_k]

    def _estimate_pred_std_mc(self, X_enhanced: np.ndarray) -> np.ndarray:

        if self.training_X_enhanced_std is None:

            return np.zeros(X_enhanced.shape[0])

        num_samples = int(self.uncertainty_num_samples)
        noise_scale = float(self.uncertainty_noise_scale)

        N, D = X_enhanced.shape

        std_vec = self.training_X_enhanced_std.reshape(1, D)

        repeated = np.repeat(X_enhanced, num_samples, axis=0)
        random_noise = np.random.normal(
            loc=0.0,
            scale=1.0,
            size=repeated.shape,
        )
        perturbed = repeated + noise_scale * random_noise * std_vec

        perturbed_pred = self.xgboost_model.predict(perturbed)
        perturbed_pred = perturbed_pred.reshape(N, num_samples)
        pred_std = np.std(perturbed_pred, axis=1)
        return pred_std
