




import os
import sys
import json
import shutil
from pathlib import Path
from typing import List, Dict, Any


sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def extract_test_configs(
    test_set_file: str, output_dir: str, num_configs: int = None
) -> None:

    print(f"加载测试集文件: {test_set_file}")


    with open(test_set_file, "r", encoding="utf-8") as f:
        test_configs = json.load(f)

    print(f"测试集包含 {len(test_configs)} 个配置")


    output_path = Path(output_dir)
    output_path.mkdir(parents=True, exist_ok=True)


    if num_configs is None:
        num_configs = len(test_configs)
    else:
        num_configs = min(num_configs, len(test_configs))

    print(f"提取 {num_configs} 个配置到: {output_path}")


    for i in range(num_configs):
        config = test_configs[i]


        config_dir = output_path / f"config_{i+1:04d}"
        config_dir.mkdir(exist_ok=True)


        config_file = config_dir / "config.json"
        with open(config_file, "w", encoding="utf-8") as f:
            json.dump(config.get("config", {}), f, indent=2, ensure_ascii=False)


        metadata = {
            "config_id": f"config_{i+1:04d}",
            "actual_fitness": config.get("final_score", 0.0),
            "original_index": i,
        }

        metadata_file = config_dir / "metadata.json"
        with open(metadata_file, "w", encoding="utf-8") as f:
            json.dump(metadata, f, indent=2, ensure_ascii=False)

        if i % 50 == 0:
            print(f"已提取 {i+1}/{num_configs} 个配置")


    index_data = {
        "total_configs": num_configs,
        "source_file": test_set_file,
        "output_directory": str(output_path),
        "configs": [],
    }

    for i in range(num_configs):
        config = test_configs[i]
        index_data["configs"].append(
            {
                "config_id": f"config_{i+1:04d}",
                "config_dir": f"config_{i+1:04d}",
                "actual_fitness": config.get("final_score", 0.0),
                "config_file": f"config_{i+1:04d}/config.json",
                "metadata_file": f"config_{i+1:04d}/metadata.json",
            }
        )

    index_file = output_path / "index.json"
    with open(index_file, "w", encoding="utf-8") as f:
        json.dump(index_data, f, indent=2, ensure_ascii=False)

    print(f"✅ 配置提取完成!")
    print(f"输出目录: {output_path}")
    print(f"索引文件: {index_file}")
    print(f"配置数量: {num_configs}")


def main():

    import argparse

    parser = argparse.ArgumentParser(description="提取测试集配置到单独文件夹")
    parser.add_argument(
        "--test-set-file",
        type=str,
        default="data/test_configs_1000.json",
        help="测试集文件路径",
    )
    parser.add_argument(
        "--output-dir", type=str, default="data/extracted_test_configs", help="输出目录"
    )
    parser.add_argument(
        "--num-configs", type=int, default=None, help="要提取的配置数量（默认全部）"
    )

    args = parser.parse_args()

    if not os.path.exists(args.test_set_file):
        print(f"错误: 测试集文件不存在: {args.test_set_file}")
        sys.exit(1)

    try:
        extract_test_configs(args.test_set_file, args.output_dir, args.num_configs)
    except Exception as e:
        print(f"提取失败: {e}")
        import traceback

        traceback.print_exc()
        sys.exit(1)


if __name__ == "__main__":
    main()
