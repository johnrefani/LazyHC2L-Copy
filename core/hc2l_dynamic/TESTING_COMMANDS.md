# HC2L Dynamic Testing Commands

## Quick build
```bash
cd /path/to/directory/LazyHC2L/core/hc2l_dynamic
mkdir -p build
cd build
cmake ..
make -j4
```


## Run Tests

### 1. Test gps_routing_json_api
```bash
cd build
./gps_routing_json_api 14.647631 121.064644 14.644476 121.064569 false 0.5
```

### 2. 


## Docker Set Up

##### Reminder 
- Make sure that the docker desktop is working.
- No need to mount the main files for running the container.

### 1. Pull the lazyhc2l image from the docker hub.
```bash
docker pull lanxe/lazyhc2l
```

### 2. Verify pulled image
```bash
docker images
```
*there should be an image **lanxe/lazyhc2l***

### 3. Start a container from the image
```bash
docker -d -p 5000:5000 --name lazyhc2l lanxe/lazyhc2l
```

### 4. Check if the container is running
```bash
docker logs lazyhc2l
```




