from flask import Flask, render_template, request, jsonify
import requests
import os
from typing import Dict, Any, Optional
from dotenv import load_dotenv
from dynamic_hc2l_wrapper import get_hc2l_wrapper
from dhl_wrapper import run_dhl_algorithm
import logging

# Load environment variables from .env file
load_dotenv()

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

app = Flask(__name__)

# Configuration
app.config['SECRET_KEY'] = os.getenv('SECRET_KEY', 'your-secret-key-here')
GOOGLE_MAPS_API_KEY = os.getenv('GOOGLE_MAPS_API_KEY', 'your-google-maps-api-key-here')


@app.route('/')
def navigation_system():
    """Serve the D-HC2L Navigation System interface."""
    return render_template('navigation_system.html', api_key=GOOGLE_MAPS_API_KEY)


@app.route('/directions', methods=['POST'])
def get_directions():
    """
    Get directions using Dynamic-HC2L algorithm as primary method,
    with Google Maps as fallback.
    
    Expected JSON payload:
    {
        "origin": "starting location or lat,lng",
        "destination": "ending location or lat,lng",
        "mode": "driving" (optional),
        "use_hc2l": true (optional, default: true)
    }
    """
    try:
        data = request.get_json()
        
        if not data or 'origin' not in data or 'destination' not in data:
            return jsonify({
                'error': 'Missing required parameters: origin and destination'
            }), 400
        
        origin = data['origin']
        destination = data['destination']
        travel_mode = data.get('mode', 'driving')
        use_hc2l = data.get('use_hc2l', True)
        
        logger.info(f"Directions request: {origin} -> {destination}, HC2L: {use_hc2l}")
        
        # Try to parse coordinates from origin/destination
        origin_coords = parse_coordinates(origin)
        dest_coords = parse_coordinates(destination)
        
        # If we have coordinates and user wants HC2L, use it
        if use_hc2l and origin_coords and dest_coords:
            try:
                wrapper = get_hc2l_wrapper()
                hc2l_result = wrapper.calculate_optimal_route(
                    origin_coords['lat'], origin_coords['lng'],
                    dest_coords['lat'], dest_coords['lng'],
                    tau_threshold=0.5
                )
                
                if hc2l_result.get('success'):
                    # Convert HC2L result to directions format
                    directions_result = convert_hc2l_to_directions(hc2l_result, origin, destination)
                    return jsonify({
                        'success': True,
                        'directions': directions_result,
                        'algorithm': 'Dynamic-HC2L',
                        'hc2l_data': hc2l_result
                    })
                else:
                    logger.warning(f"HC2L failed: {hc2l_result.get('error')}, falling back to Google Maps")
            except Exception as e:
                logger.warning(f"HC2L error: {e}, falling back to Google Maps")
        
        # Fallback to Google Maps Directions API
        google_result = get_google_directions(origin, destination, travel_mode)
        
        if google_result:
            return jsonify({
                'success': True,
                'directions': google_result,
                'algorithm': 'Google Maps'
            })
        else:
            return jsonify({
                'error': 'Could not find directions between the specified locations'
            }), 404
            
    except Exception as e:
        return jsonify({
            'error': f'An error occurred: {str(e)}'
        }), 500


def parse_coordinates(coord_string: str) -> Optional[Dict[str, float]]:
    """
    Parse coordinates from string format.
    
    Args:
        coord_string: String in format "lat,lng" or address
        
    Returns:
        Dictionary with lat/lng or None if not parseable
    """
    try:
        # Try to parse "lat,lng" format
        if ',' in coord_string:
            parts = coord_string.split(',')
            if len(parts) == 2:
                lat = float(parts[0].strip())
                lng = float(parts[1].strip())
                if -90 <= lat <= 90 and -180 <= lng <= 180:
                    return {'lat': lat, 'lng': lng}
    except (ValueError, IndexError):
        pass
    return None


def convert_hc2l_to_directions(hc2l_result: Dict[Any, Any], origin: str, destination: str) -> Dict[Any, Any]:
    """
    Convert HC2L result to Google Directions API format.
    
    Args:
        hc2l_result: Result from HC2L algorithm
        origin: Origin address/coordinates
        destination: Destination address/coordinates
        
    Returns:
        Directions in Google Maps format
    """
    # Extract distance (use disrupted distance if available, otherwise base)
    distance_val = hc2l_result.get('disrupted_distance') or hc2l_result.get('base_distance', 0)
    
    # Estimate duration based on distance (rough estimate: 50 km/h average speed in city)
    duration_hours = distance_val / 50.0
    duration_minutes = int(duration_hours * 60)
    
    # Get timing metrics
    timing_metrics = hc2l_result.get('timing_metrics', {})
    
    return {
        'distance': f'{distance_val:.1f} km',
        'duration': f'{duration_minutes} mins',
        'start_address': origin,
        'end_address': destination,
        'steps': [
            {
                'instruction': f'Route optimized using Dynamic-HC2L algorithm',
                'distance': f'{distance_val:.1f} km',
                'duration': f'{duration_minutes} mins'
            },
            {
                'instruction': f'Disruptions considered: {hc2l_result.get("disruptions_considered", 0)}',
                'distance': '0 km',
                'duration': '0 mins'
            },
            {
                'instruction': f'Arrive at destination',
                'distance': '0 km', 
                'duration': '0 mins'
            }
        ],
        'polyline': 'hc2l_optimized_route',
        'algorithm_metrics': {
            'query_time_ms': (timing_metrics.get('query_response_time', 0) * 1000),
            'labeling_time_ms': (timing_metrics.get('labeling_time', 0) * 1000),
            'labeling_size': timing_metrics.get('labeling_size', 0),
            'execution_time_ms': (hc2l_result.get('execution_time_seconds', 0) * 1000),
            'tau_threshold': hc2l_result.get('tau_threshold', 0.5)
        }
    }


def get_google_directions(origin: str, destination: str, mode: str = 'driving') -> Optional[Dict[Any, Any]]:
    """
    Call Google Maps Directions API to get directions between two points.
    
    Args:
        origin: Starting location (address, coordinates, or place name)
        destination: Ending location (address, coordinates, or place name)
        mode: Travel mode (driving, walking, bicycling, transit)
    
    Returns:
        Dictionary containing direction information or None if error
    """
    if GOOGLE_MAPS_API_KEY == 'your-google-maps-api-key-here':
        # Return mock data for demo purposes when API key is not set
        return get_mock_directions(origin, destination)
    
    try:
        url = 'https://maps.googleapis.com/maps/api/directions/json'
        params = {
            'origin': origin,
            'destination': destination,
            'mode': mode,
            'key': GOOGLE_MAPS_API_KEY
        }
        
        response = requests.get(url, params=params)
        response.raise_for_status()
        
        data = response.json()
        
        if data['status'] == 'OK' and data['routes']:
            route = data['routes'][0]
            leg = route['legs'][0]
            
            return {
                'distance': leg['distance']['text'],
                'duration': leg['duration']['text'],
                'start_address': leg['start_address'],
                'end_address': leg['end_address'],
                'steps': [
                    {
                        'instruction': step['html_instructions'],
                        'distance': step['distance']['text'],
                        'duration': step['duration']['text']
                    }
                    for step in leg['steps']
                ],
                'polyline': route['overview_polyline']['points']
            }
        else:
            print(f"Google Maps API error: {data.get('status', 'Unknown error')}")
            return None
            
    except requests.RequestException as e:
        print(f"Request error: {e}")
        return None
    except Exception as e:
        print(f"Unexpected error: {e}")
        return None


def get_mock_directions(origin: str, destination: str) -> Dict[Any, Any]:
    """
    Return mock direction data for demonstration purposes.
    This is used when no valid Google Maps API key is provided.
    """
    return {
        'distance': '5.2 km',
        'duration': '12 mins',
        'start_address': f'{origin} (Mock)',
        'end_address': f'{destination} (Mock)',
        'steps': [
            {
                'instruction': f'Head towards {destination}',
                'distance': '2.1 km',
                'duration': '5 mins'
            },
            {
                'instruction': 'Continue straight',
                'distance': '1.8 km',
                'duration': '4 mins'
            },
            {
                'instruction': f'Arrive at {destination}',
                'distance': '1.3 km',
                'duration': '3 mins'
            }
        ],
        'polyline': 'mock_polyline_data'
    }


@app.route('/optimize-route', methods=['POST'])
def optimize_route():
    """
    Optimize route using Dynamic-HC2L algorithm with disruption data.
    Also provides Google Maps route for comparison.
    
    Expected JSON payload:
    {
        "origin_lat": 14.6760,
        "origin_lng": 121.0437,
        "dest_lat": 14.6900,
        "dest_lng": 121.0500,
        "user_disruptions": [
            {
                "from_node": 2,
                "to_node": 4,
                "type": "User_Accident",
                "severity": "Heavy"
            }
        ],
        "tau_threshold": 0.5
    }
    """
    try:
        data = request.get_json()
        
        if not data:
            return jsonify({
                'error': 'No JSON data provided'
            }), 400
        
        # Validate required parameters
        required_params = ['origin_lat', 'origin_lng', 'dest_lat', 'dest_lng']
        for param in required_params:
            if param not in data:
                return jsonify({
                    'error': f'Missing required parameter: {param}'
                }), 400
        
        # Extract parameters
        origin_lat = float(data['origin_lat'])
        origin_lng = float(data['origin_lng'])
        dest_lat = float(data['dest_lat'])
        dest_lng = float(data['dest_lng'])
        user_disruptions = data.get('user_disruptions', [])
        tau_threshold = float(data.get('tau_threshold', 0.5))
        
        logger.info(f"Optimizing route from ({origin_lat}, {origin_lng}) to ({dest_lat}, {dest_lng})")
        logger.info(f"User disruptions: {user_disruptions}")
        logger.info(f"Tau threshold: {tau_threshold}")
        
        # Get HC2L wrapper and calculate optimal route
        wrapper = get_hc2l_wrapper()
        hc2l_result = wrapper.calculate_optimal_route(
            origin_lat, origin_lng,
            dest_lat, dest_lng,
            user_disruptions=user_disruptions,
            tau_threshold=tau_threshold
        )
        
        # Also get Google Maps route for comparison (if available)
        google_result = None
        try:
            google_result = get_google_directions(
                f"{origin_lat},{origin_lng}",
                f"{dest_lat},{dest_lng}",
                "driving"
            )
        except Exception as e:
            logger.warning(f"Failed to get Google Maps route: {e}")
        
        if hc2l_result.get('success'):
            response_data = {
                'success': True,
                'route_optimization': hc2l_result,
                'google_route': google_result,
                'message': 'Route optimization completed successfully',
                'algorithm_used': 'Dynamic-HC2L',
                'coordinates': {
                    'origin': {'lat': origin_lat, 'lng': origin_lng},
                    'destination': {'lat': dest_lat, 'lng': dest_lng}
                }
            }
            
            # Add comparison data if Google route is available
            if google_result and 'distance' in google_result:
                try:
                    # Extract numeric distance from Google (e.g., "5.2 km" -> 5.2)
                    google_distance_str = google_result['distance'].replace(' km', '').replace(' mi', '')
                    google_distance = float(google_distance_str)
                    
                    hc2l_distance = hc2l_result.get('disrupted_distance') or hc2l_result.get('base_distance')
                    if hc2l_distance:
                        response_data['route_comparison'] = {
                            'hc2l_distance': hc2l_distance,
                            'google_distance': google_distance,
                            'difference': abs(hc2l_distance - google_distance),
                            'hc2l_advantage': hc2l_distance < google_distance
                        }
                except (ValueError, TypeError):
                    logger.warning("Failed to parse Google distance for comparison")
            
            return jsonify(response_data)
        else:
            return jsonify({
                'success': False,
                'error': hc2l_result.get('error', 'Unknown error occurred'),
                'details': hc2l_result,
                'google_route': google_result  # Still provide Google route as fallback
            }), 500
            
    except ValueError as e:
        return jsonify({
            'error': f'Invalid parameter format: {str(e)}'
        }), 400
    except Exception as e:
        logger.error(f"Error in route optimization: {str(e)}")
        return jsonify({
            'error': f'Route optimization failed: {str(e)}'
        }), 500


@app.route('/optimize-route-dhl', methods=['POST'])
def optimize_route_dhl():
    """
    Optimize route using DHL algorithm.
    
    Expected JSON payload:
    {
        "origin_lat": 14.6760,
        "origin_lng": 121.0437,
        "dest_lat": 14.6900,
        "dest_lng": 121.0500
    }
    """
    try:
        data = request.get_json()
        
        if not data:
            return jsonify({
                'error': 'No JSON data provided'
            }), 400
        
        # Validate required parameters
        required_params = ['origin_lat', 'origin_lng', 'dest_lat', 'dest_lng']
        for param in required_params:
            if param not in data:
                return jsonify({
                    'error': f'Missing required parameter: {param}'
                }), 400
        
        # Extract parameters
        origin_lat = float(data['origin_lat'])
        origin_lng = float(data['origin_lng'])
        dest_lat = float(data['dest_lat'])
        dest_lng = float(data['dest_lng'])
        
        logger.info(f"Running DHL algorithm from ({origin_lat}, {origin_lng}) to ({dest_lat}, {dest_lng})")
        
        # Run DHL algorithm
        dhl_result = run_dhl_algorithm(origin_lat, origin_lng, dest_lat, dest_lng)
        
        if 'error' not in dhl_result:
            return jsonify({
                'success': True,
                'dhl_optimization': dhl_result,
                'message': 'DHL route optimization completed successfully',
                'algorithm_used': 'DHL',
                'coordinates': {
                    'origin': {'lat': origin_lat, 'lng': origin_lng},
                    'destination': {'lat': dest_lat, 'lng': dest_lng}
                }
            })
        else:
            return jsonify({
                'success': False,
                'error': dhl_result.get('error', 'Unknown error occurred'),
                'dhl_optimization': dhl_result
            }), 500
            
    except ValueError as e:
        return jsonify({
            'error': f'Invalid parameter format: {str(e)}'
        }), 400
    except Exception as e:
        logger.error(f"Error in DHL route optimization: {str(e)}")
        return jsonify({
            'error': f'DHL route optimization failed: {str(e)}'
        }), 500


@app.route('/algorithm-info', methods=['GET'])
def get_algorithm_info():
    """Get information about the Dynamic-HC2L algorithm."""
    try:
        wrapper = get_hc2l_wrapper()
        info = wrapper.get_algorithm_info()
        return jsonify({
            'success': True,
            'algorithm_info': info
        })
    except Exception as e:
        return jsonify({
            'success': False,
            'error': f'Failed to get algorithm info: {str(e)}'
        }), 500


@app.route('/disruption', methods=['POST'])
def report_disruption():
    """
    Handle disruption reports from the navigation system.
    
    Expected JSON payload:
    {
        "location": "disruption location",
        "type": "disruption type",
        "traffic_level": "traffic level",
        "timestamp": "ISO timestamp"
    }
    """
    try:
        data = request.get_json()
        
        if not data or 'location' not in data or 'type' not in data:
            return jsonify({
                'error': 'Missing required parameters: location and type'
            }), 400
        
        # Log the disruption report (in a real app, you'd save to database)
        disruption_data = {
            'location': data['location'],
            'type': data['type'],
            'traffic_level': data.get('traffic_level', 'unknown'),
            'timestamp': data.get('timestamp', 'unknown'),
            'status': 'received'
        }
        
        print(f"Disruption reported: {disruption_data}")
        
        return jsonify({
            'success': True,
            'message': 'Disruption report received successfully',
            'data': disruption_data
        })
        
    except Exception as e:
        return jsonify({
            'error': f'An error occurred: {str(e)}'
        }), 500


@app.route('/health')
def health_check():
    """Simple health check endpoint."""
    return jsonify({
        'status': 'healthy',
        'api_key_configured': GOOGLE_MAPS_API_KEY != 'your-google-maps-api-key-here'
    })


if __name__ == '__main__':
    print("Starting Flask application...")
    print(f"Google Maps API Key configured: {GOOGLE_MAPS_API_KEY != 'your-google-maps-api-key-here'}")
    app.run(debug=True, host='0.0.0.0', port=5000)