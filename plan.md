# Plaksha CANSAT Codebase

This codebase includes three elements:
* Mobile ESP32 (1) --- Placed on the pocketcube [ESP-IDF]
* Camera ESP32 --- A child to the mobile ESP322, running the camera module.
* Mobile ESP32 (2) --- Placed on the cansat [ESP-IDF]
* Station ESP32 --- Primary microcontroller for the ground station [ESP-IDF]
* Ground Station --- The user-facing ground station frontend [Python]

All the ESP32 devboards used utilize the esp2s3 chip.

# ESPNOw
All ESP32s communicate using the espnow protocol using unicast.

# Mobile ESP32s
Both mobile ESP32s must transmit data at a rate of 4Hz.
1) Mobile (1) starts transmitting after it is switched on. This switch is controlled by the mechanical team, but this must be taken into account on the software side. The architecture for this is left to the developer.

2) Mobile (2) transmits at 4Hz until the pocket cube is deployed, at which point the transmissin frequency falls to 1Hz.

Both mobile ESP32s must be able to parse commands from the ground station. The command architechture will be given in a subsection below. This will have to be expanded once more requirements are provided by the electronics team.

Both ESP32s will be required to do a certain level to data aquision from sensors (GPS IMU etc --- confer with electronics team) onboard. This is to be done using an I2C protocol (must be included) and polling. Polling rate is left to the developer.

Transmission data packet architechture has been given in a subsection below.

Aquired data is logged based on reuirements.

# Camera esp32s3 (Mobile)
The camera module attached to the pocketcube includes an esp32s3 chip---this will be utilied for most CV related tasks. Further work is to be outlined and documented by the responsible developer.

# Ground Station
The ground station is comprised of a PC running a python frontend, using an ESP32 for communication hardware. Raw data aquisition and mathematics is done onboard the ESP32. The python fontend is responsible purely for display, logging and sending commands.

## Data Display
...


# Packet formats and commands.

## Mobile Packets (Mobile -> Ground Station) for Routine Telemetry
```
<team_id>,<author>,<mode>,<type>,<packet_token>,<time>,<cmd_count>,<cmd_echo>, (debug)
<battery_voltage>,<gps_lat>,<gps_long>, (essentials)
<imu_ax>,<imu_ay>,<imu_az>,<imu_gx>,<imu_gy>,<imu_gz>, (imu sensor)
<other_sensors:not-determined-yet>, (other sensors)
<camera_frames_some-datatype>, (camera)
```

#### Debug:
``<team_id>`` :: ID for the Kalki Team (undetermined). Type Str or Int based on ID type.
``<author>`` :: Source for the packet---either the pocketcube `1` or cansat `2`. Type Int
``<mode>`` :: Transmission mode---Main mission `0` or CRITICAL_ERR `1` or Debug `2`.
``<type>`` :: Transmission type---Routine telemetry `0`, command_response `1`, ERROR `2`.
``<packet_token>`` :: Token assigned to the packet. Generally a count from 0 (Unsigned Long) to keep track of all pakcets Rx.
``<time>`` :: Seconds from the start of the program (Unsigned Long).
``<cmd_count>`` :: Number of commands parsed by the computer (Unsigned Int).
``<cmd_echo>`` :: Echos the previous command parsed by the computer (Str). Initialised to str("None").

#### Essential Readings:
``<battery_voltage>`` :: Battery voltage (type float).
``<gps_lat>`` :: GPS latitude (type float)
``<gps_long>`` :: GPS longitude (type float)

#### IMU Readings
Accelerometer and gyroscope readings. (Filtering can be done on the base station)

#### Other Misc. Sensors
...

#### Camera Frames
...

## Command Signatures (Ground Station -> Mobile)
```
<team_id>,<recipient>,<mode>,<type>
<cmd_body>
```

``<recipient>`` :: Using unicast, we expect this to be implicit. We add this for added benefit. Pocketcube `1` and Cansat `2`. 
``<type>`` :: Command going through (always) `1`.
``<cmd_body>`` :: Body of the command that needs to be parsed.

### Command Reply 
```
<team_id>,<author>,<mode>,<type = 1>,<packet_token>,<time>,<cmd_count>,<cmd_echo>, (debug)
<battery_voltage>,<gps_lat>,<gps_long>, (essentials)
<cmd_reply_body>
```

``cmd_reply_body`` :: Reply. Datatype varies

### Commands (so far)
1) ``ping <cansat/pocketcube>`` :: Expect Response "Pong" (Type Str). Mode DEBUG (2)
2) ``time`` :: Expect repsonse str("ACK") from the cansat. This command sends the UTC signature of the ground station to the cansat for UTC time synchronization.


# Remaining Requirements (not yet planned out)
* UTC Time Synchronization
* Mission time must be maintained through processor resets
* Container's clock must be set by a ground command to within 1 second of UTC before launch.