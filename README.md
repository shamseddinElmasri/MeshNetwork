# MeshNetwork

This project includes the design and implementation of the network layer of a wireless mesh network system using nRF24L01+ transceiver module and STM32F303 MCU.
The main algorithm resides in cmdsTasksISRs.c file in the finiteStateMachine folder. 
This Algorithm enables the network nodes to relay packets to link source and destination when destination is unreachable.
Dynamic routing tables are periodically exchanged to keep track of all available nodes and their associated gateways.
An algorithm is designed to find the best route using the number of hops as a key metric.

Future_Work is done by "Mohana Velisetti" which lays the infrastructure for those who want to implement this system in bigger projects that include more transport layer features. Note that the code in the Future_Work folder has not been tested. It only provides a starting point for other interested developers.
