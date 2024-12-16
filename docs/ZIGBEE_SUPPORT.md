# Adding Zigbee Support to Barton

Barton interacts with a Zigbee stack through its ZHAL layer (see libs/zhal) which uses JSON payloads over IPC to another process that hosts the running Zigbee stack.

The reference implementation on top of Silab's Gecko SDK will be made available in the future. Until then, the ZHAL API can be implemented on top of any other Zigbee stack.
