import random, os

gr_path = "data/processed/qc_from_csv.gr"
od_path = "experiments/configs/qc_od_pairs.csv"
num_pairs = 100

def load_nodes_from_gr(path):
    with open(path, "r") as f:
        nodes = set()
        for line in f:
            if line.startswith("a"):
                parts = line.strip().split()
                nodes.add(int(parts[1]))
                nodes.add(int(parts[2]))
    return list(nodes)

def generate_od_pairs(nodes, n):
    return [random.sample(nodes, 2) for _ in range(n)]

nodes = load_nodes_from_gr(gr_path)
pairs = generate_od_pairs(nodes, num_pairs)

os.makedirs(os.path.dirname(od_path), exist_ok=True)
with open(od_path, "w") as f:
    for s, t in pairs:
        f.write(f"{s},{t}\n")

print(f"[✅] Generated {num_pairs} OD pairs at {od_path}")
