# Engineering Design Project Part I
## A) Embedded Traffic Light System
The following Traffic Light System (TLS) project is a real-time system using the FreeRTOS real-time kernel operating system for microcontrollers. The FreeRTOS OS and the TLS project code are being executed on the STM32F4_DISCOVERY microcontroller. The TLS project uses 3 tasks, 4 queues, 2 helpers functions, 2 middleware functions, and a timer callback function to manage the system. The system is required to move cars as lit LEDs from left to right across 19 LEDs. Between the 7th and 8th LED, there will be an intersection where cars will be required to stop. A traffic light must be configured to allow traffic to pass through on a green light. On a yellow or red light, traffic must stop before the intersection but continue after/ in the middle of the intersection. The following is a demonstration of the final solution.

![TLS](https://user-images.githubusercontent.com/44009838/163854717-7150245a-7019-4288-9f2a-8b50c85fe2e0.gif)

## A) Embedded Traffic Light System
