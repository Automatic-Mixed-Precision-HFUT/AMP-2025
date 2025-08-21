




import os
import sys
import json
import shutil
from pathlib import Path


sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def reorganize_test_set(
    source_dir: str, output_base_dir: str, configs_per_folder: int = 50
) -> None:

    source_path = Path(source_dir)
    output_base_path = Path(output_base_dir)

    if not source_path.exists():
        print(f"错误: 源目录不存在: {source_dir}")
        return


    index_file = source_path / "index.json"
    if not index_file.exists():
        print(f"错误: 索引文件不存在: {index_file}")
        return

    with open(index_file, "r", encoding="utf-8") as f:
        index_data = json.load(f)

    total_configs = index_data["total_configs"]
    configs = index_data["configs"]

    print(f"源目录包含 {total_configs} 个配置")
    print(f"每个文件夹将包含 {configs_per_folder} 个配置")


    num_folders = (total_configs + configs_per_folder - 1) // configs_per_folder
    print(f"将创建 {num_folders} 个文件夹")


    output_base_path.mkdir(parents=True, exist_ok=True)


    overall_index = {
        "total_configs": total_configs,
        "configs_per_folder": configs_per_folder,
        "num_folders": num_folders,
        "folders": [],
    }


    for folder_idx in range(num_folders):
        start_idx = folder_idx * configs_per_folder
        end_idx = min(start_idx + configs_per_folder, total_configs)

        folder_name = f"batch_{folder_idx+1:02d}"
        folder_path = output_base_path / folder_name
        folder_path.mkdir(exist_ok=True)

        print(f"\n创建文件夹: {folder_name}")
        print(f"配置范围: {start_idx+1} - {end_idx}")


        batch_configs = configs[start_idx:end_idx]


        batch_index = {
            "folder_name": folder_name,
            "batch_number": folder_idx + 1,
            "configs_in_batch": len(batch_configs),
            "configs": [],
        }


        for i, config_info in enumerate(batch_configs):
            config_id = config_info["config_id"]
            source_config_dir = source_path / config_id

            if source_config_dir.exists():

                dest_config_dir = folder_path / config_id
                if dest_config_dir.exists():
                    shutil.rmtree(dest_config_dir)
                shutil.copytree(source_config_dir, dest_config_dir)


                config_info_copy = config_info.copy()
                config_info_copy["local_index"] = i + 1
                batch_index["configs"].append(config_info_copy)

                print(f"  复制配置 {config_id}")
            else:
                print(f"  警告: 配置目录不存在: {source_config_dir}")


        batch_index_file = folder_path / "index.json"
        with open(batch_index_file, "w", encoding="utf-8") as f:
            json.dump(batch_index, f, indent=2, ensure_ascii=False)


        overall_index["folders"].append(
            {
                "folder_name": folder_name,
                "batch_number": folder_idx + 1,
                "configs_count": len(batch_configs),
                "index_file": f"{folder_name}/index.json",
            }
        )

        print(f"  完成文件夹 {folder_name}: {len(batch_configs)} 个配置")


    overall_index_file = output_base_path / "overall_index.json"
    with open(overall_index_file, "w", encoding="utf-8") as f:
        json.dump(overall_index, f, indent=2, ensure_ascii=False)

    print(f"\n✅ 重新组织完成!")
    print(f"输出目录: {output_base_path}")
    print(f"总体索引文件: {overall_index_file}")
    print(f"创建了 {num_folders} 个文件夹")
    print(f"总共处理了 {total_configs} 个配置")


def main():

    import argparse

    parser = argparse.ArgumentParser(description="重新组织测试集数据")
    parser.add_argument(
        "--source-dir",
        type=str,
        default="data/extracted_test_configs",
        help="源配置目录",
    )
    parser.add_argument(
        "--output-dir",
        type=str,
        default="data/reorganized_test_set",
        help="输出基础目录",
    )
    parser.add_argument(
        "--configs-per-folder", type=int, default=50, help="每个文件夹的配置数量"
    )

    args = parser.parse_args()

    if not os.path.exists(args.source_dir):
        print(f"错误: 源目录不存在: {args.source_dir}")
        sys.exit(1)

    try:
        reorganize_test_set(args.source_dir, args.output_dir, args.configs_per_folder)
    except Exception as e:
        print(f"重新组织失败: {e}")
        import traceback

        traceback.print_exc()
        sys.exit(1)


if __name__ == "__main__":
    main()
