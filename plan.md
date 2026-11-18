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
<team_id>,<author>,<op_mode>,<type>,<packet_token>,<time>,<cmd_count>,<cmd_echo>, (debug)
<battery_voltage>,<gps_lat>,<gps_long>, (essentials)
<imu_ax>,<imu_ay>,<imu_az>,<imu_gx>,<imu_gy>,<imu_gz>, (imu sensor)
<other_sensors:not-determined-yet>, (other sensors)
<camera_frames_some-datatype>, (camera)
```

#### Debug:
``<team_id>`` :: ID for the Kalki Team (undetermined). Type Str or Int based on ID type.
``<author>`` :: Source for the packet---either the pocketcube `1` or cansat `2`. Type Int
``<op_mode>`` :: Operating mode: Flight `0` or simulation `1`. Type Unsigned Int
``<type>`` :: Transmission type---Routine telemetry `0`, command_response `1`, ERROR `2`, debug `3`.
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
<team_id>,<recipient>,<op_mode>,<type>
<cmd_body>
```
``<type>`` :: Fixed to `1` to denote commands. `2` are for critical commands. And `3` for special commands (example---keep expecting input of a certain format. This is used in a following section for uploading simulation data).

``<recipient>`` :: Using unicast, we expect this to be implicit. We add this for added benefit. Pocketcube `1` and Cansat `2`. 
``<type>`` :: Command going through (always) `1`.
``<cmd_body>`` :: Body of the command that needs to be parsed.

### Command Reply 
```
<team_id>,<author>,<op_mode>,<type = 1>,<packet_token>,<time>,<cmd_count>,<cmd_echo>, (debug)
<battery_voltage>,<gps_lat>,<gps_long>, (essentials)
<cmd_reply_body>
```

``cmd_reply_body`` :: Reply. Datatype varies

### Commands (so far)
1) ``ping <cansat/pocketcube>`` :: Expect Response "Pong" (Type Str). Response type DEBUG `3`
2) ``time`` :: Expect repsonse str("ACK") from the cansat. This command sends the UTC signature of the ground station to the cansat for UTC time synchronization. Response type REPLY `1`.
3) ``calibrate <all/cansat/pocketcube>`` :: Expect immediate response str("ACK") from (either/all) device(s) called. ... Expect response str("CAL DONE") after calibration is complete from either device. Both response types are REPLY `1`
4) ``arm deploy`` :: Expect immediate response str("ACK") and a second str("Arm Deployed") after the deployment is complete (if the mechanical team is able to sense this)

### Simualtion Commands 
This one is special so it gets it own subsection.

Sequence:
1) ``sim enable`` :: Expect response Str("ACK")
2) ``sim activate`` (two commands for extra protection) :: Expect response Str("Simulation Activated"). If ``sim_enable`` has not been run, expect response Str("Simulation not enabled!"). 
3) We need a procedure to upload csv data (given by the judges) of the pressure profile. The commands in the following format are set, and accepted only if the pocketcube is in SIM mode from step 1 and 2.

``simp _data`` is the command transmitted (this will be done automatically by the GS)

```
<team_id>,<recipient>,<op_mode = 1 (SIM)>,<type = 3 (SPECIAL)>
<cmd_body = _data>
```

:: Expect response str("ACK: _data")

This must be transmitted at 1Hz by the ground station. The cansat will not be timing the recieveing packets. The measurements are updated in real time and will reflect in the regular 4Hz transmissions of the pocketcube.

In this mode, all transmissions from the cansat/pocketcube must be ``<op_mode>`` = 1 => "SIM"

op_mode = 1 && type = 3 lets the computer know that its recieving barometric data that is then used to make computations regarding the altitude of the pocketcube. If this sort of packet is recieved during FLIGHT MODE (as opposed to SIM mode), then an error is transmitted back. (More on errors in the following sections)


```
<team_id>,<author>,<op_mode>,<type = 1>,<packet_token>,<time>,<cmd_count>,<cmd_echo>, (debug)
<error = Str("Recieved a simulated barometric datapacket unexpectedly. Mission in FLIGHT mode.")>,
<error_code = TBD>
```

``sim disable`` :: Takes up back to FLIGHT op_mode (0); expect response str("ACK: Simulation Deactivated.)

#### Mechanism Control Commands

1) ``mec deploy qube`` :: Expect response str("ACK: Deploying PocketQube..."). Deploys the PocketQube. Sent to the CanSat (2).
2) ``mec deploy solar_panel_1`` :: Expect response str("ACK: Deploying solar panel one..."). Sent to the PocketQube(1).
3) ``mec deploy solar_panel_2`` :: Expect response str("ACK: Deploying solar panel two..."). Sent to the PocketQube(1)
4) ``mec deploy boom`` :: Expect response str("ACK: Deploying magnetometer boom..."). Sent to the PocketQube(1).
5) ``cam start`` :: Expect response str("ACK: Starting camera recording...). 
6) ``cam rotate _plusminus_angle_value :: Expect response str(ACK: Rotating camera angle by _plusminus_angle_value). This function must be optimized for 90 degree rotations.
7) ``cam record start`` :: Expect response str("ACK: Starting video recording...). The video is recorded onto the onboard flash memory. 
8) ``cam record stop`` :: Expect response str("ACK: Stopping video recording...)

9) ``cam stabiliation _off_or_on`` :: <b>Note:</b> Camera stablization is always active by default. It can be turned off and on. Expect response str("ACK: Camera stabilization _off_or_on).


Ensure all the commands (apart from command 6 onwards) can only be run once. The ground station will not override the sending, but warn that these mechanism command has already been run when sending. The buttons on the ground station will prompt the user with a confirmation modal.

The PocketQube/CanSat will reply with an error message "This command can only be run once!"


<b>NOTE</b>: For commands that require processing time (such as sensor calibration), if fresh commands are issued before the previous command is finished, the command is added to a queue. Technically, a command data structure is created and a queue data structure is used to hold pointers to each of these cmd structs. This way every time a response to a command arrives, the ``<cmd_echo>`` always corresponds to the correct command issued. If a timeout is issued, an ERROR (1) mode transmission is issued. 

## Error Transmissions

### Command Error (Say a TimeOut)
```
<team_id>,<author>,<op_mode>,<type = 1>,<packet_token>,<time>,<cmd_count>,<cmd_echo>, (debug)
<error>,
<error_code>
```

``<error>`` :: Error message (Type Str) -> Displays this raw in the terminal. If the error is recognised the ground station handles it specially.
``<error_code>`` :: Codes given to specific error messages. If no code is assigned, code (TBD) is used.


# Remaining Requirements 
* Mission time must be maintained through processor resets
