import pandas as pd

# === Config: paths to your results ===
HC2L_PATH = r"C:\Users\Erica M\OneDrive\Documents\GitHub\LazyHC2L\hc2l-dynamic\experiments\results\results.csv"
DIJKSTRA_PATH = r"C:\Users\Erica M\OneDrive\Documents\GitHub\LazyHC2L\hc2l-dynamic\experiments\results\dijkstra_results.csv"
OUTPUT_PATH = r"C:\Users\Erica M\OneDrive\Documents\GitHub\LazyHC2L\hc2l-dynamic\experiments\results\comparison_report.csv"
THRESHOLD_METERS = 1.0

# === Load CSV files ===
print("[INFO] Loading HC2L results...")
hc2l_df = pd.read_csv(HC2L_PATH)
hc2l_df = hc2l_df.rename(columns={"distance_meters": "distance"})

print("[INFO] Loading Dijkstra results...")
dijkstra_df = pd.read_csv(DIJKSTRA_PATH)
dijkstra_df = dijkstra_df.rename(columns={"distance_ms": "distance"})

# === Merge on source + target ===
merged = pd.merge(
    hc2l_df,
    dijkstra_df,
    on=["source", "target"],
    suffixes=("_hc2l", "_dijkstra")
)

# === Compare distances ===
merged["abs_error_m"] = (merged["distance_hc2l"] - merged["distance_dijkstra"]).abs()
merged["match"] = merged["abs_error_m"] <= THRESHOLD_METERS

# === Count disconnected pairs ===
merged["disconnected"] = merged["disconnected_hc2l"] | merged["disconnected_dijkstra"]

# === Flag mismatches ===
mismatches = merged[~merged["match"] & ~merged["disconnected"]]

# === Save comparison report ===
merged.to_csv(OUTPUT_PATH, index=False)
print(f"[DONE] Comparison report saved to: {OUTPUT_PATH}")

# === Summary Stats ===
total = len(merged)
disconnected_count = merged["disconnected"].sum()
mismatch_count = len(mismatches)
match_count = total - mismatch_count - disconnected_count

print("\n[SUMMARY]")
print(f"Total OD pairs:         {total}")
print(f"Disconnected pairs:     {disconnected_count}")
print(f"Matches within {THRESHOLD_METERS}m: {match_count}")
print(f"Mismatches > {THRESHOLD_METERS}m:   {mismatch_count}")
print(f"Match rate:             {match_count / (total - disconnected_count) * 100:.2f}%")
