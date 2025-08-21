#!/bin/bash

#mcq 7.13

hpl_dir=/home/mcq/workspace/hpl-ai-finalist/hpl-ai-finalist
llvmlink_dir="/home/mcq/workspace/llvm-install/bin/llvm-link"
# 设置默认输出目录
default_output_dir="/home/mcq/workspace/res"

# 如果第一个参数存在，就使用它，否则用默认值
output_dir="/home/mcq/workspace/res"

# 模式：ll / ll+s / all
mode="ll"


# 解析参数
while getopts ":o:seh" opt; do
  case $opt in
    o) output_dir="$OPTARG" ;;
    s) mode="s" ;;
    e) mode="e" ;;
    h)
      echo "用法: $0 [-o 输出目录] [-s] [-e] [-h]"
      echo "      -o DIR    指定输出目录（默认: /home/mcq/workspace/res）"
      echo "      不输入-s或-e, 则只生成.ll文件        "
      echo "      -s        生成 .ll 和 .s 文件（不生成可执行文件）"
      echo "      -e        生成 .ll、.s 和可执行文件"
      echo "      -h        显示此帮助信息"
      exit 0
      ;;
    \?) echo "无效选项: -$OPTARG" >&2; exit 1 ;;
    :) echo "选项 -$OPTARG 需要一个参数" >&2; exit 1 ;;
  esac
done

# 创建输出目录（如果不存在，不会清空已有内容）
mkdir -p "$output_dir"

# .c 文件列表
file_array=("blas" "convert" "dgetrf_nopiv" "gmres" "hpl-ai" "matgen" "print" "timer")

# 编译参数
compile_flags="--target=aarch64-linux-gnu -march=armv8.2-a+fp16 -static   "

# 1. 生成 .ll 文件
for file in "${file_array[@]}"; do
    clang $compile_flags -emit-llvm -S  \
        "$hpl_dir/$file.c" -o "$output_dir/$file.ll"
done

# 2. 链接所有 .ll 为一个 hpllink.ll
llfiles=""
for file in "${file_array[@]}"; do
    llfiles="$llfiles $output_dir/$file.ll"
done

$llvmlink_dir -S $llfiles -o "$output_dir/hpllink.ll"

# 3. 根据模式判断是否继续
if [[ "$mode" == "ll" ]]; then
    echo "✅ 成功生成: .ll 文件，路径：$output_dir, 输出文件: hpllink.ll"
    exit 0
fi

# 4. 生成汇编 .s 文件
llc "$output_dir/hpllink.ll" -o "$output_dir/hpllink.s"

if [[ "$mode" == "s" ]]; then
    echo "✅ 成功生成：.ll 和 .s 文件，路径：$output_dir, 输出文件: hpllink.ll hpllink.s"
    exit 0
fi

# 5. 生成最终可执行文件
clang $compile_flags "$output_dir/hpllink.s" -o "$output_dir/hpl_exec" -lm

echo "✅ 成功生成：.ll、.s 和可执行文件，输出目录：$output_dir, 输出文件: hpllink.ll hpllink.s hpl_exec"
