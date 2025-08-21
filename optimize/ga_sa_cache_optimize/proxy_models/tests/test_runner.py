




import os
import sys
import argparse
from pathlib import Path


sys.path.insert(
    0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
)

from proxy_models.tests.reorganized_test_interface import ReorganizedTestInterface


def run_reorganized_test(args):

    print("=" * 80)
    print("代理模型测试 - Reorganized Test Set")
    print("=" * 80)


    try:
        test_interface = ReorganizedTestInterface(
            test_set_path=args.test_set_path, model_dir=args.model_dir
        )
    except Exception as e:
        print(f"初始化测试接口失败: {e}")
        return 1


    test_info = test_interface.get_test_set_info()
    print(f"\n测试集信息:")
    print(f"总配置数: {test_info['total_configs']}")
    print(f"每批次配置数: {test_info['configs_per_folder']}")
    print(f"批次数: {test_info['num_folders']}")


    try:
        if args.single_batch:

            print(f"\n测试单个批次: {args.single_batch}")
            results = test_interface.test_single_batch(
                batch_number=args.single_batch, use_surrogate=not args.no_surrogate
            )


            all_results = {
                "test_info": {
                    "test_time": "single_batch_test",
                    "use_surrogate": not args.no_surrogate,
                    "total_batches": 1,
                    "total_configs": test_info["configs_per_folder"],
                },
                "batch_results": {args.single_batch: results},
                "overall_stats": test_interface._calculate_overall_stats(
                    {"batch_results": {args.single_batch: results}}
                ),
            }
        else:

            print(f"\n测试所有批次 (最大: {args.max_batches or '全部'})")
            results = test_interface.test_all_batches(
                use_surrogate=not args.no_surrogate, max_batches=args.max_batches
            )
            all_results = results


        test_interface.print_summary(all_results)


        if args.output_dir:
            output_path = test_interface.save_results(all_results, args.output_dir)


            if args.generate_plots:
                test_interface.generate_plots(all_results, args.output_dir)

        return 0

    except Exception as e:
        print(f"测试运行失败: {e}")
        return 1


def main():

    parser = argparse.ArgumentParser(
        description="代理模型测试运行器",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=,
    )

    subparsers = parser.add_subparsers(dest="command", help="测试命令")


    reorganized_parser = subparsers.add_parser(
        "reorganized", help="运行reorganized_test_set测试"
    )

    reorganized_parser.add_argument(
        "--test-set-path", type=str, required=True, help="reorganized_test_set目录路径"
    )

    reorganized_parser.add_argument(
        "--model-dir",
        type=str,
        default="data/models",
        help="模型目录路径 (默认: data/models)",
    )

    reorganized_parser.add_argument(
        "--single-batch", type=int, help="测试单个批次 (指定批次编号)"
    )

    reorganized_parser.add_argument(
        "--max-batches", type=int, help="最大测试批次数量 (默认: 全部)"
    )

    reorganized_parser.add_argument(
        "--no-surrogate", action="store_true", help="不使用代理模型，使用直接预测"
    )

    reorganized_parser.add_argument(
        "--output-dir", type=str, help="输出目录 (默认: test_results)"
    )

    reorganized_parser.add_argument(
        "--generate-plots", action="store_true", help="生成测试结果图表"
    )


    args = parser.parse_args()

    if not args.command:
        parser.print_help()
        return 1


    if args.command == "reorganized":
        return run_reorganized_test(args)
    else:
        print(f"未知命令: {args.command}")
        return 1


if __name__ == "__main__":
    sys.exit(main())
