import pandas as pd

# Estimated speed (km/h) by road type
DEFAULT_SPEEDS = {
    "motorway": 80,
    "trunk": 70,
    "primary": 60,
    "secondary": 40,
    "tertiary": 30,
    "residential": 20,
    "unclassified": 25,
    "service": 15,
}

def estimate_speed(highway):
    return DEFAULT_SPEEDS.get(str(highway).lower(), 30)  # fallback: 30 km/h

def convert_csv_to_gr(edges_csv_path, output_gr_path, directed=True):
    df = pd.read_csv(edges_csv_path)

    # Map road type → speed → travel time in seconds
    df['speed_kmh'] = df['highway_type'].apply(estimate_speed)
    df['travel_time_sec'] = (df['length'] / 1000) / (df['speed_kmh'] / 60 / 60)
    df['travel_time_sec'] = df['travel_time_sec'].round().astype(int).clip(lower=1)

    # Create DIMACS .gr format
    nodes = pd.concat([df['source'], df['target']]).unique()
    node_map = {node_id: idx+1 for idx, node_id in enumerate(sorted(nodes))}
    n = len(node_map)
    m = len(df)

    with open(output_gr_path, 'w') as f:
        f.write(f"p sp {n} {m}\n")
        for _, row in df.iterrows():
            u = node_map[row['source']]
            v = node_map[row['target']]
            w = row['travel_time_sec']
            f.write(f"a {u} {v} {w}\n")
            if not directed:
                f.write(f"a {v} {u} {w}\n")

    print(f"✅ Wrote {m} edges and {n} nodes to {output_gr_path}")

# Example usage:
convert_csv_to_gr(
    edges_csv_path = "../../data/raw/quezon_city_edges.csv",
    output_gr_path = "../../data/processed/qc_from_csv.gr"
)
