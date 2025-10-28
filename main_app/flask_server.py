# flask_server.py - Enhanced with D-HC2L Routing
from flask import Flask, request, jsonify, render_template
import pandas as pd
import time

# Import your coordinate mapper and GPS HC2L router
from coordinate_mapper import NodeMapper
from gps_hc2l_router import GPSRoutingService
from dhl_router import DHLRouter
import request_new_datasets as rq


app = Flask(__name__)

# Initialize components
mapper = NodeMapper('../data/raw/quezon_city_nodes.csv')
try:
    gps_router = GPSRoutingService()
    print("✅ GPS HC2L Router initialized successfully")
except Exception as e:
    print(f"❌ Error initializing GPS HC2L Router: {e}")
    gps_router = None

# Initialize DHL Router
try:
    dhl_router = DHLRouter()
    print("✅ DHL Router initialized successfully")
except Exception as e:
    print(f"❌ Error initializing DHL Router: {e}")
    dhl_router = None

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/test')
def test():
    return render_template('dhc2l_routing_demo.html')

@app.route('/dhl')
def dhl_test():
    return render_template('dhl_routing_demo.html')


@app.route('/validation')
def validation_tool():
    return render_template('map.html')

@app.route('/request_new_dataset')
def request_new_dataset():
    rq.generate_all_datasets()

    return jsonify({
        'success': True,
        'message': 'New dataset added successfully'
    })

@app.route('/report_disruption', methods=['POST'])
def report_disruption():
    data = request.json
    print(f"Received disruption report: {data}")
    # Process disruption report
    return jsonify({
        'success': True,
        'message': 'Disruption reported successfully'
    })

@app.route('/get_all_nodes')
def get_all_nodes():
    """Return all nodes for display on map"""
    try:
        # Load all nodes
        nodes_df = pd.read_csv('../data/raw/quezon_city_nodes.csv')
        
        # Convert to list of dictionaries for JSON
        nodes_list = []
        for _, row in nodes_df.iterrows():
            nodes_list.append({
                'node_id': int(row['node_id']),
                'lat': float(row['latitude']),
                'lng': float(row['longitude'])
            })
        
        return jsonify({
            'success': True,
            'nodes': nodes_list,
            'count': len(nodes_list)
        })
        
    except Exception as e:
        return jsonify({
            'success': False,
            'error': str(e)
        })


@app.route('/get_all_edges')
def get_all_edges():
    """Return all edges for display on map"""
    try:
        # Load edges and nodes
        edges_df = pd.read_csv('../data/raw/quezon_city_edges.csv')
        nodes_df = pd.read_csv('../data/raw/quezon_city_nodes.csv')
        
        # Create node lookup for coordinates
        node_lookup = {}
        for _, row in nodes_df.iterrows():
            node_lookup[int(row['node_id'])] = {
                'lat': float(row['latitude']),
                'lng': float(row['longitude'])
            }
        
        # Process edges
        edges_list = []
        skipped_edges = 0
        
        for _, edge in edges_df.iterrows():
            source_id = int(edge['source'])
            target_id = int(edge['target'])
            
            # Check if both nodes exist in our nodes dataset
            if source_id in node_lookup and target_id in node_lookup:
                edges_list.append({
                    'source_id': source_id,
                    'target_id': target_id,
                    'source_lat': node_lookup[source_id]['lat'],
                    'source_lng': node_lookup[source_id]['lng'],
                    'target_lat': node_lookup[target_id]['lat'],
                    'target_lng': node_lookup[target_id]['lng'],
                    'length': float(edge['length']),
                    'name': str(edge['name']) if pd.notna(edge['name']) else 'Unnamed Road',
                    'highway': str(edge['highway']) if pd.notna(edge['highway']) else 'unclassified'
                })
            else:
                skipped_edges += 1
        
        return jsonify({
            'success': True,
            'edges': edges_list,
            'edges_count': len(edges_list),
            'skipped_edges': skipped_edges,
            'total_edges_in_file': len(edges_df)
        })
        
    except Exception as e:
        return jsonify({
            'success': False,
            'error': str(e)
        })


@app.route('/get_scenario_data')
def get_scenario_data():
    """Return scenario data with coordinates and traffic conditions"""
    try:
        # Load scenario data with coordinates
        scenario_df = pd.read_csv('../data/disruptions/qc_scenario_for_cpp_1.csv')
        
        # Convert to format suitable for visualization
        edges_with_traffic = []
        
        for _, row in scenario_df.iterrows():
            edges_with_traffic.append({
                'source_id': int(row['source']),
                'target_id': int(row['target']),
                'source_lat': float(row['source_lat']),
                'source_lng': float(row['source_lon']),
                'target_lat': float(row['target_lat']),
                'target_lng': float(row['target_lon']),
                'road_name': str(row['road_name']),
                'speed_kph': float(row['speed_kph']),
                'freeFlow_kph': float(row['freeFlow_kph']),
                'jamFactor': float(row['jamFactor']),
                'isClosed': bool(row['isClosed']),
                'segmentLength': float(row['segmentLength'])
            })
        
        return jsonify({
            'success': True,
            'edges': edges_with_traffic,
            'count': len(edges_with_traffic),
            'description': 'Quezon City traffic scenario with real coordinates and traffic conditions'
        })
        
    except Exception as e:
        return jsonify({
            'success': False,
            'error': str(e)
        })


@app.route('/find_nearest_node', methods=['POST'])
def find_nearest_node():
    """Find nearest node to clicked coordinates"""
    data = request.json
    
    try:
        # Find nearest node
        node_id, distance = mapper.find_nearest_node(
            data['lat'], data['lng'], max_distance_m=1000  # Increased range
        )
        
        if not node_id:
            return jsonify({
                'success': False,
                'error': f'No nodes within 1km. Nearest is {distance:.1f}m away.'
            })
        
        # Get node details
        nodes_df = pd.read_csv('../data/raw/quezon_city_nodes.csv')
        node_data = nodes_df[nodes_df['node_id'] == node_id].iloc[0]
        
        return jsonify({
            'success': True,
            'node_id': int(node_id),
            'lat': float(node_data['latitude']),
            'lng': float(node_data['longitude']),
            'distance_m': round(distance, 1),
            'clicked_lat': data['lat'],
            'clicked_lng': data['lng']
        })
        
    except Exception as e:
        return jsonify({
            'success': False,
            'error': str(e)
        })


@app.route('/compute_dhc2l_route', methods=['POST'])
def compute_dhc2l_route():
    """Compute optimal route using GPS HC2L algorithm"""
    data = request.json
    
    try:
        if gps_router is None:
            return jsonify({
                'success': False,
                'error': 'GPS HC2L Router not initialized properly'
            })
        
        # Extract coordinates
        start_lat = float(data['start_lat'])
        start_lng = float(data['start_lng'])
        dest_lat = float(data['dest_lat'])
        dest_lng = float(data['dest_lng'])
        threshold = float(data.get('threshold', 0.0))
        
        # Check if disruptions should be used
        use_disruptions = data.get('use_disruptions', False)
        
        print(f"Computing GPS HC2L route: ({start_lat}, {start_lng}) -> ({dest_lat}, {dest_lng}) [Disruptions: {use_disruptions}]")
        
        # Compute route using GPS HC2L
        start_time = time.time()
        route_result = gps_router.compute_route(start_lat, start_lng, dest_lat, dest_lng, use_disruptions, threshold)
        computation_time = time.time() - start_time
        
        if not route_result['success']:
            return jsonify({
                'success': False,
                'error': route_result['error'],
                'debug_info': route_result.get('raw_output', '')
            })
        
        # Get polylines for Google Maps
        polylines = gps_router.get_route_polylines_for_gmaps(route_result)
        
        # Get route summary
        summary = gps_router.get_route_summary(route_result)
        summary['total_computation_time_sec'] = round(computation_time, 3)
        
        # # Debug: Print labeling metrics
        # print(f"Route Summary Metrics:")
        # print(f"Labeling size: {summary.get('labeling_size_mb', 'N/A')} MB")
        # print(f"Labeling time: {summary.get('labeling_time_ms', 'N/A')} s")

        # Debug: Print the route structure
        # print(f"Route result structure: {list(route_result.get('route', {}).keys())}")
        if 'turn_by_turn_directions' in route_result.get('route', {}):
            directions = route_result['route']['turn_by_turn_directions']
            # print(f"Turn-by-turn directions ({len(directions)} steps): {directions[:3]}...")  # First 3 steps
        
        return jsonify({
            'success': True,
            'route': {
                'polylines': polylines,
                'start_point': {'lat': start_lat, 'lng': start_lng},
                'end_point': {'lat': dest_lat, 'lng': dest_lng},
                'coordinates': route_result.get('route', {}).get('coordinates', []),
                'path_nodes': route_result.get('route', {}).get('path_nodes', []),
                'road_segments': route_result.get('route', {}).get('road_segments', []),
                'route_summary': route_result.get('route', {}).get('route_summary', ''),
                'turn_by_turn_directions': route_result.get('route', {}).get('turn_by_turn_directions', []),
                'display_format': route_result.get('route', {}).get('display_format', {})
            },
            'metrics': summary,
            'algorithm': summary.get('algorithm', 'D-HC2L Dynamic'),  # Use algorithm from summary
            'algorithm_base': summary.get('algorithm_base', 'D-HC2L'),
            'routing_mode': summary.get('routing_mode', 'BASE'),
            'update_strategy': summary.get('update_strategy', 'none'),
            'mode_explanation': summary.get('mode_explanation', ''),
            'labels_status': summary.get('labels_status', 'original'),
            'gps_mapping': route_result.get('gps_mapping', {}),
            'raw_output': route_result.get('raw_output', '')
        })
        
    except Exception as e:
        return jsonify({
            'success': False,
            'error': f"Route computation error: {str(e)}"
        })


@app.route('/compare_routes', methods=['POST'])
def compare_routes():
    """Compare GPS HC2L base route with disrupted route"""
    data = request.json
    
    try:
        start_lat = float(data['start_lat'])
        start_lng = float(data['start_lng'])
        dest_lat = float(data['dest_lat'])
        dest_lng = float(data['dest_lng'])
        threshold = float(data.get('threshold', 0.5))
        
        # Use the GPS router's built-in comparison function
        comparison_result = gps_router.compare_routes(start_lat, start_lng, dest_lat, dest_lng, threshold)

        if not comparison_result['success']:
            return jsonify({
                'success': False,
                'error': comparison_result['error']
            })
        
        # Add straight line for additional comparison
        straight_line = [{
            'path': [
                {'lat': start_lat, 'lng': start_lng},
                {'lat': dest_lat, 'lng': dest_lng}
            ],
            'strokeColor': '#00FF00',  # Green
            'strokeOpacity': 0.5,
            'strokeWeight': 2,
            'geodesic': True
        }]
        
        comparison_result['routes']['straight_line'] = {
            'polylines': straight_line,
            'name': 'Straight Line',
            'color': '#00FF00'
        }
        
        comparison_result['start_point'] = {'lat': start_lat, 'lng': start_lng}
        comparison_result['end_point'] = {'lat': dest_lat, 'lng': dest_lng}
        
        return jsonify(comparison_result)
        
    except Exception as e:
        return jsonify({
            'success': False,
            'error': f"Route comparison error: {str(e)}"
        })


@app.route('/get_active_disruptions')
def get_active_disruptions():
    """Get all active disruptions from the scenario data with classification"""
    try:
        # Load scenario data
        scenario_df = pd.read_csv('../data/disruptions/qc_scenario_for_cpp_1.csv')
        
        # Constants for disruption classification (matching Dynamic.cpp)
        jam_tendency = 1
        hour_of_day = 12
        location_tag = "road"
        duration_min = 30
        
        disruptions_by_type = {}
        total_disruptions = 0
        
        for _, row in scenario_df.iterrows():
            source_id = int(row['source'])
            target_id = int(row['target'])
            road_name = str(row['road_name'])
            speed_kph = float(row['speed_kph'])
            free_flow_kph = float(row['freeFlow_kph'])
            jam_factor = float(row['jamFactor'])
            is_closed = bool(row['isClosed'] == 'True' or row['isClosed'] == True)
            segment_length = float(row['segmentLength'])
            
            # Calculate slowdown ratio
            slowdown_ratio = speed_kph / (free_flow_kph if free_flow_kph > 0 else 1.0)
            slowdown_ratio = max(0.0, min(slowdown_ratio, 1e9))  # Clamp slowdown
            
            # Skip if no disruption (normal conditions)
            if not is_closed and slowdown_ratio >= 1.0:
                continue
            
            # Apply disruption classification logic from Dynamic.cpp
            incident = "Other"
            if is_closed or jam_factor == 10:
                incident = "Road Closure"
            elif speed_kph < 2 and jam_factor > 7 and not is_closed:
                incident = "Accident"
            elif slowdown_ratio <= 0.5 and duration_min >= 30 and jam_factor < 7:
                incident = "Construction"
            elif jam_factor > 7 and speed_kph < 5:
                incident = "Congestion"
            elif speed_kph <= 1 and jam_factor < 4 and segment_length < 100:
                incident = "Disabled Vehicle"
            elif location_tag == "terminal" and hour_of_day >= 6 and hour_of_day <= 9:
                incident = "Mass Transit Event"
            elif location_tag == "event_venue" and (hour_of_day >= 18 or hour_of_day <= 23):
                incident = "Planned Event"
            elif slowdown_ratio < 0.4 and jam_tendency == 1 and not is_closed:
                incident = "Road Hazard"
            elif speed_kph >= 10 and speed_kph <= 15 and jam_tendency == 1 and not is_closed:
                incident = "Lane Restriction"
            elif speed_kph < 10 and duration_min > 20:
                incident = "Weather"
            
            # Determine severity
            severity = "Light"
            if slowdown_ratio < 0.5:
                severity = "Heavy"
            elif slowdown_ratio < 0.8:
                severity = "Medium"
            
            # Create disruption entry
            disruption = {
                'source_id': source_id,
                'target_id': target_id,
                'source_lat': float(row['source_lat']),
                'source_lng': float(row['source_lon']),
                'target_lat': float(row['target_lat']),
                'target_lng': float(row['target_lon']),
                'road_name': road_name,
                'incident_type': incident,
                'severity': severity,
                'speed_kph': speed_kph,
                'free_flow_kph': free_flow_kph,
                'jam_factor': jam_factor,
                'is_closed': is_closed,
                'slowdown_ratio': round(slowdown_ratio, 3),
                'segment_length': segment_length
            }
            
            # Group by incident type
            if incident not in disruptions_by_type:
                disruptions_by_type[incident] = []
            disruptions_by_type[incident].append(disruption)
            total_disruptions += 1
        
        # Calculate statistics
        type_counts = {incident_type: len(disruptions) for incident_type, disruptions in disruptions_by_type.items()}
        severity_counts = {'Heavy': 0, 'Medium': 0, 'Light': 0}
        
        for disruptions in disruptions_by_type.values():
            for disruption in disruptions:
                severity_counts[disruption['severity']] += 1
        
        return jsonify({
            'success': True,
            'total_disruptions': total_disruptions,
            'disruptions_by_type': disruptions_by_type,
            'type_counts': type_counts,
            'severity_counts': severity_counts,
            'timestamp': time.time()
        })
        
    except Exception as e:
        return jsonify({
            'success': False,
            'error': f"Error loading disruptions: {str(e)}"
        })


@app.route('/compute_dhl_route', methods=['POST'])
def compute_dhl_route():
    """Compute optimal route using DHL algorithm"""
    data = request.json
    
    try:
        if dhl_router is None:
            return jsonify({
                'success': False,
                'error': 'DHL Router not initialized properly'
            })
        
        # Extract coordinates
        start_lat = float(data['start_lat'])
        start_lng = float(data['start_lng'])
        dest_lat = float(data['dest_lat'])
        dest_lng = float(data['dest_lng'])
        
        # Check if disruptions should be used
        use_disruptions = data.get('use_disruptions', False)
        
        print(f"Computing DHL route: ({start_lat}, {start_lng}) -> ({dest_lat}, {dest_lng}) [Disruptions: {use_disruptions}]")
        
        # Compute route using DHL
        start_time = time.time()
        route_result = dhl_router.compute_route(start_lat, start_lng, dest_lat, dest_lng, use_disruptions)
        computation_time = time.time() - start_time
        
        if not route_result['success']:
            return jsonify({
                'success': False,
                'error': route_result['error'],
                'debug_info': route_result.get('raw_output', '')
            })
        
        # Get polylines for Google Maps
        polylines = dhl_router.get_route_polylines_for_gmaps(route_result)
        
        # Get route summary
        summary = dhl_router.get_route_summary(route_result)
        summary['total_computation_time_sec'] = round(computation_time, 3)
        
        # Get enhanced road name information using the new methods
        turn_by_turn_directions = dhl_router.get_turn_by_turn_directions(route_result)
        route_summary_text = dhl_router.get_route_summary_text(route_result)
        detailed_route_info = dhl_router.get_detailed_route_info(route_result)
        # print(f"🛣️  DHL Route Summary: {route_summary_text}")
        return jsonify({
            'success': True,
            'route': {
                'polylines': polylines,
                'start_point': {'lat': start_lat, 'lng': start_lng},
                'end_point': {'lat': dest_lat, 'lng': dest_lng},
                'coordinates': route_result.get('route', {}).get('coordinates', []),
                'path_nodes': route_result.get('route', {}).get('path_nodes', []),
                'road_segments': route_result.get('route', {}).get('road_segments', []),
                'route_summary': route_summary_text, 
                'route_summary_detailed': route_result.get('route', {}).get('route_summary', ''),  # Original trace
                'turn_by_turn_directions': turn_by_turn_directions,
                'display_format': route_result.get('route', {}).get('display_format', {}),
                'detailed_info': detailed_route_info
            },
            'metrics': summary,
            'algorithm': 'DHL (Dual-Hierarchy Labelling)',
            'gps_mapping': route_result.get('gps_mapping', {}),
            'disruptions': route_result.get('disruptions', {}),
            'raw_dhl_output': route_result.get('raw_dhl_output', {})
        })
        
    except Exception as e:
        return jsonify({
            'success': False,
            'error': f"DHL route computation error: {str(e)}"
        })


@app.route('/compare_dhl_routes', methods=['POST'])
def compare_dhl_routes():
    """Compare DHL base route with disrupted route"""
    data = request.json
    
    try:
        if dhl_router is None:
            return jsonify({
                'success': False,
                'error': 'DHL Router not initialized properly'
            })
        
        start_lat = float(data['start_lat'])
        start_lng = float(data['start_lng'])
        dest_lat = float(data['dest_lat'])
        dest_lng = float(data['dest_lng'])
        
        # Use the DHL router's built-in comparison function
        comparison_result = dhl_router.compare_routes(start_lat, start_lng, dest_lat, dest_lng)
        
        if not comparison_result['success']:
            return jsonify({
                'success': False,
                'error': comparison_result['error']
            })
        
        # Enhance the comparison result with detailed road name information
        if 'routes' in comparison_result:
            for route_type in ['base', 'disrupted']:
                if route_type in comparison_result['routes']:
                    route_data = comparison_result['routes'][route_type]
                    
                    # Create a mock route result to use with our new methods
                    mock_route_result = {
                        'success': True,
                        'route': {
                            'path_nodes': route_data.get('summary', {}).get('path_nodes', []),
                            'road_segments': route_data.get('road_segments', []),
                            'turn_by_turn_directions': route_data.get('turn_by_turn_directions', []),
                            'route_summary': route_data.get('route_summary', ''),
                            'display_format': route_data.get('display_format', {})
                        }
                    }
                    
                    # Add enhanced information if path nodes are available
                    if route_data.get('summary', {}).get('path_nodes'):
                        enhanced_summary = dhl_router.get_route_summary_text(mock_route_result)
                        enhanced_directions = dhl_router.get_turn_by_turn_directions(mock_route_result)
                        enhanced_details = dhl_router.get_detailed_route_info(mock_route_result)
                        
                        # Add enhanced data to the route
                        route_data['enhanced_summary'] = enhanced_summary
                        route_data['enhanced_directions'] = enhanced_directions
                        route_data['enhanced_details'] = enhanced_details
                        
                        print(f"🛣️  Enhanced {route_type} route: {enhanced_summary}")
        
        # Add straight line for additional comparison
        straight_line = [{
            'path': [
                {'lat': start_lat, 'lng': start_lng},
                {'lat': dest_lat, 'lng': dest_lng}
            ],
            'strokeColor': '#00FF00',  # Green
            'strokeOpacity': 0.5,
            'strokeWeight': 2,
            'geodesic': True
        }]
        
        comparison_result['routes']['straight_line'] = {
            'polylines': straight_line,
            'name': 'Straight Line',
            'color': '#00FF00'
        }
        
        comparison_result['start_point'] = {'lat': start_lat, 'lng': start_lng}
        comparison_result['end_point'] = {'lat': dest_lat, 'lng': dest_lng}
        
        return jsonify(comparison_result)
        
    except Exception as e:
        return jsonify({
            'success': False,
            'error': f"DHL route comparison error: {str(e)}"
        })


@app.route('/compare_algorithms', methods=['POST'])
def compare_algorithms():
    """Compare D-HC2L and DHL algorithms side by side"""
    data = request.json
    
    try:
        start_lat = float(data['start_lat'])
        start_lng = float(data['start_lng'])
        dest_lat = float(data['dest_lat'])
        dest_lng = float(data['dest_lng'])
        use_disruptions = data.get('use_disruptions', False),
        threshold = float(data.get('threshold', 0.5))
        
        results = {'success': True, 'routes': {}, 'comparison_metrics': {}}
        
        # Compute D-HC2L route if available
        if gps_router is not None:
            print("Computing D-HC2L route...")
            dhc2l_start = time.time()
            dhc2l_result = gps_router.compute_route(start_lat, start_lng, dest_lat, dest_lng, use_disruptions, threshold)
            dhc2l_time = time.time() - dhc2l_start
            
            if dhc2l_result['success']:
                dhc2l_summary = gps_router.get_route_summary(dhc2l_result)
                results['routes']['dhc2l'] = {
                    'polylines': gps_router.get_route_polylines_for_gmaps(dhc2l_result),
                    'summary': dhc2l_summary,
                    'name': dhc2l_summary.get('algorithm', 'D-HC2L Dynamic'),
                    'routing_mode': dhc2l_summary.get('routing_mode', 'BASE'),
                    'update_strategy': dhc2l_summary.get('update_strategy', 'none'),
                    'mode_explanation': dhc2l_summary.get('mode_explanation', ''),
                    'labels_status': dhc2l_summary.get('labels_status', 'original'),
                    'color': '#FF0000',
                    'computation_time': dhc2l_time
                }
        
        # Compute DHL route if available
        if dhl_router is not None:
            print("Computing DHL route...")
            dhl_start = time.time()
            dhl_result = dhl_router.compute_route(start_lat, start_lng, dest_lat, dest_lng, use_disruptions)
            dhl_time = time.time() - dhl_start
            
            if dhl_result['success']:
                # Get enhanced route information for DHL
                dhl_summary = dhl_router.get_route_summary(dhl_result)
                dhl_route_summary = dhl_router.get_route_summary_text(dhl_result)
                dhl_directions = dhl_router.get_turn_by_turn_directions(dhl_result)
                dhl_details = dhl_router.get_detailed_route_info(dhl_result)
                
                results['routes']['dhl'] = {
                    'polylines': dhl_router.get_route_polylines_for_gmaps(dhl_result),
                    'summary': dhl_summary,
                    'route_summary_text': dhl_route_summary,  # Enhanced with road names
                    'turn_by_turn_directions': dhl_directions,
                    'detailed_info': dhl_details,
                    'name': 'DHL (Dual-Hierarchy Labelling)',
                    'color': '#0066FF',
                    'computation_time': dhl_time
                }
                
                print(f"🛣️  DHL route summary: {dhl_route_summary}")
                print(f"📍 DHL directions: {len(dhl_directions)} steps")
        
        # Add straight line reference
        results['routes']['straight_line'] = {
            'polylines': [{
                'path': [
                    {'lat': start_lat, 'lng': start_lng},
                    {'lat': dest_lat, 'lng': dest_lng}
                ],
                'strokeColor': '#00FF00',
                'strokeOpacity': 0.5,
                'strokeWeight': 2,
                'geodesic': True
            }],
            'name': 'Straight Line',
            'color': '#00FF00'
        }
        
        # Add comparison metrics
        if 'dhc2l' in results['routes'] and 'dhl' in results['routes']:
            dhc2l_summary = results['routes']['dhc2l']['summary']
            dhl_summary = results['routes']['dhl']['summary']
            
            results['comparison_metrics'] = {
                'query_time_difference_ms': (
                    dhl_summary.get('query_time_ms', 0) - 
                    dhc2l_summary.get('query_time_ms', 0)
                ),
                'total_computation_difference_sec': (
                    results['routes']['dhl']['computation_time'] - 
                    results['routes']['dhc2l']['computation_time']
                ),
                'distance_comparison': {
                    'dhc2l_distance': dhc2l_summary.get('total_distance_m', 0),
                    'dhl_distance': dhl_summary.get('total_distance_units', 0)
                },
                'path_length_comparison': {
                    'dhc2l_nodes': dhc2l_summary.get('number_of_nodes', 0),
                    'dhl_nodes': dhl_summary.get('path_length', 0)
                }
            }
        
        results['start_point'] = {'lat': start_lat, 'lng': start_lng}
        results['end_point'] = {'lat': dest_lat, 'lng': dest_lng}
        results['parameters'] = {'use_disruptions': use_disruptions}

        return jsonify(results)
        
    except Exception as e:
        return jsonify({
            'success': False,
            'error': f"Algorithm comparison error: {str(e)}"
        })



if __name__ == '__main__':
    app.run(debug=True, host='0.0.0.0', port=5000)