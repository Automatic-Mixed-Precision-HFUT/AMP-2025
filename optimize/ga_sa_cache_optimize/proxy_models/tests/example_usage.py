




import os
import sys
from pathlib import Path


sys.path.insert(
    0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
)

from proxy_models.tests.reorganized_test_interface import ReorganizedTestInterface


def example_single_batch_test():

    print("=" * 60)
    print("示例：测试单个批次")
    print("=" * 60)


    test_set_path = "data/reorganized_test_set"
    model_dir = "data/models"

    try:
        interface = ReorganizedTestInterface(test_set_path, model_dir)


        info = interface.get_test_set_info()
        print(f"测试集信息: {info}")


        results = interface.test_single_batch(batch_number=1, use_surrogate=True)

        print(f"\n批次1测试完成:")
        print(f"成功预测: {results['success_count']}")
        print(f"失败预测: {results['failure_count']}")
        if results["stats"]["mean_error"] is not None:
            print(f"平均误差: {results['stats']['mean_error']:.4f}")
            print(f"成功率: {results['stats']['success_rate']:.2%}")

    except Exception as e:
        print(f"测试失败: {e}")


def example_all_batches_test():

    print("\n" + "=" * 60)
    print("示例：测试所有批次")
    print("=" * 60)

    test_set_path = "data/reorganized_test_set"
    model_dir = "data/models"

    try:
        interface = ReorganizedTestInterface(test_set_path, model_dir)


        results = interface.test_all_batches(use_surrogate=True, max_batches=3)


        interface.print_summary(results)


        output_dir = "test_results"
        interface.save_results(results, output_dir)


        interface.generate_plots(results, output_dir)

    except Exception as e:
        print(f"测试失败: {e}")


def example_direct_predict_test():

    print("\n" + "=" * 60)
    print("示例：使用直接预测")
    print("=" * 60)

    test_set_path = "data/reorganized_test_set"
    model_dir = "data/models"

    try:
        interface = ReorganizedTestInterface(test_set_path, model_dir)


        results = interface.test_single_batch(
            batch_number=1, use_surrogate=False
        )

        print(f"\n直接预测测试完成:")
        print(f"成功预测: {results['success_count']}")
        print(f"失败预测: {results['failure_count']}")
        if results["stats"]["mean_error"] is not None:
            print(f"平均误差: {results['stats']['mean_error']:.4f}")
            print(f"成功率: {results['stats']['success_rate']:.2%}")

    except Exception as e:
        print(f"测试失败: {e}")


def main():

    print("代理模型测试示例")
    print("请确保以下路径存在:")
    print("- data/reorganized_test_set/")
    print("- data/models/")
    print()


    test_set_path = Path("data/reorganized_test_set")
    model_dir = Path("data/models")

    if not test_set_path.exists():
        print(f"错误: 测试集路径不存在: {test_set_path}")
        print("请先运行 reorganize_test_set.py 创建测试集")
        return

    if not model_dir.exists():
        print(f"错误: 模型目录不存在: {model_dir}")
        print("请先运行 pre_train_data1_surrogate.py 训练模型")
        return


    example_single_batch_test()
    example_all_batches_test()
    example_direct_predict_test()

    print("\n" + "=" * 60)
    print("所有示例运行完成")
    print("=" * 60)


if __name__ == "__main__":
    main()
