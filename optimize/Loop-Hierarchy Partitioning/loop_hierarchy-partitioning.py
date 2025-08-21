import json
import os

with open("out.json", "r") as f:
    data = json.load(f)["localVar"]


func_groups = {}
for var in data:
    fname = var["function"]
    func_groups.setdefault(fname, []).append(var)


os.makedirs("grouped", exist_ok=True)
with open("grouped/group_by_function.json", "w") as f:
    json.dump(func_groups, f, indent=4)
print("function JSON: grouped/group_by_function.json")


max_depth = max(var["max_loop_depth"] for var in data)
for depth in range(1, max_depth + 1):
    depth_group = {}
    for fname, vars_list in func_groups.items():
        filtered = [v for v in vars_list if v["max_loop_depth"] >= depth]
        if filtered:
            depth_group[fname] = filtered
    if depth_group:
        out_file = f"grouped/group_depth_ge_{depth}.json"
        with open(out_file, "w") as f:
            json.dump(depth_group, f, indent=4)
        print(f"max loop depth >= {depth}  JSON: {out_file}")
