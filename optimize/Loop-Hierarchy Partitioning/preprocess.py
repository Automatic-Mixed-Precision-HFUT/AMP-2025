import json


with open("vars_depth.json", "r") as f:
    funcs = json.load(f)


with open("local_var.json", "r") as f:
    local_vars = json.load(f)["localVar"]


type_index = set((var["function"], var["name"]) for var in local_vars)


out_vars = []
for func in funcs:
    fname = func["function"]
    for var in func.get("variables", []):
        key = (fname, var["name"])
        if key in type_index:
            out_vars.append({
                "function": fname,
                "name": var["name"],
                "max_loop_depth": var["max_loop_depth"]
            })


with open("out.json", "w") as f:
    json.dump({"localVar": out_vars}, f, indent=4)

print(f"writen {len(out_vars)} variables out.json")
