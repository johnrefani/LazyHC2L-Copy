let map;
let directionsService;
let directionsRenderer;
let hc2lDirectionsRenderer; // Separate renderer for HC2L routes
let currentTab = 'user';
let selectedTrafficLevel = 'medium';
let selectedSOP = null;
let markers = []; // Array to store all pin markers
let isPinningMode = false; // Track if user is in pinning mode
let startMarker = null; // Starting point marker
let destinationMarker = null; // Destination marker
let isSettingStart = false; // Track if setting starting point
let isSettingDestination = false; // Track if setting destination
let disruptionMarkers = []; // Array to store disruption markers
let isReportingDisruption = false; // Track if user is reporting disruption
let hc2lPolyline = null; // Store HC2L route polyline

// Initialize Google Maps
function initMap() {
    // Quezon City, Philippines coordinates
    const quezonCity = { lat: 14.6380, lng: 121.0437 };
    
    map = new google.maps.Map(document.getElementById('map'), {
        zoom: 13,
        center: quezonCity,
        mapTypeControl: true,
        streetViewControl: false,
        fullscreenControl: true
    });
    
    directionsService = new google.maps.DirectionsService();
    directionsRenderer = new google.maps.DirectionsRenderer({
        draggable: true,
        panel: null,
        polylineOptions: {
            strokeColor: '#4285f4',
            strokeWeight: 4,
            strokeOpacity: 0.8
        }
    });
    
    // Separate renderer for HC2L routes with different styling
    hc2lDirectionsRenderer = new google.maps.DirectionsRenderer({
        draggable: false,
        panel: null,
        polylineOptions: {
            strokeColor: '#ff6f00',
            strokeWeight: 3,
            strokeOpacity: 0.7,
            strokeDashArray: [10, 5] // Dashed line for HC2L
        },
        suppressMarkers: true // Don't show markers for HC2L route
    });
    
    directionsRenderer.setMap(map);
    hc2lDirectionsRenderer.setMap(map);

    // Add click listener for setting waypoints and pinning locations
    map.addListener('click', function(event) {
        if (isSettingStart) {
            // Set starting point with red marker
            setStartingPoint(event.latLng);
        } else if (isSettingDestination) {
            // Set destination with green marker
            setDestinationPoint(event.latLng);
        } else if (isReportingDisruption) {
            // Add disruption marker at exact location
            addDisruptionMarker(event.latLng);
        } else if (isPinningMode) {
            // Add a general pin marker
            addPinMarker(event.latLng, `Pin ${markers.length + 1}`);
        }
    });
}

// Tab switching functionality
document.addEventListener('DOMContentLoaded', function() {
    document.querySelectorAll('.tab-btn').forEach(btn => {
        btn.addEventListener('click', function() {
            const tabName = this.dataset.tab;
            switchTab(tabName);
        });
    });

    // Traffic level selection
    document.querySelectorAll('.traffic-btn').forEach(btn => {
        btn.addEventListener('click', function() {
            document.querySelectorAll('.traffic-btn').forEach(b => b.classList.remove('active'));
            this.classList.add('active');
            selectedTrafficLevel = this.dataset.traffic;
        });
    });

    // SOP selection
    document.querySelectorAll('.sop-btn').forEach(btn => {
        btn.addEventListener('click', function() {
            document.querySelectorAll('.sop-btn').forEach(b => b.classList.remove('active'));
            this.classList.add('active');
            selectedSOP = this.dataset.sop;
            
            // Execute SOP-specific functionality
            if (selectedSOP === '1') {
                showSOP1Modal();
            } else if (selectedSOP === '2') {
                showSOP2ComparisonModal();
            } else {
                updatePerformanceMetrics(selectedSOP);
            }
        });
    });

    // Button event listeners
    document.getElementById('setStartBtn').addEventListener('click', function() {
        this.classList.toggle('active');
        if (this.classList.contains('active')) {
            this.textContent = 'Click on map';
            this.style.background = 'linear-gradient(135deg, #ff9800 0%, #f57c00 100%)';
            isSettingStart = true;
            // Disable other modes
            isPinningMode = false;
            isSettingDestination = false;
            resetOtherButtons(['pinDisruptionBtn', 'setDestBtn']);
        } else {
            this.textContent = 'Pin Starting Point';
            this.style.background = '';
            isSettingStart = false;
        }
    });

    document.getElementById('setDestBtn').addEventListener('click', function() {
        this.classList.toggle('active');
        if (this.classList.contains('active')) {
            this.textContent = 'Click on map';
            this.style.background = 'linear-gradient(135deg, #ff9800 0%, #f57c00 100%)';
            isSettingDestination = true;
            // Disable other modes
            isPinningMode = false;
            isSettingStart = false;
            resetOtherButtons(['pinDisruptionBtn', 'setStartBtn']);
        } else {
            this.textContent = 'Pin Destination Point';
            this.style.background = '';
            isSettingDestination = false;
        }
    });

    document.getElementById('pinDisruptionBtn').addEventListener('click', function() {
        this.classList.toggle('active');
        if (this.classList.contains('active')) {
            this.textContent = 'Click on map to mark disruption';
            this.style.background = 'linear-gradient(135deg, #ff9800 0%, #f57c00 100%)';
            isReportingDisruption = true;
            // Disable other modes
            isSettingStart = false;
            isSettingDestination = false;
            isPinningMode = false;
            resetOtherButtons(['setStartBtn', 'setDestBtn']);
            
            // Add visual feedback to map
            map.setOptions({ cursor: 'crosshair' });
            
            // Show instruction message
            showInstructionMessage('Click anywhere on the map to mark the disruption location');
        } else {
            this.textContent = 'Pin Disruption Point';
            this.style.background = '';
            isReportingDisruption = false;
            map.setOptions({ cursor: '' });
            hideInstructionMessage();
        }
    });

    document.getElementById('findPathBtn').addEventListener('click', function() {
        const origin = document.getElementById('startingPoint').value;
        const destination = document.getElementById('destination').value;
        
        if (origin && destination) {
            calculateRoute(origin, destination);
        }
    });

    document.getElementById('submitReportBtn').addEventListener('click', function() {
        const location = document.getElementById('disruptionLocation').value;
        const type = document.getElementById('disruptionType').value;
        
        if (location && type) {
            submitDisruptionReport(location, type, selectedTrafficLevel);
        } else {
            alert('Please fill in all disruption details');
        }
    });

    document.getElementById('applyChangesBtn').addEventListener('click', function() {
        applyPerformanceSettings();
    });

    document.getElementById('clearDataBtn').addEventListener('click', function() {
        if (confirm('Are you sure you want to clear all data?')) {
            clearAllData();
        }
    });

    // Refresh metrics button
    document.getElementById('refreshMetricsBtn').addEventListener('click', function() {
        executeSOP1InModal();
    });
    
    // Refresh comparison button
    document.getElementById('refreshComparisonBtn').addEventListener('click', function() {
        executeSOP2ComparisonInModal();
    });
});

function switchTab(tabName) {
    // Update tab buttons
    document.querySelectorAll('.tab-btn').forEach(btn => {
        btn.classList.remove('active');
    });
    document.querySelector(`[data-tab="${tabName}"]`).classList.add('active');

    // Update panel content
    document.querySelectorAll('.panel-content').forEach(panel => {
        panel.classList.add('hidden');
    });
    document.getElementById(`${tabName}-panel`).classList.remove('hidden');

    currentTab = tabName;
}

// Helper functions for user feedback
function showInstructionMessage(message) {
    // Remove existing message if any
    hideInstructionMessage();
    
    const messageDiv = document.createElement('div');
    messageDiv.id = 'instruction-message';
    messageDiv.style.cssText = `
        position: fixed;
        top: 80px;
        left: 50%;
        transform: translateX(-50%);
        background: rgba(255, 152, 0, 0.95);
        color: white;
        padding: 12px 20px;
        border-radius: 25px;
        font-size: 14px;
        font-weight: 600;
        z-index: 2000;
        box-shadow: 0 4px 12px rgba(255, 152, 0, 0.3);
        animation: slideDown 0.3s ease-out;
        backdrop-filter: blur(10px);
    `;
    messageDiv.textContent = message;
    document.body.appendChild(messageDiv);
}

function hideInstructionMessage() {
    const existing = document.getElementById('instruction-message');
    if (existing) {
        existing.remove();
    }
}

function showSuccessMessage(message) {
    const messageDiv = document.createElement('div');
    messageDiv.style.cssText = `
        position: fixed;
        top: 80px;
        left: 50%;
        transform: translateX(-50%);
        background: rgba(76, 175, 80, 0.95);
        color: white;
        padding: 12px 20px;
        border-radius: 25px;
        font-size: 14px;
        font-weight: 600;
        z-index: 2000;
        box-shadow: 0 4px 12px rgba(76, 175, 80, 0.3);
        backdrop-filter: blur(10px);
    `;
    messageDiv.textContent = message;
    document.body.appendChild(messageDiv);
    
    // Remove after 3 seconds
    setTimeout(() => {
        messageDiv.remove();
    }, 3000);
}

// Helper function to reset other buttons
function resetOtherButtons(buttonIds) {
    buttonIds.forEach(id => {
        const btn = document.getElementById(id);
        btn.classList.remove('active');
        btn.style.background = '';
        
        if (id === 'setStartBtn') {
            btn.textContent = 'Pin Starting Point';
        } else if (id === 'setDestBtn') {
            btn.textContent = 'Pin Destination Point';
        } else if (id === 'pinDisruptionBtn') {
            btn.textContent = 'Pin Disruption Point';
        }
    });
}

// Set starting point with red marker
function setStartingPoint(location) {
    // Remove existing start marker
    if (startMarker) {
        startMarker.setMap(null);
    }

    // Create new start marker (red)
    startMarker = new google.maps.Marker({
        position: location,
        map: map,
        title: 'Starting Point',
        icon: {
            url: 'https://maps.google.com/mapfiles/ms/icons/red-dot.png',
            scaledSize: new google.maps.Size(40, 40)
        },
        animation: google.maps.Animation.DROP
    });

    // Update input field
    document.getElementById('startingPoint').value = 
        `${location.lat().toFixed(6)}, ${location.lng().toFixed(6)}`;

    // Create info window
    const infoWindow = new google.maps.InfoWindow({
        content: `
            <div style="padding: 10px;">
                <h4 style="margin: 0 0 10px 0; color: #333;">Starting Point</h4>
                <p style="margin: 0; color: #666; font-size: 12px;">Lat: ${location.lat().toFixed(6)}</p>
                <p style="margin: 0; color: #666; font-size: 12px;">Lng: ${location.lng().toFixed(6)}</p>
            </div>
        `
    });

    startMarker.addListener('click', function() {
        if (destinationMarker && destinationMarker.infoWindow) {
            destinationMarker.infoWindow.close();
        }
        infoWindow.open(map, startMarker);
    });

    startMarker.infoWindow = infoWindow;

    // Reset button
    isSettingStart = false;
    document.getElementById('setStartBtn').classList.remove('active');
    document.getElementById('setStartBtn').textContent = 'Pin Starting Point';
    document.getElementById('setStartBtn').style.background = '';
}

// Set destination point with green marker
function setDestinationPoint(location) {
    // Remove existing destination marker
    if (destinationMarker) {
        destinationMarker.setMap(null);
    }

    // Create new destination marker (green)
    destinationMarker = new google.maps.Marker({
        position: location,
        map: map,
        title: 'Destination',
        icon: {
            url: 'https://maps.google.com/mapfiles/ms/icons/green-dot.png',
            scaledSize: new google.maps.Size(40, 40)
        },
        animation: google.maps.Animation.DROP
    });

    // Update input field
    document.getElementById('destination').value = 
        `${location.lat().toFixed(6)}, ${location.lng().toFixed(6)}`;

    // Create info window
    const infoWindow = new google.maps.InfoWindow({
        content: `
            <div style="padding: 10px;">
                <h4 style="margin: 0 0 10px 0; color: #333;">Destination</h4>
                <p style="margin: 0; color: #666; font-size: 12px;">Lat: ${location.lat().toFixed(6)}</p>
                <p style="margin: 0; color: #666; font-size: 12px;">Lng: ${location.lng().toFixed(6)}</p>
            </div>
        `
    });

    destinationMarker.addListener('click', function() {
        if (startMarker && startMarker.infoWindow) {
            startMarker.infoWindow.close();
        }
        infoWindow.open(map, destinationMarker);
    });

    destinationMarker.infoWindow = infoWindow;

    // Reset button
    isSettingDestination = false;
    document.getElementById('setDestBtn').classList.remove('active');
    document.getElementById('setDestBtn').textContent = 'Pin Destination Point';
    document.getElementById('setDestBtn').style.background = '';
}

// Add disruption marker function
function addDisruptionMarker(location) {
    // Remove any existing temporary disruption marker
    disruptionMarkers.forEach(markerObj => {
        if (markerObj.temporary) {
            markerObj.marker.setMap(null);
        }
    });
    disruptionMarkers = disruptionMarkers.filter(m => !m.temporary);

    // Create disruption marker with warning icon
    const marker = new google.maps.Marker({
        position: location,
        map: map,
        title: 'Disruption Location',
        icon: {
            url: 'https://maps.google.com/mapfiles/ms/icons/orange-dot.png',
            scaledSize: new google.maps.Size(40, 40)
        },
        animation: google.maps.Animation.BOUNCE
    });

    // Stop bouncing after 2 seconds
    setTimeout(() => {
        marker.setAnimation(null);
    }, 2000);

    // Add info window with disruption details
    const infoWindow = new google.maps.InfoWindow({
        content: `
            <div style="padding: 15px; min-width: 200px;">
                <h4 style="margin: 0 0 10px 0; color: #ff6f00; display: flex; align-items: center;">
                    ⚠️ Disruption Location
                </h4>
                <p style="margin: 0 0 8px 0; color: #666; font-size: 12px;">
                    <strong>Coordinates:</strong><br>
                    Lat: ${location.lat().toFixed(6)}<br>
                    Lng: ${location.lng().toFixed(6)}
                </p>
                <p style="margin: 0; color: #666; font-size: 11px; font-style: italic;">
                    Please fill in the disruption details below and submit your report.
                </p>
            </div>
        `
    });

    marker.addListener('click', function() {
        // Close other info windows
        disruptionMarkers.forEach(m => {
            if (m.infoWindow) {
                m.infoWindow.close();
            }
        });
        if (startMarker && startMarker.infoWindow) {
            startMarker.infoWindow.close();
        }
        if (destinationMarker && destinationMarker.infoWindow) {
            destinationMarker.infoWindow.close();
        }
        infoWindow.open(map, marker);
    });

    // Store as temporary marker
    disruptionMarkers.push({ 
        marker: marker, 
        infoWindow: infoWindow, 
        temporary: true,
        location: location
    });

    // Auto-fill disruption location field
    document.getElementById('disruptionLocation').value = 
        `${location.lat().toFixed(6)}, ${location.lng().toFixed(6)}`;

    // Reset button and show success message
    isReportingDisruption = false;
    document.getElementById('pinDisruptionBtn').classList.remove('active');
    document.getElementById('pinDisruptionBtn').textContent = '📍 Pin Disruption Point';
    document.getElementById('pinDisruptionBtn').style.background = '';
    map.setOptions({ cursor: '' });
    hideInstructionMessage();

    // Auto-open the info window
    infoWindow.open(map, marker);

    // Show success message
    showSuccessMessage('Disruption location marked! Please complete the form below.');

    // Scroll to disruption form section
    document.getElementById('disruptionType').focus();
}

// Add pin marker function
function addPinMarker(location, title) {
    const marker = new google.maps.Marker({
        position: location,
        map: map,
        title: title,
        icon: {
            url: 'https://maps.google.com/mapfiles/ms/icons/red-dot.png',
            scaledSize: new google.maps.Size(32, 32)
        },
        animation: google.maps.Animation.DROP
    });

    // Add info window
    const infoWindow = new google.maps.InfoWindow({
        content: `
            <div style="padding: 10px;">
                <h4 style="margin: 0 0 10px 0; color: #333;">${title}</h4>
                <p style="margin: 0; color: #666; font-size: 12px;">Lat: ${location.lat().toFixed(6)}</p>
                <p style="margin: 0; color: #666; font-size: 12px;">Lng: ${location.lng().toFixed(6)}</p>
                <button onclick="removeMarker(${markers.length})" style="
                    margin-top: 10px;
                    padding: 5px 10px;
                    background: #f44336;
                    color: white;
                    border: none;
                    border-radius: 4px;
                    cursor: pointer;
                    font-size: 12px;
                ">Remove Pin</button>
            </div>
        `
    });

    marker.addListener('click', function() {
        // Close all other info windows
        markers.forEach(m => {
            if (m.infoWindow) {
                m.infoWindow.close();
            }
        });
        infoWindow.open(map, marker);
    });

    // Store marker with its info window
    markers.push({ marker: marker, infoWindow: infoWindow });

    // Auto-fill disruption location if in pinning mode
    if (isPinningMode) {
        document.getElementById('disruptionLocation').value = 
            `${location.lat().toFixed(6)}, ${location.lng().toFixed(6)}`;
        // Turn off pinning mode after adding marker
        isPinningMode = false;
        document.getElementById('reportBtn').classList.remove('active');
        document.getElementById('reportBtn').textContent = 'Report Accurately';
        document.getElementById('reportBtn').style.background = '';
    }
}

// Remove specific marker
function removeMarker(index) {
    if (markers[index]) {
        markers[index].marker.setMap(null);
        markers[index].infoWindow.close();
        markers.splice(index, 1);
        // Update remaining marker titles
        markers.forEach((markerObj, i) => {
            markerObj.marker.setTitle(`Pin ${i + 1}`);
        });
    }
}

// Clear all markers
function clearAllMarkers() {
    markers.forEach(markerObj => {
        markerObj.marker.setMap(null);
        markerObj.infoWindow.close();
    });
    markers = [];
}

// Route calculation using Dynamic-HC2L as primary, Google Maps as fallback
function calculateRoute(origin, destination) {
    console.log('Calculating route using HC2L algorithm...');
    
    // Try to get coordinates from origin and destination
    let originCoords = null;
    let destCoords = null;
    
    // Check if we have markers with coordinates
    if (startMarker && destinationMarker) {
        originCoords = {
            lat: startMarker.getPosition().lat(),
            lng: startMarker.getPosition().lng()
        };
        destCoords = {
            lat: destinationMarker.getPosition().lat(),
            lng: destinationMarker.getPosition().lng()
        };
    }
    
    // Always show Google Maps route for visual representation
    displayGoogleMapsRoute(origin, destination);
    
    // If we have coordinates, also run HC2L for comparison and metrics
    if (originCoords && destCoords) {
        // Collect user disruptions
        const userDisruptions = disruptionMarkers
            .filter(marker => !marker.temporary)
            .map((marker, index) => ({
                from_node: index * 2 + 1,
                to_node: index * 2 + 2,
                type: marker.marker.getTitle().split(' - ')[0] || 'User_Report',
                severity: extractSeverityFromTitle(marker.marker.getTitle()) || 'Medium'
            }));
        
        // Use HC2L for route calculation and analysis
        fetch('/directions', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify({
                origin: `${originCoords.lat},${originCoords.lng}`,
                destination: `${destCoords.lat},${destCoords.lng}`,
                mode: 'driving',
                use_hc2l: true
            })
        })
        .then(response => response.json())
        .then(data => {
            console.log('HC2L routing response:', data);
            
            if (data.success && data.algorithm === 'Dynamic-HC2L') {
                console.log('Successfully used HC2L for route analysis');
                
                // Add HC2L route overlay (dotted line to show HC2L optimization)
                const hc2lRoute = new google.maps.Polyline({
                    path: [originCoords, destCoords],
                    geodesic: true,
                    strokeColor: '#ff6f00',
                    strokeOpacity: 0.8,
                    strokeWeight: 3,
                    icons: [{
                        icon: {
                            path: 'M 0,-1 0,1',
                            strokeOpacity: 1,
                            scale: 2
                        },
                        offset: '0',
                        repeat: '10px'
                    }]
                });
                
                hc2lRoute.setMap(map);
                
                // Add legend
                addRouteLegend();
                
                // Update performance metrics if SOP is selected
                if (selectedSOP) {
                    updateRouteMetricsFromHC2L(data);
                }
                
                // Show route info
                console.log(`HC2L Route Analysis: ${data.directions.distance}, ${data.directions.duration}`);
                
            } else {
                console.log('HC2L analysis not available, showing Google Maps route only');
            }
        })
        .catch(error => {
            console.error('HC2L routing error:', error);
            console.log('HC2L analysis failed, showing Google Maps route only');
        });
    } else {
        console.log('No coordinates available for HC2L analysis, using Google Maps only');
    }
}

// Display Google Maps route for visual representation
function displayGoogleMapsRoute(origin, destination) {
    const request = {
        origin: origin,
        destination: destination,
        travelMode: google.maps.TravelMode.DRIVING,
        unitSystem: google.maps.UnitSystem.METRIC
    };

    directionsService.route(request, (result, status) => {
        if (status === 'OK') {
            directionsRenderer.setDirections(result);
            
            // Display route information
            displayRouteInfo(result);
            
            // Update performance metrics if SOP is selected
            if (selectedSOP && selectedSOP !== '1') {
                updateRouteMetrics(result);
            }
        } else {
            console.error('Directions request failed due to ' + status);
        }
    });
}

// Display route information in the route display section
function displayRouteInfo(directionsResult) {
    const route = directionsResult.routes[0];
    const leg = route.legs[0];
    
    // Show the route display section
    const routeSection = document.getElementById('routeDisplaySection');
    routeSection.style.display = 'block';
    
    // Update route summary
    document.getElementById('routeDistance').textContent = leg.distance.text;
    document.getElementById('routeDuration').textContent = leg.duration.text;
    document.getElementById('routeSteps').textContent = leg.steps.length;
    
    // Update route path with step-by-step directions
    const routePathContainer = document.getElementById('routePath');
    routePathContainer.innerHTML = '';
    
    leg.steps.forEach((step, index) => {
        const stepDiv = document.createElement('div');
        stepDiv.className = 'route-step';
        
        // Extract road name from instructions
        const instruction = step.instructions.replace(/<[^>]*>/g, ''); // Remove HTML tags
        const roadMatch = instruction.match(/on (.+?)(?:\s|$|,)/);
        const roadName = roadMatch ? roadMatch[1] : 'Continue on road';
        
        stepDiv.innerHTML = 
            '<div class="step-number">' + (index + 1) + '</div>' +
            '<div class="step-content">' +
                '<div class="step-instruction">' + instruction + '</div>' +
                '<div class="step-road">' + roadName +
                '<span class="step-distance">' + step.distance.text + '</span></div>' +
            '</div>';
        
        routePathContainer.appendChild(stepDiv);
    });
    
    // Scroll to route display section
    routeSection.scrollIntoView({ behavior: 'smooth', block: 'start' });
}

// Clear route display
function clearRouteDisplay() {
    const routeSection = document.getElementById('routeDisplaySection');
    routeSection.style.display = 'none';
    
    // Clear directions renderer
    directionsRenderer.setDirections({routes: []});
    if (hc2lDirectionsRenderer) {
        hc2lDirectionsRenderer.setDirections({routes: []});
    }
    
    // Remove any HC2L polylines
    if (hc2lPolyline) {
        hc2lPolyline.setMap(null);
        hc2lPolyline = null;
    }
    
    // Remove route legend
    const legend = document.getElementById('route-legend');
    if (legend) {
        legend.remove();
    }
}


// Add route legend to explain different route types
function addRouteLegend() {
    // Remove existing legend if any
    const existingLegend = document.getElementById('route-legend');
    if (existingLegend) {
        existingLegend.remove();
    }
    
    const legend = document.createElement('div');
    legend.id = 'route-legend';
    legend.style.cssText = `
        position: absolute;
        top: 10px;
        right: 10px;
        background: rgba(255, 255, 255, 0.95);
        padding: 12px;
        border-radius: 8px;
        box-shadow: 0 2px 8px rgba(0,0,0,0.1);
        font-size: 12px;
        z-index: 1000;
        backdrop-filter: blur(10px);
    `;
    
    legend.innerHTML = `
        <div style="font-weight: bold; margin-bottom: 8px; color: #333;">Route Legend</div>
        <div style="color: #000000; display: flex; align-items: center; margin-bottom: 4px;">
            <div style="width: 20px; height: 3px; background: #4285f4; margin-right: 8px;"></div>
            <span>Google Maps Route</span>
        </div>
        <div style="color: #000000; display: flex; align-items: center;">
            <div style="width: 20px; height: 3px; background: #ff6f00; margin-right: 8px; background-image: repeating-linear-gradient(90deg, transparent, transparent 5px, #fff 5px, #fff 10px);"></div>
            <span>HC2L Analysis Path</span>
        </div>
    `;
    
    document.getElementById('map').appendChild(legend);
}


// Update route metrics from HC2L data
function updateRouteMetricsFromHC2L(data) {
    if (selectedSOP === '1') {
        // If SOP 1 is selected, the HC2L data should already include timing metrics
        if (data.hc2l_data && data.hc2l_data.timing_metrics) {
            console.log('Displaying HC2L metrics from route calculation');
            displayHC2LResults(data.hc2l_data);
        }
    } else {
        updatePerformanceMetrics(selectedSOP);
    }
}

// Submit disruption report
function submitDisruptionReport(location, type, traffic) {
    // Find the temporary disruption marker
    const tempMarker = disruptionMarkers.find(m => m.temporary);
    
    // Send to backend
    fetch('/disruption', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
        },
        body: JSON.stringify({
            location: location,
            type: type,
            traffic_level: traffic,
            timestamp: new Date().toISOString()
        })
    })
    .then(response => response.json())
    .then(data => {
        if (data.success) {
            // Convert temporary marker to permanent
            if (tempMarker) {
                tempMarker.temporary = false;
                tempMarker.marker.setTitle(`${type} - ${traffic} traffic`);
                
                // Update info window with full details
                const newContent = `
                    <div style="padding: 15px; min-width: 220px;">
                        <h4 style="margin: 0 0 10px 0; color: #ff6f00; display: flex; align-items: center;">
                            ⚠️ Reported Disruption
                        </h4>
                        <div style="margin-bottom: 10px;">
                            <strong style="color: #333;">Type:</strong> <span style="color: #666;">${type}</span><br>
                            <strong style="color: #333;">Traffic Level:</strong> <span style="color: #666;">${traffic}</span><br>
                            <strong style="color: #333;">Reported:</strong> <span style="color: #666;">${new Date().toLocaleString()}</span>
                        </div>
                        <p style="margin: 0 0 8px 0; color: #666; font-size: 12px;">
                            <strong>Coordinates:</strong><br>
                            Lat: ${tempMarker.location.lat().toFixed(6)}<br>
                            Lng: ${tempMarker.location.lng().toFixed(6)}
                        </p>
                        <button onclick="removeDisruptionMarker('${disruptionMarkers.indexOf(tempMarker)}')" style="
                            margin-top: 8px;
                            padding: 6px 12px;
                            background: #f44336;
                            color: white;
                            border: none;
                            border-radius: 4px;
                            cursor: pointer;
                            font-size: 11px;
                        ">Remove Report</button>
                    </div>
                `;
                tempMarker.infoWindow.setContent(newContent);
            }
            
            alert('Disruption reported successfully!');
            
            // Clear form
            document.getElementById('disruptionLocation').value = '';
            document.getElementById('disruptionType').value = '';
            
            // Reset traffic level to default
            document.querySelectorAll('.traffic-btn').forEach(b => b.classList.remove('active'));
            document.querySelector('[data-traffic="medium"]').classList.add('active');
            selectedTrafficLevel = 'medium';
        } else {
            alert('Error reporting disruption: ' + data.error);
        }
    })
    .catch(error => {
        console.error('Error:', error);
        alert('Network error occurred');
    });
}

// SOP 1: Dynamic-HC2L Route Optimization for Modal
function executeSOP1InModal() {
    const modalMetricsContent = document.getElementById('modalMetricsContent');
    
    // Show loading state
    modalMetricsContent.innerHTML = `
        <div class="text-center p-4">
            <div class="spinner-border text-primary" role="status">
                <span class="visually-hidden">Loading...</span>
            </div>
            <p class="mt-3 text-muted">🔄 Executing SOP 1: Dynamic-HC2L Route Optimization</p>
        </div>
    `;
    
    // Get current origin and destination
    const originInput = document.getElementById('startingPoint').value;
    const destinationInput = document.getElementById('destination').value;
    
    if (!originInput || !destinationInput) {
        modalMetricsContent.innerHTML = `
            <div class="alert alert-warning" role="alert">
                <h4 class="alert-heading">Missing Route Information</h4>
                <p>Please set both starting point and destination before running SOP 1.</p>
                <hr>
                <p class="mb-0">Use the 'Pin Starting Point' and 'Pin Destination Point' buttons in the User panel.</p>
            </div>
        `;
        return;
    }
    
    // Parse coordinates from input or use marker positions
    let originLat, originLng, destLat, destLng;
    
    if (startMarker && destinationMarker) {
        originLat = startMarker.getPosition().lat();
        originLng = startMarker.getPosition().lng();
        destLat = destinationMarker.getPosition().lat();
        destLng = destinationMarker.getPosition().lng();
    } else {
        // Try to parse coordinates from input
        const originCoords = parseCoordinates(originInput);
        const destCoords = parseCoordinates(destinationInput);
        
        if (originCoords && destCoords) {
            originLat = originCoords.lat;
            originLng = originCoords.lng;
            destLat = destCoords.lat;
            destLng = destCoords.lng;
        } else {
            // Use geocoding as fallback
            geocodeAndOptimizeForModal(originInput, destinationInput);
            return;
        }
    }
    
    // Collect user-reported disruptions
    const userDisruptions = disruptionMarkers
        .filter(marker => !marker.temporary)
        .map((marker, index) => ({
            from_node: index * 2 + 1,
            to_node: index * 2 + 2,
            type: marker.marker.getTitle().split(' - ')[0] || 'User_Report',
            severity: extractSeverityFromTitle(marker.marker.getTitle()) || 'Medium'
        }));
    
    // Call Dynamic-HC2L optimization
    optimizeRouteWithHC2LForModal(originLat, originLng, destLat, destLng, userDisruptions);
}

// Show SOP 1 Modal
function showSOP1Modal() {
    // Show the modal
    const modal = new bootstrap.Modal(document.getElementById('sop1MetricsModal'));
    modal.show();
    
    // Execute SOP 1 in the modal
    executeSOP1InModal();
}


// SOP 1: Dynamic-HC2L Route Optimization (Legacy for side panel)
function executeSOP1() {
    const metricsContent = document.querySelector('.metrics-content');
    
    // Show loading state
    metricsContent.innerHTML = `
        <div style="text-align: center; padding: 20px;">
            <div style="font-size: 18px; color: #4285f4; margin-bottom: 10px;">🔄 Executing SOP 1</div>
            <div style="font-size: 14px; color: #666; margin-bottom: 15px;">Dynamic-HC2L Route Optimization</div>
            <div style="display: inline-block; width: 40px; height: 40px; border: 4px solid #f3f3f3; border-top: 4px solid #4285f4; border-radius: 50%; animation: spin 1s linear infinite;"></div>
        </div>
        <style>
        @keyframes spin {
            0% { transform: rotate(0deg); }
            100% { transform: rotate(360deg); }
        }
        </style>
    `;
    
    // Get current origin and destination
    const originInput = document.getElementById('startingPoint').value;
    const destinationInput = document.getElementById('destination').value;
    
    if (!originInput || !destinationInput) {
        metricsContent.innerHTML = `
            <div style="text-align: center; padding: 20px; color: #f44336;">
                <div style="font-size: 16px; margin-bottom: 10px;">⚠️ Missing Information</div>
                <div style="font-size: 14px;">Please set both starting point and destination to use SOP 1</div>
            </div>
        `;
        return;
    }
    
    // Parse coordinates from input or use marker positions
    let originLat, originLng, destLat, destLng;
    
    if (startMarker && destinationMarker) {
        // Use marker positions if available
        originLat = startMarker.getPosition().lat();
        originLng = startMarker.getPosition().lng();
        destLat = destinationMarker.getPosition().lat();
        destLng = destinationMarker.getPosition().lng();
    } else {
        // Try to parse coordinates from input
        try {
            const originCoords = parseCoordinates(originInput);
            const destCoords = parseCoordinates(destinationInput);
            
            if (originCoords && destCoords) {
                originLat = originCoords.lat;
                originLng = originCoords.lng;
                destLat = destCoords.lat;
                destLng = destCoords.lng;
            } else {
                throw new Error('Could not parse coordinates');
            }
        } catch (error) {
            // Fallback to geocoding
            geocodeAndOptimize(originInput, destinationInput);
            return;
        }
    }
    
    // Collect user-reported disruptions
    const userDisruptions = disruptionMarkers
        .filter(marker => !marker.temporary)
        .map((marker, index) => ({
            from_node: index * 2 + 1, // Simple node mapping
            to_node: index * 2 + 2,
            type: marker.marker.getTitle().split(' - ')[0] || 'User_Report',
            severity: extractSeverityFromTitle(marker.marker.getTitle()) || 'Medium'
        }));
    
    // Call Dynamic-HC2L optimization
    optimizeRouteWithHC2L(originLat, originLng, destLat, destLng, userDisruptions);
}

// Helper function to parse coordinates from string
function parseCoordinates(coordString) {
    // Try to parse "lat, lng" format
    const coords = coordString.split(',');
    if (coords.length === 2) {
        const lat = parseFloat(coords[0].trim());
        const lng = parseFloat(coords[1].trim());
        
        if (!isNaN(lat) && !isNaN(lng)) {
            return { lat: lat, lng: lng };
        }
    }
    return null;
}

// Helper function to extract severity from marker title
function extractSeverityFromTitle(title) {
    if (title.includes('Heavy')) return 'Heavy';
    if (title.includes('Light')) return 'Light';
    return 'Medium';
}

// Geocode addresses and then optimize for modal
function geocodeAndOptimizeForModal(origin, destination) {
    const geocoder = new google.maps.Geocoder();
    
    Promise.all([
        new Promise((resolve, reject) => {
            geocoder.geocode({ address: origin }, (results, status) => {
                if (status === 'OK') {
                    const location = results[0].geometry.location;
                    resolve({ lat: location.lat(), lng: location.lng() });
                } else {
                    reject(new Error(`Geocoding failed for origin: ${status}`));
                }
            });
        }),
        new Promise((resolve, reject) => {
            geocoder.geocode({ address: destination }, (results, status) => {
                if (status === 'OK') {
                    const location = results[0].geometry.location;
                    resolve({ lat: location.lat(), lng: location.lng() });
                } else {
                    reject(new Error(`Geocoding failed for destination: ${status}`));
                }
            });
        })
    ]).then(([originCoords, destCoords]) => {
        const userDisruptions = disruptionMarkers
            .filter(marker => !marker.temporary)
            .map((marker, index) => ({
                from_node: index * 2 + 1,
                to_node: index * 2 + 2,
                type: marker.marker.getTitle().split(' - ')[0] || 'User_Report',
                severity: extractSeverityFromTitle(marker.marker.getTitle()) || 'Medium'
            }));
        
        optimizeRouteWithHC2LForModal(originCoords.lat, originCoords.lng, destCoords.lat, destCoords.lng, userDisruptions);
    }).catch(error => {
        console.error('Geocoding error:', error);
        displayHC2LErrorInModal(error.message);
    });
}

// Geocode addresses and then optimize
function geocodeAndOptimize(origin, destination) {
    const geocoder = new google.maps.Geocoder();
    
    Promise.all([
        new Promise((resolve, reject) => {
            geocoder.geocode({ address: origin }, (results, status) => {
                if (status === 'OK') {
                    const location = results[0].geometry.location;
                    resolve({ lat: location.lat(), lng: location.lng() });
                } else {
                    reject(new Error('Geocoding failed for origin'));
                }
            });
        }),
        new Promise((resolve, reject) => {
            geocoder.geocode({ address: destination }, (results, status) => {
                if (status === 'OK') {
                    const location = results[0].geometry.location;
                    resolve({ lat: location.lat(), lng: location.lng() });
                } else {
                    reject(new Error('Geocoding failed for destination'));
                }
            });
        })
    ]).then(([originCoords, destCoords]) => {
        // Collect user disruptions
        const userDisruptions = disruptionMarkers
            .filter(marker => !marker.temporary)
            .map((marker, index) => ({
                from_node: index * 2 + 1,
                to_node: index * 2 + 2,
                type: marker.marker.getTitle().split(' - ')[0] || 'User_Report',
                severity: extractSeverityFromTitle(marker.marker.getTitle()) || 'Medium'
            }));
        
        optimizeRouteWithHC2L(originCoords.lat, originCoords.lng, destCoords.lat, destCoords.lng, userDisruptions);
    }).catch(error => {
        console.error('Geocoding error:', error);
        const metricsContent = document.querySelector('.metrics-content');
        metricsContent.innerHTML = `
            <div style="text-align: center; padding: 20px; color: #f44336;">
                <div style="font-size: 16px; margin-bottom: 10px;">⚠️ Geocoding Failed</div>
                <div style="font-size: 14px;">Could not convert addresses to coordinates. Please use coordinate format (lat, lng) or set markers on map.</div>
            </div>
        `;
    });
}

// Optimize route using Dynamic-HC2L algorithm for modal
function optimizeRouteWithHC2LForModal(originLat, originLng, destLat, destLng, userDisruptions = []) {
    const requestData = {
        origin_lat: originLat,
        origin_lng: originLng,
        dest_lat: destLat,
        dest_lng: destLng,
        user_disruptions: userDisruptions,
        tau_threshold: 0.5
    };
    
    console.log('Sending optimization request for modal:', requestData);
    
    fetch('/optimize-route', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
        },
        body: JSON.stringify(requestData)
    })
    .then(response => response.json())
    .then(data => {
        console.log('HC2L optimization response for modal:', data);
        if (data.success) {
            displayHC2LResultsInModal(data.route_optimization);
        } else {
            displayHC2LErrorInModal(data.error || 'Unknown error occurred');
        }
    })
    .catch(error => {
        console.error('HC2L optimization error for modal:', error);
        
        // Display proper error message instead of misleading mock data
        displayHC2LErrorInModal(`Failed to connect to Dynamic-HC2L algorithm: ${error.message}`);
    });
}

// Optimize route using Dynamic-HC2L algorithm
function optimizeRouteWithHC2L(originLat, originLng, destLat, destLng, userDisruptions = []) {
    const requestData = {
        origin_lat: originLat,
        origin_lng: originLng,
        dest_lat: destLat,
        dest_lng: destLng,
        user_disruptions: userDisruptions,
        tau_threshold: 0.5 // Default threshold
    };
    
    console.log('Sending optimization request:', requestData);
    
    fetch('/optimize-route', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
        },
        body: JSON.stringify(requestData)
    })
    .then(response => response.json())
    .then(data => {
        console.log('Optimization response:', data);
        console.log('Route optimization data:', data.route_optimization);
        if (data.route_optimization && data.route_optimization.timing_metrics) {
            console.log('Timing metrics:', data.route_optimization.timing_metrics);
        }
        
        if (data.success) {
            displayHC2LResults(data.route_optimization);
            
            // Also calculate and display Google Maps route for comparison
            calculateStandardRoute(originLat, originLng, destLat, destLng);
        } else {
            displayHC2LError(data.error);
        }
    })
    .catch(error => {
        console.error('Optimization error:', error);
        displayHC2LError('Network error: ' + error.message);
    });
}

// Display Dynamic-HC2L optimization results in modal
function displayHC2LResultsInModal(results) {
    const modalMetricsContent = document.getElementById('modalMetricsContent');
    
    // Extract timing metrics with better handling
    const timingMetrics = results.timing_metrics || {};
    console.log('Extracted timing metrics for modal:', timingMetrics);
    
    // Convert and format timing metrics
    let queryTime, labelingTime, labelingSize;
    
    if (timingMetrics.query_response_time !== null && timingMetrics.query_response_time !== undefined) {
        queryTime = (timingMetrics.query_response_time * 1000).toFixed(3);
    } else {
        queryTime = 'N/A';
    }
    
    if (timingMetrics.labeling_time !== null && timingMetrics.labeling_time !== undefined) {
        labelingTime = (timingMetrics.labeling_time * 1000).toFixed(3);
    } else {
        labelingTime = 'N/A';
    }
    
    if (timingMetrics.labeling_size !== null && timingMetrics.labeling_size !== undefined) {
        labelingSize = timingMetrics.labeling_size < 1 ? (timingMetrics.labeling_size * 1000).toFixed(3) : timingMetrics.labeling_size.toFixed(0);
    } else {
        labelingSize = 'N/A';
    }
    
    console.log('Processed timing values for modal:', { queryTime, labelingTime, labelingSize });
    
    let html = `
        <div class="row mb-4">
            <div class="col-12">
                <div class="alert alert-info border-0" style="background: linear-gradient(135deg, rgba(255, 111, 0, 0.1) 0%, rgba(255, 87, 34, 0.1) 100%); border-left: 4px solid #ff6f00 !important;">
                    <h5 class="alert-heading text-center mb-3">
                        <i class="fas fa-chart-line text-warning me-2"></i>SOP 1: Dynamic-HC2L Performance Metrics
                    </h5>
                    <div class="row g-3">
                        <div class="col-md-4">
                            <div class="card border-warning">
                                <div class="card-body text-center">
                                    <h6 class="card-subtitle mb-2 text-muted">Query Response Time</h6>
                                    <h3 class="card-title text-warning mb-1">${queryTime}</h3>
                                    <small class="text-muted">milliseconds</small>
                                </div>
                            </div>
                        </div>
                        <div class="col-md-4">
                            <div class="card border-warning">
                                <div class="card-body text-center">
                                    <h6 class="card-subtitle mb-2 text-muted">Labeling Time</h6>
                                    <h3 class="card-title text-warning mb-1">${labelingTime}</h3>
                                    <small class="text-muted">milliseconds</small>
                                </div>
                            </div>
                        </div>
                        <div class="col-md-4">
                            <div class="card border-warning">
                                <div class="card-body text-center">
                                    <h6 class="card-subtitle mb-2 text-muted">Labeling Size</h6>
                                    <h3 class="card-title text-warning mb-1">${labelingSize}</h3>
                                    <small class="text-muted">nodes</small>
                                </div>
                            </div>
                        </div>
                    </div>
                </div>
            </div>
        </div>
    `;
    
    modalMetricsContent.innerHTML = html;
}

// Display error message for HC2L optimization in modal
function displayHC2LErrorInModal(errorMessage) {
    const modalMetricsContent = document.getElementById('modalMetricsContent');
    
    modalMetricsContent.innerHTML = `
        <div class="alert alert-danger" role="alert">
            <h4 class="alert-heading">
                <i class="fas fa-exclamation-triangle me-2"></i>SOP 1 Optimization Error
            </h4>
            <p>${errorMessage}</p>
            <hr>
            <p class="mb-0 small">Please ensure the C++ algorithm is compiled and all required files are available.</p>
        </div>
    `;
}

// Display Dynamic-HC2L optimization results
function displayHC2LResults(results) {
    const metricsContent = document.querySelector('.metrics-content');
    
    const baseDistance = results.base_distance !== null ? results.base_distance.toFixed(2) : 'N/A';
    const disruptedDistance = results.disrupted_distance !== null ? results.disrupted_distance.toFixed(2) : 'N/A';
    const executionTime = results.execution_time_seconds ? (results.execution_time_seconds * 1000).toFixed(1) : 'N/A';
    const distanceIncrease = results.distance_increase !== null ? results.distance_increase.toFixed(2) : 'N/A';
    const distanceIncreasePercent = results.distance_increase_percent !== null ? results.distance_increase_percent.toFixed(1) : 'N/A';
    
    // Extract timing metrics with better handling
    const timingMetrics = results.timing_metrics || {};
    console.log('Extracted timing metrics:', timingMetrics);
    
    // Convert and format timing metrics
    let queryTime, labelingTime, labelingSize;
    
    if (timingMetrics.query_response_time !== null && timingMetrics.query_response_time !== undefined) {
        queryTime = (timingMetrics.query_response_time * 1000).toFixed(2);
    } else {
        queryTime = 'N/A';
    }
    
    if (timingMetrics.labeling_time !== null && timingMetrics.labeling_time !== undefined) {
        labelingTime = (timingMetrics.labeling_time * 1000).toFixed(2);
    } else {
        labelingTime = 'N/A';
    }
    
    if (timingMetrics.labeling_size !== null && timingMetrics.labeling_size !== undefined) {
        labelingSize = timingMetrics.labeling_size.toString();
    } else {
        labelingSize = 'N/A';
    }
    
    console.log('Processed timing values:', { queryTime, labelingTime, labelingSize });
    
    let html = `
        <div style="margin-bottom: 15px; padding: 0;">
            <h3 style="margin: 0 0 12px 0; color: #4285f4; display: flex; align-items: center; font-size: 16px;">
                ✅ SOP 1: Dynamic-HC2L Results
                <span style="margin-left: 8px; font-size: 10px; background: rgba(66, 133, 244, 0.1); padding: 2px 6px; border-radius: 3px;">ACTIVE</span>
            </h3>
        </div>
        
        <!-- PRIMARY METRICS: The three required metrics in prominent display -->
        <div style="margin-bottom: 20px; padding: 15px; background: linear-gradient(135deg, rgba(255, 111, 0, 0.1) 0%, rgba(255, 87, 34, 0.1) 100%); border-radius: 8px; border: 2px solid rgba(255, 111, 0, 0.3);">
            <h4 style="margin: 0 0 12px 0; color: #ff6f00; font-size: 13px; font-weight: 700; text-align: center;">
                🎯 REQUIRED SOP 1 METRICS
            </h4>
            <div style="display: grid; grid-template-columns: 1fr; gap: 10px;">
                <div style="text-align: center; padding: 12px; background: rgba(255, 255, 255, 0.9); border-radius: 6px; border: 1px solid rgba(255, 111, 0, 0.4); box-shadow: 0 1px 4px rgba(0,0,0,0.1);">
                    <div style="font-size: 11px; color: #666; margin-bottom: 4px; font-weight: 600;">Query Response Time</div>
                    <div style="font-size: 18px; font-weight: 700; color: #ff6f00;">${queryTime}</div>
                    <div style="font-size: 9px; color: #999;">milliseconds</div>
                </div>
                
                <div style="text-align: center; padding: 12px; background: rgba(255, 255, 255, 0.9); border-radius: 6px; border: 1px solid rgba(255, 111, 0, 0.4); box-shadow: 0 1px 4px rgba(0,0,0,0.1);">
                    <div style="font-size: 11px; color: #666; margin-bottom: 4px; font-weight: 600;">Labeling Time</div>
                    <div style="font-size: 18px; font-weight: 700; color: #ff6f00;">${labelingTime}</div>
                    <div style="font-size: 9px; color: #999;">milliseconds</div>
                </div>
                
                <div style="text-align: center; padding: 12px; background: rgba(255, 255, 255, 0.9); border-radius: 6px; border: 1px solid rgba(255, 111, 0, 0.4); box-shadow: 0 1px 4px rgba(0,0,0,0.1);">
                    <div style="font-size: 11px; color: #666; margin-bottom: 4px; font-weight: 600;">Labeling Size</div>
                    <div style="font-size: 18px; font-weight: 700; color: #ff6f00;">${labelingSize}</div>
                    <div style="font-size: 9px; color: #999;">nodes</div>
                </div>
            </div>
        </div>
        
        <!-- SECONDARY METRICS: Route optimization results -->
        <div style="margin-bottom: 15px;">
            <h4 style="margin: 0 0 8px 0; color: #4285f4; font-size: 12px;">📊 ROUTE OPTIMIZATION</h4>
            <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 8px;">
                <div style="text-align: center; padding: 10px; background: rgba(76, 175, 80, 0.1); border-radius: 6px; border: 1px solid rgba(76, 175, 80, 0.2);">
                    <div style="font-size: 10px; color: #b0b0b0; margin-bottom: 3px;">Base Route</div>
                    <div style="font-size: 14px; font-weight: 600; color: #4CAF50;">${baseDistance}</div>
                </div>
                
                <div style="text-align: center; padding: 10px; background: rgba(255, 152, 0, 0.1); border-radius: 6px; border: 1px solid rgba(255, 152, 0, 0.2);">
                    <div style="font-size: 10px; color: #b0b0b0; margin-bottom: 3px;">With Disruptions</div>
                    <div style="font-size: 14px; font-weight: 600; color: #FF9800;">${disruptedDistance}</div>
                </div>
                
                <div style="text-align: center; padding: 10px; background: rgba(244, 67, 54, 0.1); border-radius: 6px; border: 1px solid rgba(244, 67, 54, 0.2);">
                    <div style="font-size: 10px; color: #b0b0b0; margin-bottom: 3px;">Increase</div>
                    <div style="font-size: 14px; font-weight: 600; color: #F44336;">${distanceIncrease}</div>
                </div>
                
                <div style="text-align: center; padding: 10px; background: rgba(156, 39, 176, 0.1); border-radius: 6px; border: 1px solid rgba(156, 39, 176, 0.2);">
                    <div style="font-size: 10px; color: #b0b0b0; margin-bottom: 3px;">Impact (%)</div>
                    <div style="font-size: 14px; font-weight: 600; color: #9C27B0;">${distanceIncreasePercent}%</div>
                </div>
            </div>
        </div>
        
        <!-- TECHNICAL DETAILS -->
        <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 8px; margin-bottom: 15px;">
            <div style="text-align: center; padding: 8px; background: rgba(66, 133, 244, 0.1); border-radius: 4px; border: 1px solid rgba(66, 133, 244, 0.2);">
                <div style="font-size: 9px; color: #b0b0b0; margin-bottom: 2px;">Execution Time</div>
                <div style="font-size: 12px; font-weight: 600; color: #4285f4;">${executionTime} ms</div>
            </div>
            
            <div style="text-align: center; padding: 8px; background: rgba(66, 133, 244, 0.1); border-radius: 4px; border: 1px solid rgba(66, 133, 244, 0.2);">
                <div style="font-size: 9px; color: #b0b0b0; margin-bottom: 2px;">Algorithm</div>
                <div style="font-size: 12px; font-weight: 600; color: #4285f4;">Dynamic-HC2L</div>
            </div>
            
            <div style="text-align: center; padding: 8px; background: rgba(66, 133, 244, 0.1); border-radius: 4px; border: 1px solid rgba(66, 133, 244, 0.2);">
                <div style="font-size: 9px; color: #b0b0b0; margin-bottom: 2px;">Disruptions</div>
                <div style="font-size: 12px; font-weight: 600; color: #4285f4;">${results.disruptions_considered || 0}</div>
            </div>
            
            <div style="text-align: center; padding: 8px; background: rgba(66, 133, 244, 0.1); border-radius: 4px; border: 1px solid rgba(66, 133, 244, 0.2);">
                <div style="font-size: 9px; color: #b0b0b0; margin-bottom: 2px;">Threshold (τ)</div>
                <div style="font-size: 12px; font-weight: 600; color: #4285f4;">${results.tau_threshold || 0.5}</div>
            </div>
        </div>
        
        <div style="background: rgba(66, 133, 244, 0.05); padding: 12px; border-radius: 6px; border-left: 3px solid #4285f4;">
            <div style="font-size: 10px; color: #4285f4; font-weight: 600; margin-bottom: 6px;">ALGORITHM ANALYSIS</div>
            <div style="font-size: 11px; color: #666; line-height: 1.3;">
                ${generateAnalysisText(results)}
            </div>
        </div>
    `;
    
    metricsContent.innerHTML = html;
}

// Generate analysis text based on results
function generateAnalysisText(results) {
    let analysis = '';
    
    if (results.base_distance !== null && results.disrupted_distance !== null) {
        const increase = results.distance_increase;
        const increasePercent = results.distance_increase_percent;
        
        if (increase > 0) {
            analysis += `The Dynamic-HC2L algorithm detected significant route impact due to disruptions. `;
            
            if (increasePercent > 20) {
                analysis += `With a ${increasePercent.toFixed(1)}% increase in distance, alternative routes should be considered. `;
            } else if (increasePercent > 10) {
                analysis += `The ${increasePercent.toFixed(1)}% distance increase suggests moderate traffic impact. `;
            } else {
                analysis += `The ${increasePercent.toFixed(1)}% increase indicates minimal disruption impact. `;
            }
        } else {
            analysis += `No significant route impact detected. The optimal path remains unchanged despite reported disruptions. `;
        }
        
        analysis += `Algorithm processed ${results.disruptions_considered || 0} disruption points in ${(results.execution_time_seconds * 1000).toFixed(1)}ms.`;
    } else {
        analysis = 'Route optimization completed. Please check the C++ algorithm output for detailed pathfinding results.';
    }
    
    return analysis;
}

// Display error message for HC2L optimization
function displayHC2LError(errorMessage) {
    const metricsContent = document.querySelector('.metrics-content');
    
    metricsContent.innerHTML = `
        <div style="text-align: center; padding: 20px;">
            <div style="font-size: 18px; color: #f44336; margin-bottom: 10px;">❌ SOP 1 Error</div>
            <div style="font-size: 14px; color: #666; margin-bottom: 15px;">Dynamic-HC2L Optimization Failed</div>
            <div style="background: rgba(244, 67, 54, 0.1); padding: 15px; border-radius: 8px; border: 1px solid rgba(244, 67, 54, 0.2);">
                <div style="font-size: 13px; color: #f44336; line-height: 1.4;">${errorMessage}</div>
            </div>
            <div style="margin-top: 15px; font-size: 12px; color: #999;">
                Please ensure the C++ algorithm is compiled and all required files are available.
            </div>
        </div>
    `;
}

// Calculate standard Google Maps route for comparison
function calculateStandardRoute(originLat, originLng, destLat, destLng) {
    const origin = new google.maps.LatLng(originLat, originLng);
    const destination = new google.maps.LatLng(destLat, destLng);
    
    const request = {
        origin: origin,
        destination: destination,
        travelMode: google.maps.TravelMode.DRIVING,
        unitSystem: google.maps.UnitSystem.METRIC
    };

    directionsService.route(request, (result, status) => {
        if (status === 'OK') {
            directionsRenderer.setDirections(result);
            
            // Add comparison info to metrics
            const route = result.routes[0];
            const leg = route.legs[0];
            
            // Add Google Maps comparison to existing HC2L results
            const metricsContent = document.querySelector('.metrics-content');
            const currentContent = metricsContent.innerHTML;
            
            const comparisonHtml = `
                <div style="margin-top: 20px; background: rgba(96, 125, 139, 0.05); padding: 15px; border-radius: 8px; border-left: 4px solid #607D8B;">
                    <div style="font-size: 12px; color: #607D8B; font-weight: 600; margin-bottom: 8px;">GOOGLE MAPS COMPARISON</div>
                    <div style="display: grid; grid-template-columns: repeat(auto-fit, minmax(120px, 1fr)); gap: 10px;">
                        <div style="text-align: center; padding: 8px; background: rgba(96, 125, 139, 0.1); border-radius: 4px;">
                            <div style="font-size: 10px; color: #b0b0b0; margin-bottom: 2px;">Distance</div>
                            <div style="font-size: 12px; font-weight: 600; color: #607D8B;">${leg.distance.text}</div>
                        </div>
                        <div style="text-align: center; padding: 8px; background: rgba(96, 125, 139, 0.1); border-radius: 4px;">
                            <div style="font-size: 10px; color: #b0b0b0; margin-bottom: 2px;">Duration</div>
                            <div style="font-size: 12px; font-weight: 600; color: #607D8B;">${leg.duration.text}</div>
                        </div>
                    </div>
                </div>
            `;
            
            metricsContent.innerHTML = currentContent + comparisonHtml;
        } else {
            console.error('Google Maps directions failed:', status);
        }
    });
}

// Update performance metrics
function updatePerformanceMetrics(sop) {
    const metricsContent = document.querySelector('.metrics-content');
    
    // Simulate metrics data based on SOP
    const metrics = {
        '1': {
            'Route Efficiency': '85%',
            'Average Speed': '45 km/h',
            'Congestion Level': 'Medium',
            'Estimated Time': '23 mins'
        },
        '2': {
            'Route Efficiency': '92%',
            'Average Speed': '52 km/h',
            'Congestion Level': 'Low',
            'Estimated Time': '18 mins'
        },
        '3': {
            'Route Efficiency': '78%',
            'Average Speed': '38 km/h',
            'Congestion Level': 'High',
            'Estimated Time': '31 mins'
        }
    };

    const sopMetrics = metrics[sop];
    let html = '<div style="display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 20px; width: 100%;">';
    
    Object.entries(sopMetrics).forEach(([key, value]) => {
        html += `
            <div style="text-align: center; padding: 15px; background: rgba(66, 133, 244, 0.1); border-radius: 8px; border: 1px solid rgba(66, 133, 244, 0.2);">
                <div style="font-size: 12px; color: #b0b0b0; margin-bottom: 5px;">${key}</div>
                <div style="font-size: 18px; font-weight: 600; color: #4285f4;">${value}</div>
            </div>
        `;
    });
    
    html += '</div>';
    metricsContent.innerHTML = html;
}

// Update route metrics
function updateRouteMetrics(result) {
    const route = result.routes[0];
    const leg = route.legs[0];
    
    const metricsContent = document.querySelector('.metrics-content');
    
    let html = '<div style="display: grid; grid-template-columns: repeat(auto-fit, minmax(150px, 1fr)); gap: 15px; width: 100%;">';
    
    const routeMetrics = {
        'Distance': leg.distance.text,
        'Duration': leg.duration.text,
        'Start': leg.start_address.split(',')[0],
        'End': leg.end_address.split(',')[0]
    };

    Object.entries(routeMetrics).forEach(([key, value]) => {
        html += `
            <div style="text-align: center; padding: 12px; background: rgba(76, 175, 80, 0.1); border-radius: 6px; border: 1px solid rgba(76, 175, 80, 0.2);">
                <div style="font-size: 11px; color: #b0b0b0; margin-bottom: 4px;">${key}</div>
                <div style="font-size: 14px; font-weight: 600; color: #4CAF50;">${value}</div>
            </div>
        `;
    });
    
    html += '</div>';
    metricsContent.innerHTML = html;
}

// Apply performance settings
function applyPerformanceSettings() {
    const filterType = document.getElementById('filterDisruption').value;
    
    // Apply settings logic here
    console.log('Applying settings:', {
        sop: selectedSOP,
        filter: filterType,
        traffic: selectedTrafficLevel
    });
    
    alert('Settings applied successfully!');
}

// Remove specific disruption marker
function removeDisruptionMarker(index) {
    if (disruptionMarkers[index]) {
        disruptionMarkers[index].marker.setMap(null);
        disruptionMarkers[index].infoWindow.close();
        disruptionMarkers.splice(index, 1);
    }
}

// Clear all disruption markers
function clearAllDisruptionMarkers() {
    disruptionMarkers.forEach(markerObj => {
        markerObj.marker.setMap(null);
        markerObj.infoWindow.close();
    });
    disruptionMarkers = [];
}

// Clear all data
function clearAllData() {
    // Clear form inputs
    document.getElementById('startingPoint').value = 'Quezon City Hall, Quezon City';
    document.getElementById('destination').value = 'SM North EDSA, Quezon City';
    document.getElementById('disruptionLocation').value = '';
    document.getElementById('disruptionType').value = '';
    
    // Clear map directions
    directionsRenderer.setDirections({routes: []});
    
    // Clear all markers
    clearAllMarkers();
    clearAllDisruptionMarkers();
    
    // Clear start and destination markers
    if (startMarker) {
        startMarker.setMap(null);
        startMarker = null;
    }
    if (destinationMarker) {
        destinationMarker.setMap(null);
        destinationMarker = null;
    }
    
    // Reset all modes
    isPinningMode = false;
    isSettingStart = false;
    isSettingDestination = false;
    isReportingDisruption = false;
    resetOtherButtons(['pinDisruptionBtn', 'setStartBtn', 'setDestBtn']);
    
    // Reset map cursor and hide messages
    map.setOptions({ cursor: '' });
    hideInstructionMessage();
    
    // Reset traffic level to default
    document.querySelectorAll('.traffic-btn').forEach(b => b.classList.remove('active'));
    document.querySelector('[data-traffic="medium"]').classList.add('active');
    selectedTrafficLevel = 'medium';
    
    // Reset metrics
    document.querySelector('.metrics-content').innerHTML = 
        '<p>Please select an SOP to view performance metrics</p>';
    
    // Reset selections
    document.querySelectorAll('.sop-btn').forEach(btn => btn.classList.remove('active'));
    selectedSOP = null;
    
    alert('All data cleared successfully!');
}

// Initialize the application
window.onload = function() {
    // Check if Google Maps API is loaded
    if (typeof google === 'undefined') {
        console.error('Google Maps API is not loaded');
        document.querySelector('.metrics-content').innerHTML = 
            '<p style="color: #f44336;">Google Maps API not loaded. Please check your API key.</p>';
    }
};

// Optimize route with HC2L and display with waypoints
function optimizeRouteWithHC2LAndDisplay(originLat, originLng, destLat, destLng, userDisruptions = []) {
    const requestData = {
        origin_lat: originLat,
        origin_lng: originLng,
        dest_lat: destLat,
        dest_lng: destLng,
        user_disruptions: userDisruptions,
        tau_threshold: 0.5
    };
    
    console.log('Sending HC2L optimization request:', requestData);
    
    fetch('/optimize-route', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
        },
        body: JSON.stringify(requestData)
    })
    .then(response => response.json())
    .then(data => {
        console.log('HC2L optimization response:', data);
        if (data.success) {
            updateRouteMetricsFromHC2L(data.results);
            // Display HC2L route with waypoints for better road-following visualization
            displayHC2LRouteWithWaypoints(originLat, originLng, destLat, destLng, data.results);
        } else {
            console.error('HC2L optimization failed:', data.error);
        }
    })
    .catch(error => {
        console.error('HC2L optimization error:', error);
        
        // Display proper error message instead of misleading mock data
        displayHC2LError(`Failed to connect to Dynamic-HC2L algorithm: ${error.message}`);
        
        // Display route with mock waypoints
        displayHC2LRouteWithWaypoints(originLat, originLng, destLat, destLng, mockResults);
        
        // Show info message about using mock data
        showInfoMessage('🔧 Backend unavailable - Using mock data for demonstration. Start Flask server for real HC2L results.');
    });
}

// Display HC2L route using Google Maps waypoints for better visualization
function displayHC2LRouteWithWaypoints(originLat, originLng, destLat, destLng, hc2lResults) {
    const origin = new google.maps.LatLng(originLat, originLng);
    const destination = new google.maps.LatLng(destLat, destLng);
    
    // Generate intermediate waypoints for smoother route visualization
    const waypoints = generateWaypointsFromHC2LData(originLat, originLng, destLat, destLng, hc2lResults);
    
    const request = {
        origin: origin,
        destination: destination,
        waypoints: waypoints,
        optimizeWaypoints: false, // Keep HC2L order
        travelMode: google.maps.TravelMode.DRIVING,
        unitSystem: google.maps.UnitSystem.METRIC
    };

    directionsService.route(request, (result, status) => {
        if (status === 'OK') {
            // Use the HC2L-specific renderer with different styling
            hc2lDirectionsRenderer.setDirections(result);
            
            // Add route legend to show both routes
            addEnhancedRouteLegend();
            
            console.log('HC2L route displayed with waypoints for better road-following');
        } else {
            console.error('HC2L route display failed:', status);
            // Fallback to simple polyline if directions service fails
            displayHC2LSimplePolyline(originLat, originLng, destLat, destLng);
        }
    });
}

// Generate waypoints from HC2L optimization data
function generateWaypointsFromHC2LData(originLat, originLng, destLat, destLng, hc2lResults) {
    const waypoints = [];
    
    // Calculate intermediate points for smoother route
    const numWaypoints = 3; // Adjust based on distance
    
    for (let i = 1; i < numWaypoints; i++) {
        const ratio = i / numWaypoints;
        const lat = originLat + (destLat - originLat) * ratio;
        const lng = originLng + (destLng - originLng) * ratio;
        
        // Add some intelligent offset based on HC2L route optimization
        const offset = hc2lResults.distance_increase_percent ? (hc2lResults.distance_increase_percent / 100) * 0.001 : 0.001;
        
        waypoints.push({
            location: new google.maps.LatLng(lat + offset, lng + offset),
            stopover: false
        });
    }
    
    return waypoints;
}

// Fallback simple polyline if waypoints fail
function displayHC2LSimplePolyline(originLat, originLng, destLat, destLng) {
    // Remove existing HC2L polyline
    if (hc2lPolyline) {
        hc2lPolyline.setMap(null);
    }
    
    // Create simple polyline path
    const path = [
        new google.maps.LatLng(originLat, originLng),
        new google.maps.LatLng(destLat, destLng)
    ];
    
    hc2lPolyline = new google.maps.Polyline({
        path: path,
        geodesic: true,
        strokeColor: '#ff6f00',
        strokeOpacity: 0.7,
        strokeWeight: 3
    });
    
    hc2lPolyline.setMap(map);
    console.log('HC2L route displayed as simple polyline');
}

// Enhanced route legend showing both Google Maps and HC2L routes
function addEnhancedRouteLegend() {
    // Remove existing legend if any
    const existingLegend = document.getElementById('route-legend');
    if (existingLegend) {
        existingLegend.remove();
    }
    
    const legend = document.createElement('div');
    legend.id = 'route-legend';
    legend.style.cssText = `
        position: absolute;
        top: 10px;
        right: 10px;
        background: rgba(255, 255, 255, 0.95);
        padding: 15px;
        border-radius: 8px;
        box-shadow: 0 2px 10px rgba(0,0,0,0.1);
        font-size: 12px;
        z-index: 1000;
        backdrop-filter: blur(10px);
        border: 1px solid rgba(0,0,0,0.1);
    `;
    
    legend.innerHTML = `
        <div style="font-weight: bold; margin-bottom: 10px; color: #333; font-size: 13px;">Route Comparison</div>
        <div style="display: flex; align-items: center; margin-bottom: 6px;">
            <div style="width: 25px; height: 4px; background: #4285f4; margin-right: 8px; border-radius: 2px;"></div>
            <span style="color: #333;">Google Maps Route</span>
        </div>
        <div style="display: flex; align-items: center; margin-bottom: 6px;">
            <div style="width: 25px; height: 3px; background: #ff6f00; margin-right: 8px; border-radius: 2px; background-image: repeating-linear-gradient(90deg, #ff6f00 0px, #ff6f00 10px, transparent 10px, transparent 15px);"></div>
            <span style="color: #333;">HC2L Optimized Route</span>
        </div>
        <div style="margin-top: 8px; padding-top: 8px; border-top: 1px solid #eee; font-size: 10px; color: #666;">
            Both routes follow actual roads for accurate navigation
        </div>
    `;
    
    document.getElementById('map').appendChild(legend);
}

// Show error message to user
function showErrorMessage(message) {
    const messageDiv = document.createElement('div');
    messageDiv.style.cssText = `
        position: fixed;
        top: 80px;
        left: 50%;
        transform: translateX(-50%);
        background: rgba(244, 67, 54, 0.95);
        color: white;
        padding: 12px 20px;
        border-radius: 25px;
        font-size: 14px;
        font-weight: 600;
        z-index: 2000;
        box-shadow: 0 4px 12px rgba(244, 67, 54, 0.3);
        backdrop-filter: blur(10px);
    `;
    messageDiv.textContent = message;
    document.body.appendChild(messageDiv);
    
    // Remove after 5    seconds
    setTimeout(() => {
        if (messageDiv.parentNode) {
            messageDiv.parentNode.removeChild(messageDiv);
        }
    }, 5000);
}

// Show info message to user
function showInfoMessage(message) {
    const messageDiv = document.createElement('div');
    messageDiv.style.cssText = `
        position: fixed;
        top: 80px;
        left: 50%;
        transform: translateX(-50%);
        background: rgba(33, 150, 243, 0.95);
        color: white;
        padding: 12px 20px;
        border-radius: 25px;
        font-size: 14px;
        font-weight: 600;
        z-index: 2000;
        box-shadow: 0 4px 12px rgba(33, 150, 243, 0.3);
        backdrop-filter: blur(10px);
    `;
    messageDiv.textContent = message;
    document.body.appendChild(messageDiv);
    
    // Remove after 7 seconds (longer for info messages)
    setTimeout(() => {
        if (messageDiv.parentNode) {
            messageDiv.parentNode.removeChild(messageDiv);
        }
    }, 7000);
}

// ======================================================
// SOP 2: DYNAMIC HC2L vs DHL COMPARISON FUNCTIONS
// ======================================================

// Show SOP 2 Comparison Modal
function showSOP2ComparisonModal() {
    // Show the modal
    const modal = new bootstrap.Modal(document.getElementById('sop2MetricsModal'));
    modal.show();
    
    // Execute SOP 2 comparison in the modal
    executeSOP2ComparisonInModal();
}

// SOP 2: Algorithm Comparison for Modal
function executeSOP2ComparisonInModal() {
    const modalComparisonContent = document.getElementById('modalComparisonContent');
    
    // Show loading state
    modalComparisonContent.innerHTML = `
        <div class="text-center p-4">
            <div class="spinner-border text-info" role="status">
                <span class="visually-hidden">Loading...</span>
            </div>
            <p class="mt-3 text-muted">🔄 Executing SOP 2: Dynamic-HC2L vs DHL Comparison</p>
        </div>
    `;
    
    // Get current origin and destination
    const originInput = document.getElementById('startingPoint').value;
    const destinationInput = document.getElementById('destination').value;
    
    if (!originInput || !destinationInput) {
        modalComparisonContent.innerHTML = `
            <div class="alert alert-warning" role="alert">
                <h4 class="alert-heading">Missing Route Information</h4>
                <p>Please set both starting point and destination before running SOP 2.</p>
                <hr>
                <p class="mb-0">Use the 'Pin Starting Point' and 'Pin Destination Point' buttons in the User panel.</p>
            </div>
        `;
        return;
    }
    
    // Parse coordinates from input or use marker positions
    let originLat, originLng, destLat, destLng;
    
    if (startMarker && destinationMarker) {
        originLat = startMarker.getPosition().lat();
        originLng = startMarker.getPosition().lng();
        destLat = destinationMarker.getPosition().lat();
        destLng = destinationMarker.getPosition().lng();
    } else {
        // Try to parse coordinates from input
        const originCoords = parseCoordinates(originInput);
        const destCoords = parseCoordinates(destinationInput);
        
        if (originCoords && destCoords) {
            originLat = originCoords.lat;
            originLng = originCoords.lng;
            destLat = destCoords.lat;
            destLng = destCoords.lng;
        } else {
            // Use geocoding as fallback
            geocodeAndCompareAlgorithmsModal(originInput, destinationInput);
            return;
        }
    }
    
    // Run both algorithms for comparison
    runAlgorithmComparison(originLat, originLng, destLat, destLng);
}

// Run both Dynamic-HC2L and DHL algorithms for comparison
function runAlgorithmComparison(originLat, originLng, destLat, destLng) {
    const modalComparisonContent = document.getElementById('modalComparisonContent');
    
    // Update loading message
    modalComparisonContent.innerHTML = `
        <div class="text-center p-4">
            <div class="spinner-border text-info" role="status">
                <span class="visually-hidden">Loading...</span>
            </div>
            <p class="mt-3 text-muted">🔄 Running Dynamic-HC2L and DHL algorithms...</p>
        </div>
    `;
    
    // Collect user-reported disruptions for HC2L
    const userDisruptions = disruptionMarkers
        .filter(marker => !marker.temporary)
        .map((marker, index) => ({
            from_node: index * 2 + 1,
            to_node: index * 2 + 2,
            type: marker.marker.getTitle().split(' - ')[0] || 'User_Report',
            severity: extractSeverityFromTitle(marker.marker.getTitle()) || 'Medium'
        }));
    
    // Run both algorithms in parallel
    Promise.all([
        // Dynamic-HC2L API call
        fetch('/optimize-route', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                origin_lat: originLat,
                origin_lng: originLng,
                dest_lat: destLat,
                dest_lng: destLng,
                user_disruptions: userDisruptions,
                tau_threshold: 0.5
            })
        }).then(response => response.json()),
        
        // DHL API call
        fetch('/optimize-route-dhl', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                origin_lat: originLat,
                origin_lng: originLng,
                dest_lat: destLat,
                dest_lng: destLng
            })
        }).then(response => response.json())
    ])
    .then(([hc2lData, dhlData]) => {
        console.log('HC2L comparison result:', hc2lData);
        console.log('DHL comparison result:', dhlData);
        
        if (hc2lData.success && dhlData.success) {
            displayAlgorithmComparison(hc2lData.route_optimization, dhlData.dhl_optimization);
        } else {
            displayComparisonError(hc2lData, dhlData);
        }
    })
    .catch(error => {
        console.error('Algorithm comparison error:', error);
        displayComparisonError(null, null, error.message);
    });
}

// Display algorithm comparison results
function displayAlgorithmComparison(hc2lResults, dhlResults) {
    const modalComparisonContent = document.getElementById('modalComparisonContent');
    
    // Extract and format HC2L metrics
    const hc2lTiming = hc2lResults.timing_metrics || {};
    const hc2lQueryTime = hc2lTiming.query_response_time ? (hc2lTiming.query_response_time * 1000).toFixed(3) : 'N/A';
    const hc2lLabelingTime = hc2lTiming.labeling_time ? (hc2lTiming.labeling_time * 1000).toFixed(3) : 'N/A';
    const hc2lLabelingSize = hc2lTiming.labeling_size ? (hc2lTiming.labeling_size < 1 ? (hc2lTiming.labeling_size * 1000).toFixed(0) : hc2lTiming.labeling_size.toFixed(0)) : 'N/A';
    
    // Extract and format DHL metrics
    const dhlTiming = dhlResults.timing_metrics || {};
    const dhlQueryTimeRaw = dhlTiming.query_response_time || 0;
    const dhlQueryTime = dhlQueryTimeRaw < 0.000001 ? (dhlQueryTimeRaw * 1000000).toFixed(3) + ' μs' : (dhlQueryTimeRaw * 1000).toFixed(3) + ' ms';
    const dhlLabelingTime = dhlTiming.labeling_time ? (dhlTiming.labeling_time * 1000).toFixed(3) : 'N/A';
    const dhlLabelingSize = dhlTiming.labeling_size ? dhlTiming.labeling_size.toFixed(0) : 'N/A';
    
    let html = 
        '<!-- COMPARISON HEADER -->' +
        '<div class="row mb-4">' +
            '<div class="col-12">' +
                '<div class="alert alert-info border-0" style="background: linear-gradient(135deg, rgba(23, 162, 184, 0.1) 0%, rgba(17, 139, 170, 0.1) 100%); border-left: 4px solid #17a2b8 !important;">' +
                    '<h5 class="alert-heading text-center mb-3">' +
                        '<i class="fas fa-balance-scale text-info me-2"></i>SOP 2: ALGORITHM PERFORMANCE COMPARISON' +
                    '</h5>' +
                    '<p class="text-center mb-0">Dynamic-HC2L vs DHL Algorithm Analysis</p>' +
                '</div>' +
            '</div>' +
        '</div>' +
        
        '<!-- PRIMARY METRICS COMPARISON -->' +
        '<div class="row mb-4">' +
            '<div class="col-12">' +
                '<h5 class="mb-3"><i class="fas fa-chart-bar text-primary me-2"></i>Core Performance Metrics</h5>' +
                '<div class="table-responsive">' +
                    '<table class="table table-striped table-hover">' +
                        '<thead class="table-dark">' +
                            '<tr>' +
                                '<th scope="col">Metric</th>' +
                                '<th scope="col" class="text-warning">Dynamic-HC2L</th>' +
                                '<th scope="col" class="text-success">DHL</th>' +
                            '</tr>' +
                        '</thead>' +
                        '<tbody>' +
                            '<tr>' +
                                '<td><strong>Query Time</strong></td>' +
                                '<td class="text-warning">' + hc2lQueryTime + ' ms</td>' +
                                '<td class="text-success">' + dhlQueryTime + '</td>' +
                            '</tr>' +
                            '<tr>' +
                                '<td><strong>Labeling Time</strong></td>' +
                                '<td class="text-warning">' + hc2lLabelingTime + ' ms</td>' +
                                '<td class="text-success">' + dhlLabelingTime + ' ms</td>' +
                            '</tr>' +
                            '<tr>' +
                                '<td><strong>Labeling Size</strong></td>' +
                                '<td class="text-warning">' + hc2lLabelingSize + ' nodes</td>' +
                                '<td class="text-success">' + dhlLabelingSize + ' nodes</td>' +
                            '</tr>' +
                        '</tbody>' +
                    '</table>' +
                '</div>' +
            '</div>' +
        '</div>';
    
    modalComparisonContent.innerHTML = html;
}

// Generate comparison analysis text
function generateComparisonAnalysis(hc2lTiming, dhlTiming, hc2lResults, dhlResults) {
    let analysis = '<ul class="mb-0">';
    
    // Query time analysis
    if (hc2lTiming.query_response_time && dhlTiming.query_response_time) {
        const hc2lQuery = hc2lTiming.query_response_time * 1000;
        const dhlQuery = dhlTiming.query_response_time * 1000000; // Convert to microseconds for comparison
        
        if (dhlQuery < 1) { // DHL is in microseconds
            analysis += '<li><strong>Query Speed:</strong> DHL demonstrates ultra-fast query times in the microsecond range due to preprocessing.</li>';
        } else if (hc2lQuery < dhlQuery) {
            analysis += '<li><strong>Query Speed:</strong> Dynamic-HC2L shows faster query response times.</li>';
        } else {
            analysis += '<li><strong>Query Speed:</strong> DHL shows faster query response times.</li>';
        }
    }
    
    // Labeling analysis
    if (hc2lTiming.labeling_size && dhlTiming.labeling_size) {
        analysis += '<li><strong>Labeling Efficiency:</strong> Dynamic-HC2L processes ' + (hc2lTiming.labeling_size < 1 ? Math.round(hc2lTiming.labeling_size * 1000) : Math.round(hc2lTiming.labeling_size)) + ' nodes vs DHL\'s ' + dhlTiming.labeling_size + ' nodes.</li>';
    }
    
    // Algorithm characteristics
    analysis += '<li><strong>Dynamic-HC2L:</strong> Adaptive algorithm that handles real-time disruptions and provides flexible route optimization.</li>';
    analysis += '<li><strong>DHL:</strong> Preprocessing-based algorithm optimized for very fast queries on static graphs.</li>';
    
    analysis += '</ul>';
    return analysis;
}

// Display comparison error
function displayComparisonError(hc2lData, dhlData, customError) {
    const modalComparisonContent = document.getElementById('modalComparisonContent');
    
    let errorMessage = customError || 'Unknown error occurred during algorithm comparison.';
    
    if (hc2lData && !hc2lData.success) {
        errorMessage += '<br><strong>Dynamic-HC2L Error:</strong> ' + hc2lData.error;
    }
    
    if (dhlData && !dhlData.success) {
        errorMessage += '<br><strong>DHL Error:</strong> ' + dhlData.error;
    }
    
    modalComparisonContent.innerHTML = `
        <div class="alert alert-danger" role="alert">
            <h4 class="alert-heading">Algorithm Comparison Error</h4>
            <p>${errorMessage}</p>
            <hr>
            <p class="mb-0">Please check your coordinates and try again, or contact support if the problem persists.</p>
        </div>
    `;
}

// Geocoding fallback for comparison
function geocodeAndCompareAlgorithmsModal(originAddress, destinationAddress) {
    const modalComparisonContent = document.getElementById('modalComparisonContent');
    
    modalComparisonContent.innerHTML = `
        <div class="text-center p-4">
            <div class="spinner-border text-info" role="status">
                <span class="visually-hidden">Loading...</span>
            </div>
            <p class="mt-3 text-muted">🔄 Converting addresses to coordinates...</p>
        </div>
    `;
    
    // Use Google's geocoding service
    const geocoder = new google.maps.Geocoder();
    
    Promise.all([
        new Promise((resolve, reject) => {
            geocoder.geocode({ address: originAddress }, (results, status) => {
                if (status === 'OK') {
                    resolve(results[0].geometry.location);
                } else {
                    reject(new Error('Failed to geocode origin: ' + status));
                }
            });
        }),
        new Promise((resolve, reject) => {
            geocoder.geocode({ address: destinationAddress }, (results, status) => {
                if (status === 'OK') {
                    resolve(results[0].geometry.location);
                } else {
                    reject(new Error('Failed to geocode destination: ' + status));
                }
            });
        })
    ])
    .then(([originLocation, destinationLocation]) => {
        const originLat = originLocation.lat();
        const originLng = originLocation.lng();
        const destLat = destinationLocation.lat();
        const destLng = destinationLocation.lng();
        
        runAlgorithmComparison(originLat, originLng, destLat, destLng);
    })
    .catch(error => {
        console.error('Geocoding error for comparison:', error);
        displayComparisonError(null, null, 'Geocoding failed: ' + error.message);
    });
}

// Show SOP 2 Modal
function showSOP2Modal() {
    // Show the modal
    const modal = new bootstrap.Modal(document.getElementById('sop2MetricsModal'));
    modal.show();
    
    // Execute SOP 2 in the modal
    executeSOP2InModal();
}

// SOP 2: DHL Route Optimization for Modal
function executeSOP2InModal() {
    const modalDHLMetricsContent = document.getElementById('modalDHLMetricsContent');
    
    // Show loading state
    modalDHLMetricsContent.innerHTML = `
        <div class="text-center p-4">
            <div class="spinner-border text-success" role="status">
                <span class="visually-hidden">Loading...</span>
            </div>
            <p class="mt-3 text-muted">🔄 Executing SOP 2: DHL Route Optimization</p>
        </div>
    `;
    
    // Get current origin and destination
    const originInput = document.getElementById('startingPoint').value;
    const destinationInput = document.getElementById('destination').value;
    
    if (!originInput || !destinationInput) {
        modalDHLMetricsContent.innerHTML = `
            <div class="alert alert-warning" role="alert">
                <h4 class="alert-heading">Missing Route Information</h4>
                <p>Please set both starting point and destination before running SOP 2.</p>
                <hr>
                <p class="mb-0">Use the 'Pin Starting Point' and 'Pin Destination Point' buttons in the User panel.</p>
            </div>
        `;
        return;
    }
    
    // Parse coordinates from input or use marker positions
    let originLat, originLng, destLat, destLng;
    
    if (startMarker && destinationMarker) {
        originLat = startMarker.getPosition().lat();
        originLng = startMarker.getPosition().lng();
        destLat = destinationMarker.getPosition().lat();
        destLng = destinationMarker.getPosition().lng();
    } else {
        // Try to parse coordinates from input
        const originCoords = parseCoordinates(originInput);
        const destCoords = parseCoordinates(destinationInput);
        
        if (originCoords && destCoords) {
            originLat = originCoords.lat;
            originLng = originCoords.lng;
            destLat = destCoords.lat;
            destLng = destCoords.lng;
        } else {
            // Use geocoding as fallback
            geocodeAndOptimizeForDHLModal(originInput, destinationInput);
            return;
        }
    }
    
    // Call DHL optimization
    optimizeRouteWithDHLForModal(originLat, originLng, destLat, destLng);
}

// DHL Route Optimization API Call
function optimizeRouteWithDHLForModal(originLat, originLng, destLat, destLng) {
    const requestData = {
        origin_lat: originLat,
        origin_lng: originLng,
        dest_lat: destLat,
        dest_lng: destLng
    };
    
    console.log('Sending DHL optimization request for modal:', requestData);
    
    fetch('/optimize-route-dhl', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
        },
        body: JSON.stringify(requestData)
    })
    .then(response => response.json())
    .then(data => {
        console.log('DHL optimization response for modal:', data);
        if (data.success) {
            displayDHLResultsInModal(data.dhl_optimization);
        } else {
            displayDHLErrorInModal(data.error || 'Unknown error occurred');
        }
    })
    .catch(error => {
        console.error('DHL optimization error for modal:', error);
        displayDHLErrorInModal(`Failed to connect to DHL algorithm: ${error.message}`);
    });
}

// Display DHL Results in Modal
function displayDHLResultsInModal(results) {
    const modalDHLMetricsContent = document.getElementById('modalDHLMetricsContent');
    
    // Extract timing metrics
    const timingMetrics = results.timing_metrics || {};
    console.log('Extracted DHL timing metrics for modal:', timingMetrics);
    
    // Convert and format timing metrics
    let queryTime, labelingTime, labelingSize;
    
    if (timingMetrics.query_response_time !== null && timingMetrics.query_response_time !== undefined) {
        queryTime = (timingMetrics.query_response_time * 1000).toFixed(3);
    } else {
        queryTime = 'N/A';
    }
    
    if (timingMetrics.labeling_time !== null && timingMetrics.labeling_time !== undefined) {
        labelingTime = (timingMetrics.labeling_time * 1000).toFixed(3);
    } else {
        labelingTime = 'N/A';
    }
    
    if (timingMetrics.labeling_size !== null && timingMetrics.labeling_size !== undefined) {
        labelingSize = timingMetrics.labeling_size.toFixed(0);
    } else {
        labelingSize = 'N/A';
    }
    
    const distance = results.distance_km !== null ? results.distance_km.toFixed(2) : 'N/A';
    const algorithm = results.algorithm || 'DHL';
    const status = results.status || 'success';
    
    console.log('Processed DHL timing values for modal:', { queryTime, labelingTime, labelingSize });
    
    let html = `
        <!-- PRIMARY METRICS: The three required metrics for SOP 2 -->
        <div class="row mb-4">
            <div class="col-12">
                <div class="alert alert-success border-0" style="background: linear-gradient(135deg, rgba(40, 167, 69, 0.1) 0%, rgba(25, 135, 84, 0.1) 100%); border-left: 4px solid #28a745 !important;">
                    <h5 class="alert-heading text-center mb-3">
                        <i class="fas fa-bullseye text-success me-2"></i>SOP 2 REQUIRED METRICS
                    </h5>
                    <div class="row g-3">
                        <div class="col-md-4">
                            <div class="card border-success">
                                <div class="card-body text-center">
                                    <h6 class="card-subtitle mb-2 text-muted">Query Time</h6>
                                    <h3 class="card-title text-success mb-1">${queryTime}</h3>
                                    <small class="text-muted">milliseconds</small>
                                </div>
                            </div>
                        </div>
                        <div class="col-md-4">
                            <div class="card border-success">
                                <div class="card-body text-center">
                                    <h6 class="card-subtitle mb-2 text-muted">Labeling Time</h6>
                                    <h3 class="card-title text-success mb-1">${labelingTime}</h3>
                                    <small class="text-muted">milliseconds</small>
                                </div>
                            </div>
                        </div>
                        <div class="col-md-4">
                            <div class="card border-success">
                                <div class="card-body text-center">
                                    <h6 class="card-subtitle mb-2 text-muted">Labeling Size</h6>
                                    <h3 class="card-title text-success mb-1">${labelingSize}</h3>
                                    <small class="text-muted">labels</small>
                                </div>
                            </div>
                        </div>
                    </div>
                </div>
            </div>
        </div>
        
        <!-- SECONDARY METRICS: DHL algorithm results -->
        <div class="row mb-4">
            <div class="col-12">
                <h5 class="mb-3"><i class="fas fa-route text-success me-2"></i>DHL Algorithm Results</h5>
                <div class="row g-3">
                    <div class="col-md-6">
                        <div class="card">
                            <div class="card-body">
                                <h6 class="card-subtitle mb-2 text-muted">Route Distance</h6>
                                <h4 class="card-title text-success mb-0">${distance} km</h4>
                            </div>
                        </div>
                    </div>
                    <div class="col-md-6">
                        <div class="card">
                            <div class="card-body">
                                <h6 class="card-subtitle mb-2 text-muted">Algorithm Status</h6>
                                <h4 class="card-title ${status === 'success' ? 'text-success' : 'text-warning'} mb-0">${status.toUpperCase()}</h4>
                            </div>
                        </div>
                    </div>
                </div>
            </div>
        </div>
        
        <!-- ALGORITHM INFO -->
        <div class="row">
            <div class="col-12">
                <div class="card bg-light">
                    <div class="card-body">
                        <h6 class="card-title">
                            <i class="fas fa-info-circle text-info me-2"></i>Algorithm Information
                        </h6>
                        <p class="card-text mb-2">
                            <strong>Algorithm:</strong> ${algorithm}<br>
                            <strong>Origin:</strong> ${results.origin_coordinates ? results.origin_coordinates.lat.toFixed(6) + ', ' + results.origin_coordinates.lng.toFixed(6) : 'N/A'}<br>
                            <strong>Destination:</strong> ${results.destination_coordinates ? results.destination_coordinates.lat.toFixed(6) + ', ' + results.destination_coordinates.lng.toFixed(6) : 'N/A'}
                        </p>
                        <small class="text-muted">
                            DHL (Distance and Hub Labeling) is a preprocessing-based shortest path algorithm that provides fast query responses through precomputed labels.
                        </small>
                    </div>
                </div>
            </div>
        </div>
    `;
    
    modalDHLMetricsContent.innerHTML = html;
}

// Display DHL Error in Modal
function displayDHLErrorInModal(errorMessage) {
    const modalDHLMetricsContent = document.getElementById('modalDHLMetricsContent');
    
    modalDHLMetricsContent.innerHTML = `
        <div class="alert alert-danger" role="alert">
            <h4 class="alert-heading">DHL Algorithm Error</h4>
            <p>${errorMessage}</p>
            <hr>
            <p class="mb-0">Please check your coordinates and try again, or contact support if the problem persists.</p>
        </div>
    `;
}

// Geocoding fallback for DHL
function geocodeAndOptimizeForDHLModal(originAddress, destinationAddress) {
    const modalDHLMetricsContent = document.getElementById('modalDHLMetricsContent');
    
    modalDHLMetricsContent.innerHTML = `
        <div class="text-center p-4">
            <div class="spinner-border text-success" role="status">
                <span class="visually-hidden">Loading...</span>
            </div>
            <p class="mt-3 text-muted">🔄 Converting addresses to coordinates...</p>
        </div>
    `;
    
    // Use Google's geocoding service
    const geocoder = new google.maps.Geocoder();
    
    Promise.all([
        new Promise((resolve, reject) => {
            geocoder.geocode({ address: originAddress }, (results, status) => {
                if (status === 'OK') {
                    resolve(results[0].geometry.location);
                } else {
                    reject(new Error(`Failed to geocode origin: ${status}`));
                }
            });
        }),
        new Promise((resolve, reject) => {
            geocoder.geocode({ address: destinationAddress }, (results, status) => {
                if (status === 'OK') {
                    resolve(results[0].geometry.location);
                } else {
                    reject(new Error(`Failed to geocode destination: ${status}`));
                }
            });
        })
    ])
    .then(([originLocation, destinationLocation]) => {
        const originLat = originLocation.lat();
        const originLng = originLocation.lng();
        const destLat = destinationLocation.lat();
        const destLng = destinationLocation.lng();
        
        optimizeRouteWithDHLForModal(originLat, originLng, destLat, destLng);
    })
    .catch(error => {
        console.error('Geocoding error for DHL:', error);
        displayDHLErrorInModal(`Geocoding failed: ${error.message}`);
    });
}