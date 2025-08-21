# 代理模型系统使用指南

## 概述

这个代理模型系统是一个用于混合精度优化的智能代理，可以预测不同配置的性能，从而加速优化过程。系统包含特征工程引擎、XGBoost模型和代理优化器三个核心组件。

## 系统架构

```
proxy_models/
├── config.py                    # 配置文件
├── xgboost_model_manager_data1.py  # XGBoost模型管理器
├── surrogate_optimizer_data1.py    # 代理优化器
├── feature_engineering.py       # 特征工程引擎
├── prepare_train_data1.py       # 数据预处理脚本
├── pre_train_data1_surrogate.py # 预训练脚本
├── generate_feature_names.py    # 特征名称生成脚本
├── models/                      # 模型存储目录
├── data/                        # 数据存储目录
└── logs/                        # 日志存储目录
```

## 快速开始

### 1. 数据预处理

首先，您需要将train_data1格式的数据转换为代理模型可以使用的格式：

```bash
cd proxy_models
python prepare_train_data1.py
```

这个脚本会：
- 读取 `data/train_data1/` 目录中的数据
- 提取精度、函数和复杂度特征
- 计算适应度分数
- 生成训练矩阵和特征名称文件

### 2. 预训练代理模型

使用预处理后的数据训练代理模型：

```bash
python pre_train_data1_surrogate.py
```

这个脚本会：
- 加载预处理后的训练数据
- 训练XGBoost模型和特征工程引擎
- 评估模型性能
- 生成训练结果图表
- 保存训练好的模型

### 3. 在优化过程中使用

训练完成后，您可以在优化过程中使用代理模型：

```python
from proxy_models.surrogate_optimizer_data1 import SurrogateOptimizerData1

# 创建代理优化器
optimizer = SurrogateOptimizerData1()

# 加载预训练的数据
optimizer.load_training_data('processed_train_data1.json')

# 预测新配置的性能
config = {
    'double_ratio': 0.3,
    'float_ratio': 0.5,
    'half_ratio': 0.2,
    'total_variables': 40,
    'function_diversity': 0.6,
    'pointer_ratio': 0.3,
    'array_ratio': 0.1
}

predicted_fitness, confidence = optimizer.predict_fitness(config)
print(f"预测适应度分数: {predicted_fitness:.2f}")
print(f"置信度: {confidence:.2f}")
```

## 特征说明

系统自动提取以下特征：

### 精度特征
- `double_ratio`: double精度变量比例
- `float_ratio`: float精度变量比例
- `half_ratio`: half精度变量比例
- `total_variables`: 总变量数量

### 函数特征
- `dgemm_ratio`: dgemm函数变量比例
- `dtrsm_ratio`: dtrsm函数变量比例
- `dlange_ratio`: dlange函数变量比例
- `dgemv_ratio`: dgemv函数变量比例
- `copy_mat_ratio`: copy_mat函数变量比例
- `dgetrf_nopiv_ratio`: dgetrf_nopiv函数变量比例
- `dgetrf2_nopiv_ratio`: dgetrf2_nopiv函数变量比例
- `rotmat_ratio`: rotmat函数变量比例
- `gmres_ratio`: gmres函数变量比例
- `function_diversity`: 函数多样性

### 复杂度特征
- `pointer_ratio`: 指针类型变量比例
- `array_ratio`: 数组类型变量比例

## 配置参数

### 模型配置
```python
MODEL_CONFIG = {
    "xgboost": {
        "n_estimators": 200,      # XGBoost中树的数量
        "max_depth": 6,           # 树的最大深度
        "learning_rate": 0.1,     # 学习率
        "subsample": 0.8,         # 样本采样比例
        "colsample_bytree": 0.8,  # 特征采样比例
        "reg_alpha": 0.1,         # L1正则化参数
        "reg_lambda": 1.0,        # L2正则化参数
        "random_state": 42,       # 随机种子
        "n_jobs": -1,             # 并行作业数
    },
    "surrogate_optimizer": {
        "confidence_threshold": 0.8,    # 置信度阈值
        "min_training_samples": 10,     # 开始使用代理模型的最小样本数
    }
}
```

### 特征配置
```python
FEATURE_CONFIG = {
    "precision_features": ["double_ratio", "float_ratio", "half_ratio"],
    "optimization_features": ["loop_unroll_factor", "vectorization_level"],
    "interaction_features": True,    # 是否生成交互特征
    "leaf_features": True,           # 是否生成叶子特征
}
```

## 高级用法

### 1. 自定义特征工程

您可以修改 `feature_engineering.py` 来添加新的特征：

```python
def extract_custom_features(self, config):
    """提取自定义特征"""
    features = {}
    # 添加您的特征提取逻辑
    return features
```

### 2. 模型超参数调优

在 `pre_train_data1_surrogate.py` 中修改模型参数：

```python
# 创建模型管理器时指定参数
manager = XGBoostModelManagerData1(
    n_estimators=300,      # 增加树的数量
    max_depth=8,           # 增加树的深度
    learning_rate=0.05     # 降低学习率
)
```

### 3. 集成到遗传算法

在您的GA+SA+Cache优化算法中集成代理模型：

```python
def evaluate_fitness_with_surrogate(individual, optimizer):
    """使用代理模型评估适应度"""

    # 检查是否可以使用代理模型
    if optimizer.should_use_surrogate(individual.config):
        # 使用代理模型快速预测
        predicted_fitness, confidence = optimizer.predict_fitness(individual.config)

        # 如果置信度高，直接返回预测结果
        if confidence > 0.8:
            return predicted_fitness

    # 否则运行完整的性能测试
    actual_fitness = run_full_performance_test(individual)

    # 将结果添加到训练数据中
    optimizer.add_training_data(individual.config, actual_fitness)

    return actual_fitness
```

## 监控和调试

### 1. 查看模型信息

```python
# 获取模型信息
info = optimizer.get_model_info()
print(f"模型状态: {info}")

# 获取特征重要性
importance = optimizer.get_feature_importance()
print(f"特征重要性: {importance}")
```

### 2. 保存和加载数据

```python
# 保存训练数据
optimizer.save_training_data("my_training_data.json")

# 加载已有数据
optimizer.load_training_data("my_training_data.json")
```

### 3. 查看训练日志

训练过程中的日志会保存在 `logs/` 目录下：
- `training.log`: 训练日志
- `prediction.log`: 预测日志
- `training_results.png`: 训练结果图表

## 性能优化建议

### 1. 数据质量
- 确保训练数据覆盖足够的配置空间
- 移除异常值和无效数据
- 平衡不同性能区间的样本数量

### 2. 特征选择
- 使用特征重要性分析选择最相关的特征
- 考虑添加领域特定的特征
- 定期重新评估特征的有效性

### 3. 模型更新
- 定期使用新数据重新训练模型
- 监控模型性能，及时调整超参数
- 使用交叉验证评估模型稳定性

## 故障排除

### 常见问题

1. **训练数据加载失败**
   - 检查文件路径是否正确
   - 确保数据文件格式正确
   - 检查文件权限

2. **模型训练失败**
   - 检查训练数据是否为空
   - 确保特征值都是数值类型
   - 检查内存是否足够

3. **预测结果不准确**
   - 增加训练样本数量
   - 调整模型超参数
   - 检查特征工程是否合适

### 调试技巧

1. 使用 `test_simple.py` 和 `test_system.py` 验证系统配置
2. 检查日志文件了解详细错误信息
3. 使用小数据集测试系统功能
4. 逐步增加数据量和复杂度

## 扩展开发

### 1. 添加新的模型类型

您可以扩展系统支持其他机器学习模型：

```python
from sklearn.ensemble import GradientBoostingRegressor

class ExtendedModelManager(ModelManager):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.gradient_boosting = GradientBoostingRegressor()
```

### 2. 支持新的特征类型

在 `feature_engineering.py` 中添加新的特征提取方法：

```python
def extract_loop_features(self, config):
    """提取循环相关特征"""
    # 实现循环特征提取逻辑
    pass
```

### 3. 集成其他优化算法

扩展系统支持其他优化算法：

```python
class MultiAlgorithmOptimizer:
    """多算法优化器"""
    def __init__(self):
        self.surrogate_optimizer = SurrogateOptimizer()
        self.bayesian_optimizer = BayesianOptimizer()
```

## 联系和支持

如果您在使用过程中遇到问题或有改进建议，请：

1. 检查本文档的故障排除部分
2. 查看代码注释和日志文件
3. 提交Issue或Pull Request

## 更新日志

- v1.0.0: 初始版本，支持基本的代理模型功能
- v1.1.0: 添加特征工程引擎
- v1.2.0: 支持预训练和模型保存/加载
- v1.3.0: 添加可视化功能和性能评估
