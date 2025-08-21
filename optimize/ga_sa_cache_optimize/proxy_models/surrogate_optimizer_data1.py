import numpy as np
import json
import os
from typing import Dict, List, Tuple, Any
from .xgboost_model_manager_data1 import XGBoostModelManagerData1
from proxy_models.config import (
    get_model_path,
    get_data_path,
    get_log_path,
    MODEL_CONFIG,
)


class SurrogateOptimizerData1:

    def __init__(
        self,
        model_dir: str = None,
        confidence_threshold: float = None,
        min_training_samples: int = None,
        enable_online_training: bool = False,
    ):

        self.model_dir = model_dir or str(get_model_path()["feature_engine"].parent)
        self.confidence_threshold = (
            confidence_threshold
            or MODEL_CONFIG["surrogate_optimizer"]["confidence_threshold"]
        )
        self.min_training_samples = (
            min_training_samples
            or MODEL_CONFIG["surrogate_optimizer"]["min_training_samples"]
        )
        self.enable_online_training = enable_online_training

        self.model_manager = XGBoostModelManagerData1(model_dir=self.model_dir)

        self.training_data = {
            "configs": [],
            "fitness_values": [],
            "feature_names": None,
        }

        self.model_ready = False
        self.last_training_size = 0

    def add_training_data(self, config: Dict[str, Any], fitness: float):

        self.training_data["configs"].append(config)
        self.training_data["fitness_values"].append(fitness)

        if self.training_data["feature_names"] is None:
            self.training_data["feature_names"] = list(config.keys())

        if self.enable_online_training:

            current_size = len(self.training_data["configs"])
            if (
                current_size >= self.min_training_samples
                and current_size > self.last_training_size + 5
            ):

                self._train_model()
                self.last_training_size = current_size

    def _train_model(self):

        if len(self.training_data["configs"]) < self.min_training_samples:
            return

        print(
            f"Starting surrogate model training with {len(self.training_data['configs'])} samples..."
        )

        try:

            X, y = self._prepare_training_data()

            results = self.model_manager.train(
                X, y, self.training_data["feature_names"]
            )

            self.model_ready = True
            print(f"Model training completed, R² = {results['r2']:.4f}")

        except Exception as e:
            print(f"Model training failed: {e}")
            self.model_ready = False

    def _prepare_training_data(self) -> Tuple[np.ndarray, np.ndarray]:

        configs = self.training_data["configs"]
        fitness_values = self.training_data["fitness_values"]
        feature_names = self.training_data["feature_names"]

        X = []
        for config in configs:
            features = [config.get(name, 0.0) for name in feature_names]
            X.append(features)

        X = np.array(X)
        y = np.array(fitness_values)

        return X, y

    def predict_fitness(self, config: Dict[str, Any]) -> Tuple[float, float]:

        if not self.model_ready:
            return 0.0, 0.0

        try:

            feature_names = self.training_data["feature_names"]
            features = [config.get(name, 0.0) for name in feature_names]
            X = np.array([features]).reshape(1, -1)

            predictions, confidence = self.model_manager.predict(X)

            return float(predictions[0]), float(confidence[0])

        except Exception as e:
            print(f"Prediction failed: {e}")
            return 0.0, 0.0

    def should_use_surrogate(self, confidence: float) -> bool:

        return confidence >= self.confidence_threshold

    def load_training_data(self, data_file: str):

        try:
            with open(data_file, "r", encoding="utf-8") as f:
                data = json.load(f)

            if isinstance(data, dict) and "X" in data and "y" in data:

                X = np.array(data["X"])
                y = np.array(data["y"])
                feature_names = data.get("feature_names", [])

                configs = []
                for i in range(len(X)):
                    config = dict(zip(feature_names, X[i]))
                    configs.append(config)

                self.training_data["configs"] = configs
                self.training_data["fitness_values"] = y.tolist()
                self.training_data["feature_names"] = feature_names

                print(f"Loaded {len(configs)} samples from {data_file}")

                if len(configs) >= self.min_training_samples:
                    self._train_model()

            else:
                print(f"Unsupported data format: {data_file}")

        except Exception as e:
            print(f"Failed to load training data: {e}")

    def save_model(self, model_name: str = "surrogate_model_data1"):

        if self.model_ready:
            self.model_manager.save_model(model_name)
        else:
            print("Model not trained yet, cannot save")

    def load_model(self, model_name: str = "surrogate_model_data1"):

        try:
            self.model_manager.load_model(model_name)
            self.model_ready = True

            if self.model_manager.feature_names:
                self.training_data["feature_names"] = self.model_manager.feature_names
                print(f"Feature names loaded: {len(self.model_manager.feature_names)}")

            print("Model loaded successfully")
        except Exception as e:
            print(f"Model loading failed: {e}")
            self.model_ready = False

    def get_feature_importance(self) -> Dict[str, float]:

        if self.model_ready:
            return self.model_manager.get_feature_importance()
        else:
            return {}

    def get_top_features(self, top_k: int = 10) -> List[Tuple[str, float]]:

        if self.model_ready:
            return self.model_manager.get_top_features(top_k)
        else:
            return []

    def get_model_status(self) -> Dict[str, Any]:

        return {
            "model_ready": self.model_ready,
            "training_samples": len(self.training_data["configs"]),
            "min_training_samples": self.min_training_samples,
            "confidence_threshold": self.confidence_threshold,
            "feature_names": self.training_data["feature_names"],
        }
