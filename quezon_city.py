import osmnx as ox
import networkx as nx

# Extract driving network for Quezon City, Philippines
G = ox.graph_from_place("Quezon City, Philippines", network_type="drive")

# Verification
print(f"Network extracted successfully!")
print(f"Nodes: {len(G.nodes())}")
print(f"Edges: {len(G.edges())}")