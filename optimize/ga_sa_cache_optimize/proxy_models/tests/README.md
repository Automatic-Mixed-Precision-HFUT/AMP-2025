# 代理模型测试系统

这个目录包含了代理模型系统的各种测试工具和接口，专门用于验证和评估代理模型的性能。

## 📁 目录结构

```
tests/
├── __init__.py                           # 测试包初始化
├── README.md                             # 本文件
├── reorganized_test_interface.py         # 重组测试集接口
├── test_runner.py                        # 测试运行器
├── example_usage.py                      # 使用示例
├── direct_predict.py                     # 直接预测脚本
├── extract_test_configs.py               # 提取测试配置脚本
├── create_test_set.py                    # 创建测试集脚本
├── predict_batch_folders.py              # 批量文件夹预测脚本
└── reorganize_test_set.py                # 重组测试集脚本
```

## 🚀 快速开始

### 1. 使用测试运行器

```bash
# 测试单个批次
python test_runner.py reorganized --test-set-path data/reorganized_test_set --single-batch 1

# 测试所有批次
python test_runner.py reorganized --test-set-path data/reorganized_test_set

# 测试前5个批次，不使用代理模型
python test_runner.py reorganized --test-set-path data/reorganized_test_set --max-batches 5 --no-surrogate

# 测试并生成图表
python test_runner.py reorganized --test-set-path data/reorganized_test_set --generate-plots --output-dir test_results
```

### 2. 使用Python接口

```python
from proxy_models.tests.reorganized_test_interface import ReorganizedTestInterface

# 初始化测试接口
interface = ReorganizedTestInterface(
    test_set_path="data/reorganized_test_set",
    model_dir="data/models"
)

# 测试单个批次
results = interface.test_single_batch(batch_number=1, use_surrogate=True)

# 测试所有批次
all_results = interface.test_all_batches(use_surrogate=True, max_batches=3)

# 保存结果
interface.save_results(all_results, "test_results")

# 生成图表
interface.generate_plots(all_results, "test_results")

# 打印摘要
interface.print_summary(all_results)
```

### 3. 运行示例

```bash
python example_usage.py
```

## 📊 测试接口功能

### ReorganizedTestInterface

专门处理 `reorganized_test_set` 格式数据的测试接口，提供以下功能：

- **批量测试**: 支持测试单个批次或所有批次
- **多种预测模式**: 支持代理模型预测和直接预测
- **结果分析**: 自动计算误差统计信息
- **结果保存**: 保存详细结果和摘要
- **图表生成**: 生成预测结果可视化图表

#### 主要方法

- `get_test_set_info()`: 获取测试集信息
- `test_single_batch()`: 测试单个批次
- `test_all_batches()`: 测试所有批次
- `save_results()`: 保存测试结果
- `generate_plots()`: 生成结果图表
- `print_summary()`: 打印结果摘要

## 📈 测试结果

测试完成后会生成以下文件：

### 结果文件
- `detailed_results_YYYYMMDD_HHMMSS.json`: 详细测试结果
- `summary_YYYYMMDD_HHMMSS.json`: 总体统计摘要

### 图表文件
- `test_plots_YYYYMMDD_HHMMSS.png`: 测试结果图表，包含：
  - 预测值 vs 实际值散点图
  - 误差分布直方图
  - 各批次平均误差
  - 置信度分布

## 🔧 工具脚本

### 数据准备工具

1. **create_test_set.py**: 从train_data1创建测试集
2. **reorganize_test_set.py**: 将测试集重组为批次格式
3. **extract_test_configs.py**: 提取测试配置到单独文件夹

### 预测工具

1. **direct_predict.py**: 直接预测单个配置
2. **predict_batch_folders.py**: 批量预测文件夹中的配置

## 📋 测试数据格式

### Reorganized Test Set 格式

```
reorganized_test_set/
├── overall_index.json           # 总体索引文件
├── overall_prediction_results.json
├── overall_prediction_summary.json
├── batch_01/                    # 批次1
│   ├── index.json              # 批次索引
│   ├── prediction_results.json
│   ├── prediction_summary.json
│   └── config_0001/            # 配置1
│       ├── config.json         # 配置文件
│       └── metadata.json       # 元数据
├── batch_02/                    # 批次2
│   └── ...
└── ...
```

### 索引文件格式

```json
{
  "total_configs": 1000,
  "configs_per_folder": 50,
  "num_folders": 20,
  "folders": [
    {
      "folder_name": "batch_01",
      "batch_number": 1,
      "configs_count": 50,
      "index_file": "batch_01/index.json"
    }
  ]
}
```

## 🎯 使用场景

1. **模型验证**: 验证训练好的代理模型性能
2. **性能对比**: 对比代理模型和直接预测的性能
3. **误差分析**: 分析预测误差的分布和特征
4. **批次测试**: 分批次测试大量配置
5. **结果可视化**: 生成直观的测试结果图表

## ⚠️ 注意事项

1. 确保测试集路径存在且格式正确
2. 确保模型文件已训练并保存
3. 大量配置测试可能需要较长时间
4. 图表生成需要matplotlib依赖

## 🔍 故障排除

### 常见问题

1. **测试集路径不存在**
   - 运行 `reorganize_test_set.py` 创建测试集

2. **模型文件不存在**
   - 运行 `pre_train_data1_surrogate.py` 训练模型

3. **预测失败**
   - 检查配置文件格式是否正确
   - 检查模型文件是否完整

4. **图表生成失败**
   - 安装matplotlib: `pip install matplotlib`
   - 确保有足够的磁盘空间
