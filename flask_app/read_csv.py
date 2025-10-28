import json
import os

# Fix the path - missing leading slash
csv_path = '/home/lance/Documents/commissions/LazyHC2L/flask_app/static/js/disruption_data.json'

def read_csv(file_path):
    """
    Read JSON data from file and return as list
    """
    data = []
    
    # Check if file exists
    if not os.path.exists(file_path):
        print(f"Error: File not found: {file_path}")
        return data
    
    try:
        with open(file_path, 'r') as file:
            json_data = json.load(file)
            
            # Handle different JSON structures
            if isinstance(json_data, list):
                data = json_data
            elif isinstance(json_data, dict) and 'disruptions' in json_data:
                data = json_data['disruptions']
            else:
                data = [json_data]  # Single object
                
        print(f"Successfully read {len(data)} entries from {file_path}")
        return data
        
    except json.JSONDecodeError as e:
        print(f"Error: Invalid JSON format: {e}")
        return data
    except Exception as e:
        print(f"Error reading file: {e}")
        return data

def display_data(data, limit=5):
    """
    Display first few entries of the data
    """
    print(f"\nDisplaying first {min(limit, len(data))} entries:")
    print("-" * 50)
    
    for i, entry in enumerate(data[:limit]):
        print(f"Entry {i+1}:")
        if isinstance(entry, dict):
            for key, value in entry.items():
                print(f"  {key}: {value}")
        else:
            print(f"  {entry}")
        print()

if __name__ == "__main__":
    # Read the data
    data = read_csv(csv_path)
    
    # Display results
    if data:
        display_data(data)
        
        # Show some statistics
        print(f"Total entries: {len(data)}")
        if data and isinstance(data[0], dict):
            print(f"Available fields: {list(data[0].keys())}")
    else:
        print("No data found or error occurred.")







