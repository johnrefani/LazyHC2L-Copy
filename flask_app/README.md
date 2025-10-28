### Required C++ Executables
The Flask app interfaces with compiled C++ algorithms. Ensure these are built:

1. **Dynamic-HC2L**: `../core/hc2l_dynamic/build/main_dynamic`
2. **DHL Algorithm**: `../DHL/build/test_qc_dhl` or `../DHL/test_qc_dhl`

### Data Files
- `../data/processed/qc_from_csv.gr` - Main graph file
- `../data/processed/node_mapping.csv` - Node coordinate mapping
- `./static/js/disruption_data.json` - Traffic disruption data

## 🛠 Installation

### 2. Install Python Dependencies
```bash
# Create virtual environment (recommended)
python -m venv venv
venv/bin/activate 

# Install required packages (rename the requirements.md to requirements.txt)
pip install -r requirements.txt
```

### 2. Navigate to the App
```bash
cd /path/to/LazyHC2L/flask_app
```

### 3. Build Required C++ Components

#### Build Dynamic-HC2L
```bash
cd path/to/core/hc2l_dynamic
mkdir -p build && cd build
cmake .. && cmake --build .
# Verify: ls -la main_dynamic
```

#### Build DHL Algorithm (using cmake)
```bash
cd path/to/DHL
mkdir -p build && cd build
cmake .. && cmake --build .
# Verify: ls -la main_dynamic
```

#### Build DHL Algorithm (using make)
```bash
cd ../../../DHL
make clean && make all
# Verify: ls -la test_qc_dhl
```

### 4. Verify Data Files
Ensure these files exist:
```bash
ls -la ../data/processed/qc_from_csv.gr
ls -la ../data/processed/node_mapping.csv
ls -la ./static/js/disruption_data.json
```

## 🚀 Running the Application

### Development Mode
```bash
python app.py
```

The application will be available at: `http://localhost:5000`

## 📡 API Endpoints

### 1. Navigation Interface
- **GET** `/` - Main navigation system interface
- **GET** `/health` - Health check and API status

### 2. Route Optimization
- **POST** `/directions` - Get directions (HC2L + Google Maps fallback)
- **POST** `/optimize-route` - Dynamic-HC2L route optimization
- **POST** `/optimize-route-dhl` - DHL algorithm route optimization

### 3. Algorithm Information
- **GET** `/algorithm-info` - Get Dynamic-HC2L algorithm details
- **POST** `/disruption` - Report traffic disruptions
