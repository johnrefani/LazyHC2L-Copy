# gps_hc2l_router.py - Enhanced GPS HC2L Routing Integration with Road Names
import subprocess
import json
import os
import csv
from typing import Dict, List, Tuple, Optional
from road_name_mapper import RoadNameMapper

class GPSRoutingService:
    """
    Python wrapper for the C++ GPS Routing Service
    Provides easy integration with Flask applications
    """
    
    def __init__(self, cpp_executable_path: str = None, nodes_csv_path: str = None):
        """Initialize GPS HC2L router with path to C++ executable and nodes data"""
        
        # Default to the JSON API executable we just created
        if cpp_executable_path is None:
            # Get the workspace root directory
            current_dir = os.path.dirname(os.path.abspath(__file__))
            workspace_root = os.path.dirname(current_dir)  # Go up one level from main_app to workspace root
            
            # Try multiple possible paths, prioritizing build directory then main directory
            possible_paths = [
                # First try the build directory (most likely location)
                os.path.join(workspace_root, 'core', 'hc2l_dynamic', 'build', 'gps_routing_json_api'),
                os.path.join(workspace_root, 'build', 'core', 'hc2l_dynamic', 'gps_routing_json_api'),
                # Then try the main source directory 
                os.path.join(workspace_root, 'core', 'hc2l_dynamic', 'gps_routing_json_api'),
                # Legacy paths for backward compatibility
                '/app/core/hc2l_dynamic/build/gps_routing_json_api',  # Docker path
                '../core/hc2l_dynamic/build/gps_routing_json_api',   # Relative from main_app
                '/home/lance/Documents/commissions/LazyHC2L/core/hc2l_dynamic/build/gps_routing_json_api',  # Absolute path
                os.path.abspath('../core/hc2l_dynamic/build/gps_routing_json_api'),  # Convert relative to absolute
                # Additional fallback paths
                os.path.abspath('../core/hc2l_dynamic/gps_routing_json_api'),  # Main directory relative
                '/home/lance/Documents/commissions/LazyHC2L/core/hc2l_dynamic/gps_routing_json_api',  # Main directory absolute
            ]
            
            self.cpp_executable = None
            for path in possible_paths:
                if os.path.exists(path):
                    self.cpp_executable = path
                    break
            
            if self.cpp_executable is None:
                # List what we tried for debugging
                paths_tried = '\n'.join(f"  - {path}" for path in possible_paths)
                raise FileNotFoundError(f"C++ executable not found in any of these paths:\n{paths_tried}")
        else:
            self.cpp_executable = cpp_executable_path
            
        if not os.path.exists(self.cpp_executable):
            raise FileNotFoundError(f"C++ executable not found: {self.cpp_executable}")
        
        print(f"✅ Using GPS routing executable: {self.cpp_executable}")
        
        # Load nodes data for coordinate lookup
        self.nodes_csv_path = nodes_csv_path or self._find_nodes_csv()
        self.nodes_data = None
        self._load_nodes_data()
        
        # Initialize road name mapper
        self.road_mapper = RoadNameMapper(self._find_edges_csv())
    
    def _find_nodes_csv(self):
        """Find the nodes CSV file in possible locations"""
        current_dir = os.path.dirname(os.path.abspath(__file__))
        workspace_root = os.path.dirname(current_dir)  # Go up one level from main_app to workspace root
        
        possible_paths = [
            # Relative to workspace root
            os.path.join(workspace_root, 'data', 'raw', 'quezon_city_nodes.csv'),
            os.path.join(workspace_root, 'data', 'processed', 'quezon_city_nodes.csv'),
            # Legacy paths
            '../data/raw/quezon_city_nodes.csv',  # Relative from main_app
            '/app/data/raw/quezon_city_nodes.csv',  # Docker path
            '/home/lance/Documents/commissions/LazyHC2L/data/raw/quezon_city_nodes.csv',  # Absolute path
            os.path.abspath('../data/raw/quezon_city_nodes.csv'),  # Convert relative to absolute
        ]
        
        for path in possible_paths:
            if os.path.exists(path):
                return path
        
        # Return the first path as fallback (will cause error in _load_nodes_data)
        return possible_paths[0]
    
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
    
    def _load_nodes_data(self):
        """Load nodes CSV data for coordinate lookup"""
        try:
            if os.path.exists(self.nodes_csv_path):
                # Load CSV using standard library
                self.nodes_data = {}
                with open(self.nodes_csv_path, 'r') as f:
                    reader = csv.DictReader(f)
                    for row in reader:
                        node_id = int(row['node_id'])
                        self.nodes_data[node_id] = {
                            'latitude': float(row['latitude']),
                            'longitude': float(row['longitude'])
                        }
                print(f"✅ Loaded {len(self.nodes_data)} nodes from {self.nodes_csv_path}")
            else:
                print(f"⚠️  Warning: Nodes file not found at {self.nodes_csv_path}")
                print("    Route coordinates will be limited to start/end points")
        except Exception as e:
            print(f"❌ Error loading nodes data: {e}")
            self.nodes_data = None
    
    def compute_route(self, start_lat: float, start_lng: float, 
                     dest_lat: float, dest_lng: float, 
                     use_disruptions: bool = False, threshold: float = 0.5) -> Dict:
        """
        Compute route using HC2L GPS Routing Service
        Returns enhanced route data with Google Maps integration
        """
        try:
            # Call C++ executable
            disruption_flag = "true" if use_disruptions else "false"
            cmd = [
                self.cpp_executable,
                str(start_lat), str(start_lng),
                str(dest_lat), str(dest_lng),
                disruption_flag,
                str(threshold)
            ]
            
            print(f"🚀 Executing GPS HC2L routing: {' '.join(cmd)}")
            
            # Execute from the HC2L dynamic directory where data files exist
            current_dir = os.path.dirname(os.path.abspath(__file__))
            workspace_root = os.path.dirname(current_dir)  # Go up one level from main_app to workspace root
            
            # Try to find the hc2l_dynamic directory
            possible_hc2l_dirs = [
                os.path.join(workspace_root, 'core', 'hc2l_dynamic'), 
                '/app/core/hc2l_dynamic/',  # Docker path
                '../core/hc2l_dynamic/',  # Local relative path
                os.path.abspath('../core/hc2l_dynamic/')  # Convert relative to absolute
            ]
            
            hc2l_dir = None
            for dir_path in possible_hc2l_dirs:
                if os.path.exists(dir_path):
                    hc2l_dir = dir_path
                    break
            
            if hc2l_dir is None:
                hc2l_dir = possible_hc2l_dirs[0]  # Use first as fallback

            print(f"📁 Working directory: {hc2l_dir}")


            result = subprocess.run(
                cmd, 
                capture_output=True, 
                text=True, 
                timeout=60,
                cwd=hc2l_dir
            )
            
            if result.returncode != 0:
                return {
                    'success': False,
                    'error': f"GPS routing failed: {result.stderr}",
                    'stdout': result.stdout
                }
            
            # Parse JSON output
            try:
                # Extract JSON from output (might have debug info before it)
                output_lines = result.stdout.strip().split('\n')
                json_line = None
                
                for line in output_lines:
                    line = line.strip()
                    if line.startswith('{') and line.endswith('}'):
                        json_line = line
                        break
                
                if json_line is None:
                    return {
                        'success': False,
                        'error': 'No JSON output found',
                        'raw_output': result.stdout
                    }
                
                route_data = json.loads(json_line)
                
                # Debug: Print the metrics received from C++ API
                if route_data.get('success', False):
                    metrics = route_data.get('metrics', {})
                    print(f"📊 Received metrics from C++ API:")
                    print(f"   🕐 Query time: {metrics.get('query_time_ms', 'N/A')} ms")
                    print(f"   📏 Distance: {metrics.get('total_distance_meters', 'N/A')} m")
                    print(f"   🏷️  Labeling size: {metrics.get('labeling_size_mb', 'N/A')} MB")
                    print(f"   ⏱️  Labeling time: {metrics.get('labeling_time_ms', 'N/A')} s")
                
                # Enhance with coordinate data if available
                if route_data.get('success', False):
                    route_data = self._enhance_route_with_coordinates(route_data)
                    route_data = self._enhance_route_with_road_names(route_data)
                
                return route_data
                
            except json.JSONDecodeError as e:
                return {
                    'success': False,
                    'error': f"Failed to parse JSON output: {e}",
                    'raw_output': result.stdout
                }
            
        except subprocess.TimeoutExpired:
            return {
                'success': False,
                'error': "GPS route computation timed out (60 seconds)"
            }
        except Exception as e:
            return {
                'success': False,
                'error': f"Error executing GPS route computation: {str(e)}"
            }
    
    def _enhance_route_with_coordinates(self, route_data: Dict) -> Dict:
        """Enhance route data with actual GPS coordinates for all path nodes"""
        
        if not route_data.get('success', False) or self.nodes_data is None:
            return route_data
        
        try:
            path_nodes = route_data.get('route', {}).get('path_nodes', [])
            
            if not path_nodes:
                return route_data
            
            # Get coordinates for all nodes in the path
            enhanced_coordinates = []
            
            for node_id in path_nodes:
                # Find node in our nodes data
                if node_id in self.nodes_data:
                    node_info = self.nodes_data[node_id]
                    enhanced_coordinates.append({
                        'node_id': int(node_id),
                        'lat': float(node_info['latitude']),
                        'lng': float(node_info['longitude'])
                    })
                else:
                    print(f"⚠️  Warning: Node {node_id} not found in nodes data")
            
            # Update route data with enhanced coordinates
            route_data['route']['coordinates'] = enhanced_coordinates
            
            # Create polylines for Google Maps
            if len(enhanced_coordinates) >= 2:
                route_data['route']['polylines'] = [{
                    'path': [{'lat': coord['lat'], 'lng': coord['lng']} for coord in enhanced_coordinates],
                    'strokeColor': '#FF0000',  # Red for HC2L route
                    'strokeOpacity': 0.8,
                    'strokeWeight': 5,
                    'geodesic': True
                }]
            
            print(f"✅ Enhanced route with {len(enhanced_coordinates)} GPS coordinates")
            
        except Exception as e:
            print(f"⚠️  Warning: Failed to enhance coordinates: {e}")
        
        return route_data
    
    def _enhance_route_with_road_names(self, route_data: Dict) -> Dict:
        """Enhance route data with road names and turn-by-turn directions"""
        
        if not route_data.get('success', False):
            return route_data
        
        try:
            path_nodes = route_data.get('route', {}).get('path_nodes', [])
            
            if not path_nodes or len(path_nodes) < 2:
                return route_data
            # print(f'Route Data: {route_data}')
            # print(f'Path Nodes: {path_nodes}')
            # Get road segments with names
            road_segments = self.road_mapper.get_route_with_road_names(path_nodes)
            route_summary = self.road_mapper.get_route_summary_text(path_nodes)

            turn_directions = self.road_mapper.get_turn_by_turn_directions(path_nodes)
            
            # Add road information to route data
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
            
            print(f"✅ Enhanced route with {len(road_segments)} road segments")
            print(f"🛣️  Route summary: {route_summary}")
            
        except Exception as e:
            print(f"⚠️  Warning: Failed to enhance road names: {e}")
        
        return route_data
    
    def get_route_polylines_for_gmaps(self, route_data: Dict) -> List[Dict]:
        """
        Convert route data to Google Maps polyline format
        """
        if not route_data.get('success', False):
            return []
        
        return route_data.get('route', {}).get('polylines', [])
    
    def get_route_summary(self, route_data: Dict) -> Dict:
        """Get summary information for display"""
        if not route_data.get('success', False):
            return {'error': route_data.get('error', 'Unknown error')}
        
        metrics = route_data.get('metrics', {})
        
        summary = {
            'algorithm': route_data.get('algorithm', 'D-HC2L Dynamic GPS'),
            'algorithm_base': route_data.get('algorithm_base', 'D-HC2L'),
            'query_time_ms': metrics.get('query_time_ms', 0),
            'query_time_microseconds': metrics.get('query_time_microseconds', 0),
            'total_distance_meters': metrics.get('total_distance_meters', 0),  # Keep original field name
            'total_distance_m': metrics.get('total_distance_meters', 0),       # Also provide alternative
            'path_length': metrics.get('path_length', 0),
            'routing_mode': metrics.get('routing_mode', 'BASE'),
            'uses_disruptions': metrics.get('uses_disruptions', False),
            'tau_threshold': metrics.get('tau_threshold', 0.5),
            
            # NEW: Add mode-specific information
            'update_strategy': metrics.get('update_strategy', 'none'),
            'mode_explanation': metrics.get('mode_explanation', ''),
            'labels_status': metrics.get('labels_status', 'original'),
            
            # NEW: Add labeling metrics
            'labeling_size_mb': metrics.get('labeling_size_mb', 0.0),
            'labeling_time_ms': metrics.get('labeling_time_seconds', 0.0)
        }
        
        # Add disruption comparison if available
        if metrics.get('distance_difference_meters') is not None:
            summary.update({
                'base_distance_meters': metrics.get('base_distance_meters', 0),
                'base_distance_m': metrics.get('base_distance_meters', 0),
                'distance_difference_meters': metrics.get('distance_difference_meters', 0),
                'distance_difference_m': metrics.get('distance_difference_meters', 0),
                'distance_change_percentage': metrics.get('distance_change_percentage', 0),
                'route_comparison': metrics.get('route_comparison', '')
            })
        
        return summary
    
    def compare_routes(self, start_lat: float, start_lng: float, 
                      dest_lat: float, dest_lng: float) -> Dict:
        """Compare base route vs disrupted route"""
        
        # Get base route
        base_route = self.compute_route(start_lat, start_lng, dest_lat, dest_lng, False)
        
        # Get disrupted route
        disrupted_route = self.compute_route(start_lat, start_lng, dest_lat, dest_lng, True)
        
        if not base_route.get('success') or not disrupted_route.get('success'):
            return {
                'success': False,
                'error': 'Failed to compute one or both routes for comparison'
            }
        
        # Create comparison
        comparison = {
            'success': True,
            'routes': {
                'base': {
                    'polylines': self.get_route_polylines_for_gmaps(base_route),
                    'metrics': self.get_route_summary(base_route),
                    'color': '#0000FF',  # Blue for base
                    'name': 'HC2L Base Route'
                },
                'disrupted': {
                    'polylines': self.get_route_polylines_for_gmaps(disrupted_route),
                    'metrics': self.get_route_summary(disrupted_route),
                    'color': '#FF0000',  # Red for disrupted
                    'name': 'HC2L Disrupted Route'
                }
            },
            'comparison': {
                'distance_difference_m': (
                    disrupted_route['metrics']['total_distance_meters'] - 
                    base_route['metrics']['total_distance_meters']
                ),
                'time_difference_ms': (
                    disrupted_route['metrics']['query_time_ms'] - 
                    base_route['metrics']['query_time_ms']
                )
            }
        }
        
        return comparison
    
    def get_network_stats(self) -> Dict:
        """Get network statistics"""
        return {
            'algorithm': 'HC2L Dynamic with GPS Integration',
            'dataset': 'Quezon City, Philippines',
            'nodes': len(self.nodes_data) if self.nodes_data is not None else 13649,
            'coverage': 'Real GPS coordinates with traffic disruption support'
        }

# Test function
def test_hc2l_routing():
    """
    Comprehensive test function for HC2L GPS routing service
    Tests both base and disrupted routing with real coordinates
    """
    print("🧪 Starting HC2L GPS Routing Service Tests")
    print("=" * 60)
    
    try:
        # Initialize the GPS routing service
        print("\n📡 Initializing GPS Routing Service...")
        service = GPSRoutingService()
        
        # Test coordinates (Quezon City, Philippines)
        test_cases = [
            {
                'name': 'Short Distance Route',
                'start_lat': 14.6760,
                'start_lng': 121.0437,
                'dest_lat': 14.6542,
                'dest_lng': 121.0790,
                'description': 'Route within Quezon City'
            },
            {
                'name': 'Medium Distance Route',
                'start_lat': 14.6420,
                'start_lng': 121.0580,
                'dest_lat': 14.6455,
                'dest_lng': 121.0572,
                'description': 'Cross-district route'
            }
        ]
        
        print(f"✅ Service initialized successfully!")
        print(f"📊 Network stats: {service.get_network_stats()}")
        
        # Test each route case
        for i, test_case in enumerate(test_cases, 1):
            print(f"\n🗺️  Test Case {i}: {test_case['name']}")
            print(f"   {test_case['description']}")
            print(f"   From: ({test_case['start_lat']}, {test_case['start_lng']})")
            print(f"   To: ({test_case['dest_lat']}, {test_case['dest_lng']})")
            print("-" * 50)
            
            # Test 1: Base routing (no disruptions)
            print("\n🔵 Testing BASE routing (no disruptions)...")
            base_result = service.compute_route(
                test_case['start_lat'], test_case['start_lng'],
                test_case['dest_lat'], test_case['dest_lng'],
                use_disruptions=False
            )
            
            if base_result.get('success'):
                base_summary = service.get_route_summary(base_result)
                print("   ✅ Base route computed successfully!")
                print(f"   📏 Distance: {base_summary.get('total_distance_meters', 0)} meters")
                print(f"   ⏱️  Query time: {base_summary.get('query_time_ms', 0):.3f} ms")
                print(f"   🗺️  Path length: {base_summary.get('path_length', 0)} nodes")
                
                # Check if route has coordinates
                coordinates = base_result.get('route', {}).get('coordinates', [])
                print(f"   🌍 GPS coordinates: {len(coordinates)} points")
                
                # Check if route has road names
                road_segments = base_result.get('route', {}).get('road_segments', [])
                if road_segments:
                    print(f"   🛣️  Road segments: {len(road_segments)} segments")
                    print(f"   📝 Route summary: {base_result.get('route', {}).get('route_summary', 'N/A')[:100]}...")
                
                # Check polylines for Google Maps
                polylines = service.get_route_polylines_for_gmaps(base_result)
                print(f"   📍 Google Maps polylines: {len(polylines)} polylines")
                
            else:
                print(f"   ❌ Base route failed: {base_result.get('error', 'Unknown error')}")
                if 'raw_output' in base_result:
                    print(f"   🔍 Raw output: {base_result['raw_output'][:200]}...")
            
            # Test 2: Disrupted routing
            print("\n🔴 Testing DISRUPTED routing...")
            disrupted_result = service.compute_route(
                test_case['start_lat'], test_case['start_lng'],
                test_case['dest_lat'], test_case['dest_lng'],
                use_disruptions=True
            )
            
            if disrupted_result.get('success'):
                disrupted_summary = service.get_route_summary(disrupted_result)
                print("   ✅ Disrupted route computed successfully!")
                print(f"   📏 Distance: {disrupted_summary.get('total_distance_meters', 0)} meters")
                print(f"   ⏱️  Query time: {disrupted_summary.get('query_time_ms', 0):.3f} ms")
                print(f"   🗺️  Path length: {disrupted_summary.get('path_length', 0)} nodes")
                print(f"   🚧 Uses disruptions: {disrupted_summary.get('uses_disruptions', False)}")
                
            else:
                print(f"   ❌ Disrupted route failed: {disrupted_result.get('error', 'Unknown error')}")
                if 'raw_output' in disrupted_result:
                    print(f"   🔍 Raw output: {disrupted_result['raw_output'][:200]}...")
            
            # Test 3: Route comparison
            if base_result.get('success') and disrupted_result.get('success'):
                print("\n🆚 Testing route comparison...")
                comparison = service.compare_routes(
                    test_case['start_lat'], test_case['start_lng'],
                    test_case['dest_lat'], test_case['dest_lng']
                )
                
                if comparison.get('success'):
                    comp_data = comparison.get('comparison', {})
                    print("   ✅ Route comparison successful!")
                    print(f"   📊 Distance difference: {comp_data.get('distance_difference_m', 0)} meters")
                    print(f"   ⏱️  Time difference: {comp_data.get('time_difference_ms', 0):.3f} ms")
                    
                    # Show route names
                    base_name = comparison['routes']['base']['name']
                    disrupted_name = comparison['routes']['disrupted']['name']
                    print(f"   🔵 {base_name}: {comparison['routes']['base']['metrics'].get('total_distance_meters', 0)}m")
                    print(f"   🔴 {disrupted_name}: {comparison['routes']['disrupted']['metrics'].get('total_distance_meters', 0)}m")
                else:
                    print(f"   ❌ Route comparison failed: {comparison.get('error', 'Unknown error')}")
        
        # Test 4: Error handling
        print(f"\n🚨 Testing error handling...")
        print("   Testing with invalid coordinates...")
        
        invalid_result = service.compute_route(
            999.0, 999.0,  # Invalid coordinates
            999.0, 999.0,
            use_disruptions=False
        )
        
        if not invalid_result.get('success'):
            print("   ✅ Error handling works correctly!")
            print(f"   📝 Error message: {invalid_result.get('error', 'No error message')}")
        else:
            print("   ⚠️  Warning: Invalid coordinates did not trigger error")
        
        # Summary
        print("\n" + "=" * 60)
        print("📋 TEST SUMMARY")
        print("=" * 60)
        print("✅ HC2L GPS Routing Service tests completed!")
        print("🔍 Check the output above for any failures or warnings.")
        print("🌐 If all tests passed, the service is ready for web integration.")
        
    except FileNotFoundError as e:
        print(f"❌ Initialization failed: {e}")
        print("💡 Make sure the C++ executable exists and is built correctly.")
        print("🔧 Build the executable with: make gps_routing_json_api")
        
    except Exception as e:
        print(f"❌ Test failed with exception: {e}")
        print("🔍 Check the error details above for debugging information.")

def test_hc2l_performance():
    """
    Performance test for HC2L routing service
    Tests multiple routes to measure average performance
    """
    print("\n⚡ HC2L Performance Test")
    print("=" * 40)
    
    try:
        service = GPSRoutingService()
        
        # Performance test coordinates
        test_routes = [
            (14.6760, 121.0437, 14.6542, 121.0790),
            (14.6420, 121.0580, 14.6455, 121.0572)
        ]
        
        print(f"🚀 Testing {len(test_routes)} routes for performance...")
        
        total_time = 0
        successful_routes = 0
        
        for i, (start_lat, start_lng, dest_lat, dest_lng) in enumerate(test_routes, 1):
            print(f"   Route {i}: ({start_lat}, {start_lng}) → ({dest_lat}, {dest_lng})")
            
            result = service.compute_route(
                start_lat, start_lng, dest_lat, dest_lng, use_disruptions=False
            )
            
            if result.get('success'):
                summary = service.get_route_summary(result)
                query_time = summary.get('query_time_ms', 0)
                total_time += query_time
                successful_routes += 1
                print(f"      ✅ {query_time:.3f} ms")
            else:
                print(f"      ❌ Failed: {result.get('error', 'Unknown')}")
        
        if successful_routes > 0:
            avg_time = total_time / successful_routes
            print(f"\n📊 Performance Results:")
            print(f"   Successful routes: {successful_routes}/{len(test_routes)}")
            print(f"   Average query time: {avg_time:.3f} ms")
            print(f"   Total time: {total_time:.3f} ms")
            
            if avg_time < 10:
                print("   🏆 Excellent performance! (<10ms average)")
            elif avg_time < 50:
                print("   ✅ Good performance! (<50ms average)")
            else:
                print("   ⚠️  Performance may need optimization (>50ms average)")
        else:
            print("   ❌ No successful routes for performance analysis")
            
    except Exception as e:
        print(f"❌ Performance test failed: {e}")

if __name__ == "__main__":
    # Run comprehensive tests
    test_hc2l_routing()
    
    # Run performance tests
    # test_hc2l_performance()
    