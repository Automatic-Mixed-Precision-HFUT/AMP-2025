




import numpy as np
from typing import Dict, List
from sklearn.ensemble import GradientBoostingRegressor
from sklearn.preprocessing import StandardScaler
import joblib
import os


class FeatureEngineeringEngine:


    def __init__(
        self,
        n_estimators: int = 100,
        learning_rate: float = 0.1,
        max_depth: int = 6,
        subsample: float = 0.8,
        random_state: int = 42,
    ):

        self.n_estimators = n_estimators
        self.learning_rate = learning_rate
        self.max_depth = max_depth
        self.subsample = subsample
        self.random_state = random_state

        self.gbdt_model = None
        self.feature_names = None
        self.feature_importance = None
        self.scaler = StandardScaler()
        self.is_fitted = False

    def fit(
        self, X: np.ndarray, y: np.ndarray, feature_names: List[str] = None
    ) -> "FeatureEngineeringEngine":
        
        if feature_names is None:
            feature_names = [f"feature_{i}" for i in range(X.shape[1])]

        self.feature_names = feature_names


        X_scaled = self.scaler.fit_transform(X)


        self.gbdt_model = GradientBoostingRegressor(
            n_estimators=self.n_estimators,
            learning_rate=self.learning_rate,
            max_depth=self.max_depth,
            subsample=self.subsample,
            random_state=self.random_state,
        )

        self.gbdt_model.fit(X_scaled, y)


        self.feature_importance = self.gbdt_model.feature_importances_

        self.is_fitted = True
        return self

    def transform(self, X: np.ndarray) -> np.ndarray:

        if not self.is_fitted:
            raise ValueError("模型尚未训练，请先调用fit方法")


        X_scaled = self.scaler.transform(X)


        leaf_features = self.gbdt_model.apply(X_scaled)


        interaction_features = self._create_interaction_features(X_scaled)


        X_enhanced = np.column_stack(
            [
                X_scaled,
                leaf_features,
                interaction_features,
            ]
        )

        return X_enhanced

    def _create_interaction_features(self, X: np.ndarray) -> np.ndarray:

        n_samples, n_features = X.shape
        interaction_features = []


        if "double_ratio" in self.feature_names and "float_ratio" in self.feature_names:
            double_idx = self.feature_names.index("double_ratio")
            float_idx = self.feature_names.index("float_ratio")


            precision_balance = X[:, double_idx] * X[:, float_idx]
            interaction_features.append(precision_balance)


            precision_diff = np.abs(X[:, double_idx] - X[:, float_idx])
            interaction_features.append(precision_diff)


        if "vectorization_level" in self.feature_names:
            vec_idx = self.feature_names.index("vectorization_level")


            for i, name in enumerate(self.feature_names):
                if "ratio" in name:
                    vec_precision = X[:, vec_idx] * X[:, i]
                    interaction_features.append(vec_precision)


        if "loop_unroll_factor" in self.feature_names:
            loop_idx = self.feature_names.index("loop_unroll_factor")


            for i, name in enumerate(self.feature_names):
                if "ratio" in name:
                    loop_precision = X[:, loop_idx] * X[:, i]
                    interaction_features.append(loop_precision)


        if not interaction_features:
            for i in range(min(n_features, 3)):
                for j in range(i + 1, min(n_features, 4)):
                    interaction = X[:, i] * X[:, j]
                    interaction_features.append(interaction)

        return (
            np.column_stack(interaction_features)
            if interaction_features
            else np.empty((n_samples, 0))
        )

    def get_feature_importance(self) -> Dict[str, float]:

        if not self.is_fitted:
            raise ValueError("模型尚未训练")

        return dict(zip(self.feature_names, self.feature_importance))

    def get_enhanced_feature_names(self) -> List[str]:

        if not self.is_fitted:
            raise ValueError("模型尚未训练")

        enhanced_names = []


        enhanced_names.extend([f"scaled_{name}" for name in self.feature_names])


        n_trees = self.gbdt_model.n_estimators
        enhanced_names.extend([f"leaf_{i}" for i in range(n_trees)])


        n_interactions = self._create_interaction_features(
            np.zeros((1, len(self.feature_names)))
        ).shape[1]
        enhanced_names.extend([f"interaction_{i}" for i in range(n_interactions)])

        return enhanced_names

    def save_model(self, filepath: str):

        if not self.is_fitted:
            raise ValueError("模型尚未训练")

        model_data = {
            "gbdt_model": self.gbdt_model,
            "feature_names": self.feature_names,
            "feature_importance": self.feature_importance,
            "scaler": self.scaler,
            "is_fitted": self.is_fitted,
        }

        joblib.dump(model_data, filepath)

    def load_model(self, filepath: str):

        if not os.path.exists(filepath):
            raise FileNotFoundError(f"模型文件不存在: {filepath}")

        model_data = joblib.load(filepath)

        self.gbdt_model = model_data["gbdt_model"]
        self.feature_names = model_data["feature_names"]
        self.feature_importance = model_data["feature_importance"]
        self.scaler = model_data["scaler"]
        self.is_fitted = model_data["is_fitted"]
