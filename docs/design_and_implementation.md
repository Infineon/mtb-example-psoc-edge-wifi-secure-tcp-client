[Click here](../README.md) to view the README.

## Design and implementation

The design of this application is minimalistic to get started with code examples on PSOC&trade; Edge MCU devices. All PSOC&trade; Edge E84 MCU applications have a dual-CPU three-project structure to develop code for the CM33 and CM55 cores. The CM33 core has two separate projects for the secure processing environment (SPE) and non-secure processing environment (NSPE). A project folder consists of various subfolders, each denoting a specific aspect of the project. The three project folders are as follows:

**Table 1. Application projects**

Project | Description
--------|------------------------
*proj_cm33_s* | Project for CM33 secure processing environment (SPE)
*proj_cm33_ns* | Project for CM33 non-secure processing environment (NSPE)
*proj_cm55* | CM55 project

<br>

In this code example, at device reset, the secure boot process starts from the ROM boot with the secure enclave (SE) as the root of trust (RoT). From the secure enclave, the boot flow is passed on to the system CPU subsystem where the secure CM33 application starts. After all necessary secure configurations, the flow is passed on to the non-secure CM33 application.

In the CM33 non-secure application, the clocks and system resources are initialized by the BSP initialization function. The retarget-io middleware is configured to use the debug UART, the LED is initialized, a task called "network task" is created, and RTOS scheduler is started.

The rest of the application code is provided in the *secure_tcp_client.c* file and the keys and other macros are defined in the *network credentials.h* and *secure_tcp_client.h* files. *python-tcp-secure-client* contains the keys and certificates for the client and a python implementation of a simple secure TCP server.

In this example, the TCP client establishes a secure connection with a TCP server through an SSL handshake. During the SSL handshake, the client presents its SSL certificate (self-signed) for verification, and  verifies the server's identity to which it is connecting. See **Creating a self-signed certificate** section from [README](../README.md) for more details. 

Once the SSL handshake completes successfully, the TCP client toggles the user LED on or off based on the command received from the TCP server.
