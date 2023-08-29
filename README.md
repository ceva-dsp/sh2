# SH2 Sensorhub driver for MCU application

The files in this repository provide application-level SH2 sensor hub functionality.

To use this code, an application developer will need to:
* Incorporate this code into a project.
* Provide platform-level functions, as specified in sh2_hal.h
* Develop application logic to call the functions in sh2.h

An example project based on this driver can be found here:
* [sh2-demo-nucleo](https://github.com/ceva-dsp/sh2-demo-nucleo)

## Release Notes
### Version 1.4.0
* Added reset-recovery to all API operations.  In the event of a device reset, SH2 API calls will return SH2_ERR.  (For operations that are intended to produce a device reset, that event is handled without error.)
* Added checks to all API calls to ensure sh2_open() was called prior.
* New parameters added to sh2_setTareNow()
* Added Euler decomposition functions to convert quaternions to yaw, pitch, roll.
* Updated sh2_getProductIds to read all ids for FSP201.

