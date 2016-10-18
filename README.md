simple-mavlink-proxy
======

A simple mavlink proxy for mavlink communication analysis.
The basic flow of the program is shown below:

```
Serial Input(SiK radio or px4/APM compatible flight controller) 

	--> program(parses mavlink frames then prints those to a textfile)

	--> target GCS(QGroundControl or something similiar)(via UDP)

	--> program(processes frames from the GCS this time)

	--> Serial Output(SiK radio or px4/APM compatible flight controller) 
```

Project directory is as follows:

### bin/
	- Output executables(removed on ` $ make clean ` )

### build/
	- Object files(removed on ` $ make clean ` )

### include/
	- Project header files. Constants get defined here for the time being.

### lib/
	- MAVLink 1.0v c header files.

### src/
	- The source code.

### logs/
	- MAVLink transmission logs are kept here.
