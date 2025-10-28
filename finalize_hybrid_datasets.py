# hc2l-dynamic/sim/ingest/finalize_hybrid_datasets.py
# source and target lat and lon.

import pandas as pd
import random
import os
import requests
import osmnx as ox
from math import radians, sin, cos, sqrt, atan2

# --- CONFIGURATION ---
USE_HERE_API_DATA = True
NUMBER_OF_SCENARIOS = 5
SYNTHETIC_JAM_PERCENTAGE_RANGE = (0.10, 0.20)
SYNTHETIC_CLOSURE_PERCENTAGE_RANGE = (0.005, 0.02)
HERE_API_KEY = "ATeqWF4s0DPcT199IkP3-tgiwK7W-U6vBjHeRI9dmKg" 

# --- ROBUST PATHS ---
# script_dir = os.path.dirname(os.path.abspath(__file__))
BASE_GRAPH_CSV = './data/raw/quezon_city_edges.csv'
OUTPUT_DIR = './data/processed/'

# --- SIMULATION & HELPER FUNCTIONS ---
DISRUPTION_LEVELS = {
    'Light': {'jam_factor_range': (2.0, 4.0), 'speed_reduction_factor_range': (0.7, 0.9)},
    'Medium': {'jam_factor_range': (4.0, 7.0), 'speed_reduction_factor_range': (0.4, 0.7)},
    'Heavy': {'jam_factor_range': (7.0, 10.0), 'speed_reduction_factor_range': (0.1, 0.4)}
}
DEFAULT_SPEED_KPH = 50
MIN_SPEED_KPH = 1
ox.settings.requests_timeout = 600

def kph_to_mps(kph):
    return kph * 1000 / 3600

def get_traffic_from_here_api():
    """Makes the API call with the final, correct bounding box format."""
    bounding_box = "121.01,14.59,121.14,14.76" # West,South,East,North
    url = f"https://data.traffic.hereapi.com/v7/flow?in=bbox:{bounding_box}&locationReferencing=shape&apiKey={HERE_API_KEY}"
    
    print("Contacting HERE API for real-time traffic data...")
    # --- THIS BLOCK IS NOW CORRECTED AND EXPANDED ---
    try:
        response = requests.get(url, timeout=60)
        response.raise_for_status()
        data = response.json()
        print(f"✅ Successfully received data for {len(data.get('results', []))} road segments from HERE API.")
        return data.get('results', [])
    except requests.exceptions.RequestException as e:
        print(f"❌ ERROR: Failed to get data from HERE API. Details: {e}")
        return None
    # --- END OF CORRECTION ---

def haversine_distance(lat1, lon1, lat2, lon2):
    """Calculate distance between two lat/lon points in meters."""
    R = 6371000
    lat1_rad, lon1_rad, lat2_rad, lon2_rad = map(radians, [lat1, lon1, lat2, lon2])
    dlon = lon2_rad - lon1_rad
    dlat = lat2_rad - lat1_rad
    a = sin(dlat / 2)**2 + cos(lat1_rad) * cos(lat2_rad) * sin(dlon / 2)**2
    c = 2 * atan2(sqrt(a), sqrt(1 - a))
    return R * c

def match_traffic_to_graph(here_results, G_nx, node_coords, base_graph_df):
    """Matches HERE traffic data to the OSMnx graph using geographic proximity."""
    matched_data = []
    print("Matching HERE API results to your graph edges...")
    for item in here_results:
        try:
            shape = item['location']['shape']['links'][0]['points']
            start_lat, start_lon = shape[0]['lat'], shape[0]['lng']
            
            closest_node_id, (lat, lon) = min(node_coords.items(), key=lambda node_item: haversine_distance(start_lat, start_lon, node_item[1][0], node_item[1][1]))
            min_dist = haversine_distance(start_lat, start_lon, lat, lon)

            if min_dist < 50:
                matched_edges = base_graph_df[base_graph_df['source'] == closest_node_id]
                if not matched_edges.empty:
                    edge = matched_edges.iloc[0]
                    matched_data.append({
                        'source': edge['source'], 'target': edge['target'],
                        'jamFactor': item['currentFlow']['jamFactor'], 'speed_kph': item['currentFlow']['speed']
                    })
        except (KeyError, IndexError):
            continue
    print(f"✅ Matched {len(matched_data)} reports.")
    return pd.DataFrame(matched_data)

def fetch_and_process_here_data(G_nx, node_coords, base_df):
    """Complete, real implementation that calls the API and matches results."""
    here_results = get_traffic_from_here_api()
    if not here_results:
        return pd.DataFrame()
    return match_traffic_to_graph(here_results, G_nx, node_coords, base_df)

# --- SCRIPT EXECUTION ---
print("--- Starting Master Dataset Generation ---")
os.makedirs(OUTPUT_DIR, exist_ok=True)
try:
    base_df = pd.read_csv(BASE_GRAPH_CSV)
    print(f"✅ Loaded base graph with {len(base_df)} edges.")
except FileNotFoundError:
    print(f"❌ ERROR: Base graph file not found at '{BASE_GRAPH_CSV}'.")
    exit()

if 'name' not in base_df.columns:
    base_df['name'] = 'Unnamed Road'
else:
    base_df['name'].fillna('Unnamed Road', inplace=True)

print("Fetching graph and node coordinates from OSMnx (this may take several minutes)...")
G_nx = ox.graph_from_place("Quezon City, Philippines", network_type='drive')
node_coords = {node: {'lat': data['y'], 'lon': data['x']} for node, data in G_nx.nodes(data=True)}
node_coords_df = pd.DataFrame.from_dict(node_coords, orient='index').reset_index().rename(columns={'index': 'node_id'})
base_df = base_df.merge(node_coords_df, left_on='source', right_on='node_id', how='left').rename(columns={'lat': 'source_lat', 'lon': 'source_lon'}).drop(columns=['node_id'])
base_df = base_df.merge(node_coords_df, left_on='target', right_on='node_id', how='left').rename(columns={'lat': 'target_lat', 'lon': 'target_lon'}).drop(columns=['node_id'])
print("✅ Added source/target coordinates to the base dataset.")

all_nodes_osm = sorted(list(pd.unique(base_df[['source', 'target']].values.ravel())))
mapping_df = pd.DataFrame({'osm_id': all_nodes_osm, 'gr_id': range(1, len(all_nodes_osm) + 1)})
OUTPUT_MAPPING_FILE = os.path.join(OUTPUT_DIR, 'node_mapping.csv')
mapping_df.to_csv(OUTPUT_MAPPING_FILE, index=False)
print(f"✅ Created node mapping file.")

if USE_HERE_API_DATA:
    here_traffic_df = fetch_and_process_here_data(G_nx, node_coords, base_df)
    if not here_traffic_df.empty:
        base_df = base_df.merge(here_traffic_df, on=['source', 'target'], how='left', suffixes=('', '_real'))
        print(f"✅ Merged real traffic data.")
    else:
        print("⚠️ No real traffic data matched.")

for i in range(1, NUMBER_OF_SCENARIOS + 1):
    print(f"\n--- Generating Scenario {i}/{NUMBER_OF_SCENARIOS} ---")
    df = base_df.copy()
    df['freeFlow_kph'] = DEFAULT_SPEED_KPH
    df['speed_kph'] = df['freeFlow_kph']
    df['jamFactor'] = 1.0
    df['isClosed'] = False
    
    real_data_mask = pd.Series(False, index=df.index)
    if USE_HERE_API_DATA and 'jamFactor_real' in df.columns:
        real_data_mask = df['jamFactor_real'].notna()
        if real_data_mask.any():
            df.loc[real_data_mask, 'jamFactor'] = df['jamFactor_real']
            df.loc[real_data_mask, 'speed_kph'] = df['speed_kph_real']

    eligible_indices = df[~real_data_mask].index.tolist()
    closure_percentage = random.uniform(*SYNTHETIC_CLOSURE_PERCENTAGE_RANGE)
    num_to_close = int(len(eligible_indices) * closure_percentage)
    closure_indices = random.sample(eligible_indices, num_to_close)
    df.loc[closure_indices, 'isClosed'] = True
    df.loc[closure_indices, 'jamFactor'] = 10.0
    df.loc[closure_indices, 'speed_kph'] = MIN_SPEED_KPH
    print(f"Applied {len(closure_indices)} synthetic road closures.")
    
    remaining_indices = [idx for idx in eligible_indices if idx not in closure_indices]
    jam_percentage = random.uniform(*SYNTHETIC_JAM_PERCENTAGE_RANGE)
    num_to_disrupt = int(len(remaining_indices) * jam_percentage)
    disruption_indices = random.sample(remaining_indices, num_to_disrupt)
    for idx in disruption_indices:
        params = DISRUPTION_LEVELS[random.choice(list(DISRUPTION_LEVELS.keys()))]
        df.loc[idx, 'jamFactor'] = random.uniform(*params['jam_factor_range'])
        df.loc[idx, 'speed_kph'] *= random.uniform(*params['speed_reduction_factor_range'])
    print(f"Applied {len(disruption_indices)} synthetic traffic disruptions.")

    output_df = df.rename(columns={'length': 'segmentLength', 'name': 'road_name'})
    
    # *** APPLY NODE MAPPING TO CONVERT OSM IDs TO RENUMBERED IDs (1-N) ***
    output_df = output_df.merge(mapping_df, left_on='source', right_on='osm_id', how='left')
    output_df = output_df.drop(columns=['source', 'osm_id']).rename(columns={'gr_id': 'source'})
    
    output_df = output_df.merge(mapping_df, left_on='target', right_on='osm_id', how='left')
    output_df = output_df.drop(columns=['target', 'osm_id']).rename(columns={'gr_id': 'target'})
    
    print(f"✅ Applied node mapping: OSM IDs → Renumbered IDs (1-{len(mapping_df)})")
    
    final_columns = [
        'source_lat', 'source_lon',
        'target_lat', 'target_lon',
        'source', 'target', 
        'road_name', 'speed_kph', 'freeFlow_kph', 
        'jamFactor', 'isClosed', 'segmentLength'
    ]
    
    output_df = output_df[final_columns]
    output_csv_file = os.path.join(OUTPUT_DIR, f'qc_scenario_for_cpp_{i}.csv')
    output_df.to_csv(output_csv_file, index=False, float_format='%.6f')
    
    print(f"✅ Saved rich data scenario with new column order to '{os.path.basename(output_csv_file)}'")

print("\n--- All datasets generated successfully! ---")