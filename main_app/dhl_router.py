# dhl_router.py - DHL Routing Integration
import subprocess
import json
import os
from typing import Dict, List, Tuple, Optional
from road_name_mapper import RoadNameMapper

class DHLRouter:
    def __init__(self, cpp_executable_path: str = None):
        """Initialize DHL router with path to C++ JSON API executable"""
        
        # If no path provided, try to find the executable automatically
        if cpp_executable_path is None:
            cpp_executable_path = self._find_dhl_executable()
        
        self.cpp_executable = cpp_executable_path
        if not os.path.exists(cpp_executable_path):
            raise FileNotFoundError(f"DHL JSON API executable not found: {cpp_executable_path}")
        
        # Initialize road name mapper for turn-by-turn directions
        self.road_mapper = RoadNameMapper(self._find_edges_csv())
        
        print(f"✅ Using DHL routing executable: {self.cpp_executable}")
    
    def _find_dhl_executable(self):
        """Find the DHL executable in possible locations"""
        current_dir = os.path.dirname(os.path.abspath(__file__))
        workspace_root = os.path.dirname(current_dir)  # Go up one level from main_app to workspace root
        
        # Try multiple possible paths, prioritizing build directory then main directory
        possible_paths = [
            # First try the build directory (most likely location)
            os.path.join(workspace_root, 'DHL', 'build', 'dhl_routing_json_api'),
            # Then try the main source directory 
            os.path.join(workspace_root, 'DHL', 'dhl_routing_json_api'),
            # Legacy paths for backward compatibility
            '/app/DHL/build/dhl_routing_json_api',  # Docker path
            '../DHL/build/dhl_routing_json_api',   # Relative from main_app
            os.path.abspath('../DHL/build/dhl_routing_json_api'),  # Convert relative to absolute
            # Additional fallback paths
            os.path.abspath('../DHL/dhl_routing_json_api'),  # Main directory relative
        ]
        
        for path in possible_paths:
            if os.path.exists(path):
                return path
        
        # If nothing found, list what we tried for debugging
        paths_tried = '\n'.join(f"  - {path}" for path in possible_paths)
        raise FileNotFoundError(f"DHL executable not found in any of these paths:\n{paths_tried}")
    
    def _find_edges_csv(self):
        """Find the edges CSV file in possible locations"""
        current_dir = os.path.dirname(os.path.abspath(__file__))
        workspace_root = os.path.dirname(current_dir)  # Go up one level from main_app to workspace root
        
        possible_paths = [
            # Relative to workspace root
            os.path.join(workspace_root, 'data', 'raw', 'quezon_city_edges.csv'),
            os.path.join(workspace_root, 'data', 'processed', 'quezon_city_edges.csv'),
            # Legacy paths
            '../data/raw/quezon_city_edges.csv',  # Relative from main_app
            '/app/data/raw/quezon_city_edges.csv',  # Docker path
            '/home/lance/Documents/commissions/LazyHC2L/data/raw/quezon_city_edges.csv',  # Absolute path
            os.path.abspath('../data/raw/quezon_city_edges.csv'),  # Convert relative to absolute
        ]
        
        for path in possible_paths:
            if os.path.exists(path):
                return path
        
        # Return the first path as fallback
        return possible_paths[0]
    
    def compute_route(self, start_lat: float, start_lng: float, 
                     dest_lat: float, dest_lng: float, use_disruptions: bool = False) -> Dict:
        """
        Compute route using DHL algorithm via JSON API
        Returns route data with polylines and metrics
        """
        try:
            print(f"Executing DHL JSON API route computation...")
            
            # Use the refactored JSON API - much simpler now!
            # The JSON API handles all file path detection automatically
            cmd = [
                self.cpp_executable,
                str(start_lat), str(start_lng),
                str(dest_lat), str(dest_lng),
                "true" if use_disruptions else "false"
            ]
            
            print(f"Command: {' '.join(cmd)}")
            
            result = subprocess.run(
                cmd, 
                capture_output=True, 
                text=True, 
                timeout=30
            )
            
            if result.returncode != 0:
                return {
                    'success': False,
                    'error': f"DHL JSON API failed: {result.stderr}",
                    'stdout': result.stdout
                }
            
            # Parse JSON output from DHL JSON API
            try:
                # The DHL executable outputs initialization messages followed by JSON
                # We need to extract just the JSON part
                output_lines = result.stdout.strip()
                
                # Find the start of JSON (first '{' character)
                json_start = output_lines.find('{')
                if json_start == -1:
                    return {
                        'success': False,
                        'error': "No JSON found in DHL output",
                        'raw_output': result.stdout
                    }
                
                # Extract just the JSON part
                json_output = output_lines[json_start:]
                
                dhl_data = json.loads(json_output)
                if not dhl_data.get('success', False):
                    return {
                        'success': False,
                        'error': dhl_data.get('error', 'Unknown DHL error')
                    }
                
                # Convert DHL JSON output to our route format
                parsed_data = self._convert_dhl_to_route_format(dhl_data)
                parsed_data['success'] = True
                parsed_data['raw_dhl_output'] = dhl_data
                
                return parsed_data
                
            except json.JSONDecodeError as e:
                return {
                    'success': False,
                    'error': f"Failed to parse DHL JSON output: {str(e)}",
                    'raw_output': result.stdout
                }
            
        except subprocess.TimeoutExpired:
            return {
                'success': False,
                'error': "DHL JSON API computation timed out (30 seconds)"
            }
        except Exception as e:
            return {
                'success': False,
                'error': f"Error executing DHL JSON API: {str(e)}"
            }
    
    def _convert_dhl_to_route_format(self, dhl_data: Dict) -> Dict:
        """Convert DHL JSON API output to our standard route format"""
        
        # Extract coordinates from the node mapping
        coordinates = []
        path_nodes = dhl_data.get('route', {}).get('path_nodes', [])
        
        # JSON API provides better coordinate data
        complete_trace = dhl_data.get('route', {}).get('complete_trace', '')
        if complete_trace and path_nodes:
            coordinates = self._extract_coordinates_from_trace(complete_trace, path_nodes)
        
        # Fallback: Load coordinate mapping from CSV file if trace parsing failed
        # JSON API provides the actual file paths used
        if not coordinates:
            coord_file = dhl_data.get('data_sources', {}).get('coordinates_file', '')
            if coord_file and path_nodes:
                coordinates = self._load_coordinates_for_nodes(coord_file, path_nodes)
        
        # Create route data structure with enhanced metrics from API
        route_data = {
            'metrics': {
                'algorithm': dhl_data.get('algorithm', 'DHL'),
                'query_time_microseconds': dhl_data.get('metrics', {}).get('query_time_microseconds', 0),
                'query_time_ms': dhl_data.get('metrics', {}).get('query_time_ms', 0),
                'labeling_time_ms': dhl_data.get('metrics', {}).get('labeling_time_ms', 0),
                'labeling_size_bytes': dhl_data.get('metrics', {}).get('labeling_size_bytes', 0),
                'total_distance_units': dhl_data.get('metrics', {}).get('total_distance_units', 0),
                'path_length': dhl_data.get('metrics', {}).get('path_length', 0),
                'hoplinks_examined': dhl_data.get('metrics', {}).get('hoplinks_examined', 0),
                'uses_disruptions': dhl_data.get('metrics', {}).get('uses_disruptions', False),
                'routing_mode': dhl_data.get('metrics', {}).get('routing_mode', 'DHL'),
                # Enhanced index statistics from API
                'index_height': dhl_data.get('index_stats', {}).get('index_height', 0),
                'avg_cut_size': dhl_data.get('index_stats', {}).get('avg_cut_size', 0),
                'total_labels': dhl_data.get('index_stats', {}).get('total_labels', 0),
                'graph_nodes': dhl_data.get('index_stats', {}).get('graph_nodes', 0),
                'graph_edges': dhl_data.get('index_stats', {}).get('graph_edges', 0)
            },
            'route': {
                'path_nodes': path_nodes,
                'coordinates': coordinates,
                'polylines': [],
                'start_node': dhl_data.get('gps_mapping', {}).get('start_node', 0),
                'dest_node': dhl_data.get('gps_mapping', {}).get('dest_node', 0),
                'route_summary': dhl_data.get('route', {}).get('complete_trace', ''),
                'road_segments': self._create_road_segments(coordinates)
            },
            'gps_mapping': dhl_data.get('gps_mapping', {}),
            'disruptions': dhl_data.get('disruptions', {}),
            'data_sources': dhl_data.get('data_sources', {}),
            'input': dhl_data.get('input', {})
        }
        
        # Create polylines from coordinates
        if len(coordinates) >= 2:
            polyline_coords = []
            for coord in coordinates:
                polyline_coords.append([coord['lat'], coord['lng']])
            
            route_data['route']['polylines'] = [{
                'coordinates': polyline_coords,
                'color': '#0066FF',  # Blue for DHL route
                'weight': 5,
                'opacity': 0.8
            }]
        
        # Enhance route with road names and turn-by-turn directions
        route_data = self._enhance_route_with_road_names(route_data)
        
        return route_data
    
    def _extract_coordinates_from_trace(self, complete_trace: str, node_ids: List[int]) -> List[Dict]:
        """Extract GPS coordinates from the complete_trace string"""
        coordinates = []
        
        try:
            # Parse the complete trace to extract coordinates
            # Format: "DHL Route (2184 (14.676090, 121.043758) -> 12130 (14.674896, 121.043668) -> ...)"
            
            # Remove the prefix and suffix
            trace_content = complete_trace
            if trace_content.startswith("DHL Route ("):
                trace_content = trace_content[11:]  # Remove "DHL Route ("
            if trace_content.endswith(")"):
                trace_content = trace_content[:-1]  # Remove final ")"
            
            # Split by " -> " to get individual node entries
            node_entries = trace_content.split(" -> ")
            
            for i, entry in enumerate(node_entries):
                # Parse each entry: "2184 (14.676090, 121.043758)"
                entry = entry.strip()
                
                # Find the node ID (before the first space and parenthesis)
                if "(" in entry and ")" in entry:
                    node_part = entry[:entry.find("(")].strip()
                    coord_start = entry.find("(")
                    coord_end = entry.rfind(")")
                    coord_part = entry[coord_start:coord_end+1]
                    
                    try:
                        node_id = int(node_part)
                        
                        # Extract coordinates from "(lat, lng)"
                        coord_content = coord_part[1:-1]  # Remove parentheses
                        coord_parts = coord_content.split(", ")
                        if len(coord_parts) == 2:
                            lat_str, lng_str = coord_parts
                            lat = float(lat_str)
                            lng = float(lng_str)
                            
                            coordinates.append({
                                'node_id': node_id,
                                'lat': lat,
                                'lng': lng
                            })
                        else:
                            print(f"Warning: Invalid coordinate format in '{entry}': {coord_parts}")
                        
                    except (ValueError, IndexError) as e:
                        print(f"Warning: Could not parse entry '{entry}': {e}")
                        continue
                else:
                    print(f"Warning: No coordinates found in entry '{entry}'")
            
            # Verify we got all the nodes we expected
            if len(coordinates) != len(node_ids):
                print(f"Warning: Expected {len(node_ids)} nodes but extracted {len(coordinates)} coordinates")
            
        except Exception as e:
            print(f"Error extracting coordinates from trace: {e}")
            coordinates = []
        
        return coordinates
    
    def _load_coordinates_for_nodes(self, coord_file: str, node_ids: List[int]) -> List[Dict]:
        """Load GPS coordinates for given node IDs"""
        coordinates = []
        
        try:
            # Try different possible paths for the coordinate file
            possible_paths = [
                coord_file,
                os.path.join('data', 'quezon_city_nodes.csv'),
                os.path.join('..', 'data', 'raw', 'quezon_city_nodes.csv'),
                'quezon_city_nodes.csv'
            ]
            
            coord_data = None
            for path in possible_paths:
                if os.path.exists(path):
                    coord_data = {}
                    with open(path, 'r') as f:
                        # Skip header
                        next(f)
                        for line in f:
                            parts = line.strip().split(',')
                            if len(parts) >= 3:
                                node_id = int(parts[0])
                                lat = float(parts[1])
                                lng = float(parts[2])
                                coord_data[node_id] = {'lat': lat, 'lng': lng}
                    break
            
            if coord_data:
                for node_id in node_ids:
                    if node_id in coord_data:
                        coordinates.append({
                            'node_id': node_id,
                            'lat': coord_data[node_id]['lat'],
                            'lng': coord_data[node_id]['lng']
                        })
                    else:
                        print(f"Warning: Node {node_id} not found in coordinate data")
            
        except Exception as e:
            print(f"Error loading coordinates: {e}")
        
        return coordinates
    
    def _create_road_segments(self, coordinates: List[Dict]) -> List[Dict]:
        """Create road segments from coordinates for turn-by-turn directions"""
        segments = []
        
        for i in range(len(coordinates) - 1):
            current = coordinates[i]
            next_coord = coordinates[i + 1]
            
            # Calculate distance between points (simplified)
            lat_diff = next_coord['lat'] - current['lat']
            lng_diff = next_coord['lng'] - current['lng']
            distance = ((lat_diff ** 2 + lng_diff ** 2) ** 0.5) * 111320  # Rough meters
            
            segments.append({
                'from_node': current['node_id'],
                'to_node': next_coord['node_id'],
                'from_lat': current['lat'],
                'from_lng': current['lng'],
                'to_lat': next_coord['lat'],
                'to_lng': next_coord['lng'],
                'distance_m': round(distance, 1),
                'instruction': f"Head from node {current['node_id']} to node {next_coord['node_id']}"
            })
        
        return segments
    
    def _enhance_route_with_road_names(self, route_data: Dict) -> Dict:
        """Enhance route data with road names and turn-by-turn directions using RoadNameMapper"""
        
        if not route_data.get('route', {}).get('path_nodes'):
            return route_data
        
        try:
            path_nodes = route_data['route']['path_nodes']
            
            if len(path_nodes) < 2:
                print("⚠️  Warning: Path has less than 2 nodes, cannot generate road names")
                return route_data
            
            print(f"🛣️  Enhancing DHL route with road names for {len(path_nodes)} nodes...")
            
            # Get road segments with names using RoadNameMapper
            road_segments = self.road_mapper.get_route_with_road_names(path_nodes)
            route_summary = self.road_mapper.get_route_summary_text(path_nodes)
            turn_directions = self.road_mapper.get_turn_by_turn_directions(path_nodes)
            
            # Update the route data with enhanced road information
            route_data['route']['road_segments'] = road_segments
            route_data['route']['route_summary'] = route_summary
            route_data['route']['turn_by_turn_directions'] = turn_directions
            
            # Create a simplified display format for frontend
            route_data['route']['display_format'] = {
                'node_path': ' → '.join(map(str, path_nodes)),
                'road_path': route_summary,
                'instruction_count': len(turn_directions),
                'road_count': len(road_segments)
            }
            
            # Update existing road_segments with road names if they exist
            if route_data['route'].get('coordinates'):
                for i, segment in enumerate(route_data['route'].get('road_segments', [])):
                    if i < len(road_segments):
                        # Replace the generic instruction with road name instruction
                        segment.update({
                            'road_name': road_segments[i]['road_name'],
                            'highway_type': road_segments[i]['highway_type'],
                            'instruction': road_segments[i]['instruction']
                        })
            
            print(f"✅ Enhanced DHL route with {len(road_segments)} road segments")
            print(f"🛣️  Route summary: {route_summary}")
            
        except Exception as e:
            print(f"⚠️  Warning: Failed to enhance DHL route with road names: {e}")
        
        return route_data
    
    def get_turn_by_turn_directions(self, route_data: Dict) -> List[str]:
        """Get turn-by-turn directions from enhanced route data"""
        if not route_data.get('success', False):
            return []
        
        return route_data.get('route', {}).get('turn_by_turn_directions', [])
    
    def get_route_summary_text(self, route_data: Dict) -> str:
        """Get human-readable route summary with road names"""
        if not route_data.get('success', False):
            return "Route computation failed"
        
        return route_data.get('route', {}).get('route_summary', 'No route summary available')
    
    def get_detailed_route_info(self, route_data: Dict) -> Dict:
        """Get detailed route information including road segments and directions"""
        if not route_data.get('success', False):
            return {'error': 'Route computation failed'}
        
        route = route_data.get('route', {})
        
        return {
            'success': True,
            'path_nodes': route.get('path_nodes', []),
            'road_segments': route.get('road_segments', []),
            'turn_by_turn_directions': route.get('turn_by_turn_directions', []),
            'route_summary': route.get('route_summary', ''),
            'display_format': route.get('display_format', {}),
            'total_nodes': len(route.get('path_nodes', [])),
            'total_road_segments': len(route.get('road_segments', [])),
            'total_instructions': len(route.get('turn_by_turn_directions', []))
        }
    
    def get_route_polylines_for_gmaps(self, route_data: Dict) -> List[Dict]:
        """
        Convert route data to Google Maps polyline format
        """
        if not route_data.get('success', False):
            return []
        
        polylines = []
        
        # Main route polyline
        if route_data['route']['polylines']:
            for polyline in route_data['route']['polylines']:
                polylines.append({
                    'path': [{'lat': coord[0], 'lng': coord[1]} for coord in polyline['coordinates']],
                    'strokeColor': polyline['color'],
                    'strokeOpacity': polyline['opacity'],
                    'strokeWeight': polyline['weight'],
                    'geodesic': True
                })
        
        return polylines
    
    def get_route_summary(self, route_data: Dict) -> Dict:
        """Get summary information for display - enhanced for new JSON API"""
        if not route_data.get('success', False):
            return {'error': route_data.get('error', 'Unknown error')}
        
        metrics = route_data.get('metrics', {})
        
        summary = {
            'algorithm': 'DHL (Dual-Hierarchy Labelling)',
            'routing_mode': metrics.get('routing_mode', 'DHL'),
            'query_time_ms': metrics.get('query_time_ms', 0),
            'query_time_microseconds': metrics.get('query_time_microseconds', 0),
            'labeling_time_ms': metrics.get('labeling_time_ms', 0),
            'labeling_size_mb': metrics.get('labeling_size_bytes', 0) / (1024 * 1024),
            'total_distance_units': metrics.get('total_distance_units', 0),
            'path_length': metrics.get('path_length', 0),
            'hoplinks_examined': metrics.get('hoplinks_examined', 0),
            'uses_disruptions': metrics.get('uses_disruptions', False),
            
            # DHL-specific metrics
            'index_height': metrics.get('index_height', 0),
            'avg_cut_size': round(metrics.get('avg_cut_size', 0), 2),
            'total_labels': metrics.get('total_labels', 0),
            'graph_nodes': metrics.get('graph_nodes', 0),
            'graph_edges': metrics.get('graph_edges', 0),
            'edge_count': len(route_data['route']['coordinates']) - 1 if len(route_data['route']['coordinates']) > 1 else 0,
            
            # Data source information from API
            'data_sources': route_data.get('data_sources', {}),
            'labeling_efficiency': {
                'labels_per_node': round(metrics.get('total_labels', 0) / max(metrics.get('graph_nodes', 1), 1), 2),
                'bytes_per_node': round(metrics.get('labeling_size_bytes', 0) / max(metrics.get('graph_nodes', 1), 1), 2)
            }
        }
        print(f"📝 DHL route summary: {summary}")
        return summary
    
    def compare_routes(self, start_lat: float, start_lng: float, 
                      dest_lat: float, dest_lng: float) -> Dict:
        """Compare routes with and without disruptions"""
        try:
            # Compute route without disruptions
            print("Computing DHL route without disruptions...")
            base_route = self.compute_route(start_lat, start_lng, dest_lat, dest_lng, use_disruptions=False)
            
            # Compute route with disruptions
            print("Computing DHL route with disruptions...")
            disrupted_route = self.compute_route(start_lat, start_lng, dest_lat, dest_lng, use_disruptions=True)
            
            if not base_route['success']:
                return {
                    'success': False,
                    'error': f"Base route failed: {base_route['error']}"
                }
            
            if not disrupted_route['success']:
                return {
                    'success': False,
                    'error': f"Disrupted route failed: {disrupted_route['error']}"
                }
            
            # Create comparison result
            comparison = {
                'success': True,
                'algorithm': 'DHL Comparison',
                'routes': {
                    'base': {
                        'polylines': self.get_route_polylines_for_gmaps(base_route),
                        'summary': self.get_route_summary(base_route),
                        'name': 'DHL Base Route',
                        'color': '#0066FF'
                    },
                    'disrupted': {
                        'polylines': self.get_route_polylines_for_gmaps(disrupted_route),
                        'summary': self.get_route_summary(disrupted_route),
                        'name': 'DHL Disrupted Route',
                        'color': '#FF6600'
                    }
                },
                'comparison_metrics': {
                    'distance_difference_units': (
                        disrupted_route['metrics']['total_distance_units'] - 
                        base_route['metrics']['total_distance_units']
                    ),
                    'query_time_difference_ms': (
                        disrupted_route['metrics']['query_time_ms'] - 
                        base_route['metrics']['query_time_ms']
                    ),
                    'path_length_difference': (
                        disrupted_route['metrics']['path_length'] - 
                        base_route['metrics']['path_length']
                    ),
                    'blocked_edges_count': len(disrupted_route.get('disruptions', {}).get('blocked_edges', [])),
                    'blocked_nodes_count': len(disrupted_route.get('disruptions', {}).get('blocked_nodes', []))
                }
            }
            
            return comparison
            
        except Exception as e:
            return {
                'success': False,
                'error': f"DHL route comparison error: {str(e)}"
            }

# Test function
if __name__ == "__main__":
    # Test with automatic path detection
    print("🧪 Testing DHL Router with automatic path detection...")
    
    try:
        router = DHLRouter()  # No path specified - will auto-detect
        
        print("Testing DHL Router with new JSON API...")
        result = router.compute_route(14.6760, 121.0437, 14.6348, 121.0480, use_disruptions=False)
        
        if result['success']:
            print("✅ DHL JSON API route computation successful!")
            print(f"Metrics: {result['metrics']}")
            print(f"Route nodes: {len(result['route']['path_nodes'])}")
            print(f"Coordinates: {len(result['route']['coordinates'])}")
            
            # Test polyline generation
            polylines = router.get_route_polylines_for_gmaps(result)
            print(f"Generated {len(polylines)} polylines for Google Maps")
            
            # Test summary
            summary = router.get_route_summary(result)
            print(f"Route summary: {summary}")
            
            # Test NEW road name and turn-by-turn direction features
            print("\n🛣️  Testing road name mapping and turn-by-turn directions...")
            
            # Get turn-by-turn directions
            directions = router.get_turn_by_turn_directions(result)
            print(f"✅ Generated {len(directions)} turn-by-turn directions:")
            for i, direction in enumerate(directions[:5]):  # Show first 5 directions
                print(f"   {direction}")
            if len(directions) > 5:
                print(f"   ... and {len(directions) - 5} more directions")
            
            # Get route summary with road names
            route_summary = router.get_route_summary_text(result)
            print(f"\n🗺️  Route summary with road names:")
            print(f"   {route_summary}")
            
            # Get detailed route information
            detailed_info = router.get_detailed_route_info(result)
            if detailed_info.get('success'):
                print(f"\n📊 Detailed route information:")
                print(f"   Total nodes: {detailed_info['total_nodes']}")
                print(f"   Total road segments: {detailed_info['total_road_segments']}")
                print(f"   Total instructions: {detailed_info['total_instructions']}")
                
                # Show some road segments
                road_segments = detailed_info.get('road_segments', [])
                if road_segments:
                    print(f"\n🛣️  Road segments preview:")
                    for i, segment in enumerate(road_segments[:3]):  # Show first 3 segments
                        print(f"   {i+1}. {segment.get('road_name', 'Unknown')} ({segment.get('highway_type', 'unknown')})")
                        print(f"      {segment.get('instruction', 'No instruction')}")
                    if len(road_segments) > 3:
                        print(f"   ... and {len(road_segments) - 3} more segments")
            
            # Test comparison
            print("\n🔄 Testing route comparison...")
            comparison = router.compare_routes(14.6760, 121.0437, 14.6507, 121.0323)
            if comparison['success']:
                print("✅ Route comparison successful!")
                print(f"Comparison metrics: {comparison['comparison_metrics']}")
            else:
                print(f"❌ Route comparison failed: {comparison['error']}")
        else:
            print("❌ DHL JSON API route computation failed:")
            print(result['error'])
            
    except FileNotFoundError as e:
        print(f"❌ DHL Router initialization failed: {e}")
        print("Please build the DHL project first.")
        print("Build command: cd ../DHL && make dhl_routing_json_api")
    except Exception as e:
        print(f"❌ Unexpected error: {e}")