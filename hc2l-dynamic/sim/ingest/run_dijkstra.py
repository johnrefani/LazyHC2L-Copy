import heapq
import csv

GR_PATH = r"C:\Users\Erica M\OneDrive\Documents\GitHub\LazyHC2L\hc2l-dynamic\data\processed\qc_from_csv.gr"
OD_PATH = r"C:\Users\Erica M\OneDrive\Documents\GitHub\LazyHC2L\hc2l-dynamic\experiments\configs\qc_od_pairs.csv"
OUT_PATH = r"C:\Users\Erica M\OneDrive\Documents\GitHub\LazyHC2L\hc2l-dynamic\experiments\results\dijkstra_results.csv"



def load_graph(gr_path):
    graph = {}
    with open(gr_path, "r") as f:
        for line in f:
            if line.startswith("p"):
                _, _, n_nodes, _ = line.strip().split()
                n_nodes = int(n_nodes)
                for i in range(1, n_nodes + 1):
                    graph[i] = []
            elif line.startswith("a"):
                _, u, v, d = line.strip().split()
                u, v, d = int(u), int(v), int(d)
                graph[u].append((v, d))
    return graph


def dijkstra(graph, start, target):
    dist = {node: float("inf") for node in graph}
    dist[start] = 0
    visited = set()
    heap = [(0, start)]

    while heap:
        current_dist, u = heapq.heappop(heap)
        if u in visited:
            continue
        visited.add(u)

        if u == target:
            return current_dist

        for v, weight in graph[u]:
            if dist[v] > current_dist + weight:
                dist[v] = current_dist + weight
                heapq.heappush(heap, (dist[v], v))

    return None  # disconnected


def load_od_pairs(od_path):
    pairs = []
    with open(od_path, "r") as f:
        reader = csv.DictReader(f)
        for row in reader:
            source = int(row["source"])
            target = int(row["target"])
            pairs.append((source, target))
    return pairs


def main():
    print("[INFO] Loading graph...")
    graph = load_graph(GR_PATH)

    print("[INFO] Loading OD pairs...")
    od_pairs = load_od_pairs(OD_PATH)

    print("[INFO] Running Dijkstra on each pair...")
    results = []

    for i, (s, t) in enumerate(od_pairs):
        print(f"[QUERY] {s} → {t}")
        dist = dijkstra(graph, s, t)
        if dist is None:
            results.append((s, t, "", True))  # disconnected
        else:
            results.append((s, t, dist, False))

    print("[INFO] Writing results...")
    with open(OUT_PATH, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["source", "target", "distance_ms", "disconnected"])
        for row in results:
            writer.writerow(row)

    print(f"[DONE] Results written to {OUT_PATH}")


if __name__ == "__main__":
    main()
