# ------------------------------ tabstop = 4 ----------------------------------
#
#  Copyright (C) 2024 Comcast Corporation
#
#  All rights reserved.
#
#  This software is protected by copyright laws of the United States
#  and of foreign countries. This material may also be protected by
#  patent laws of the United States and of foreign countries.
#
#  This software is furnished under a license agreement and/or a
#  nondisclosure agreement and may only be used or copied in accordance
#  with the terms of those agreements.
#
#  The mere transfer of this software does not imply any licenses of trade
#  secrets, proprietary technology, copyrights, patents, trademarks, or
#  any other form of intellectual property whatsoever.
#
#  Comcast Corporation retains all ownership rights.
#
# ------------------------------ tabstop = 4 ----------------------------------

import json
import os
import subprocess
import signal
import time


def terminate_program(process):
    try:
        process.terminate()
        process.wait()  # Wait for the process to terminate
        print(f"Terminated program with PID: {process.pid}")
    except Exception as e:
        print(f"An error occurred while terminating the program: {e}")


def write_to_named_pipe(message, path):
    try:
        # Open the named pipe in write mode
        with open(path, "w") as pipe:
            # Write the JSON string to the named pipe
            pipe.write(message)
            pipe.write()
    except Exception as e:
        print(f"An error occurred: {e}")


def start_light():
    process = subprocess.Popen("chip-lighting-app")
    return process


def send_to_light(message, process):
    # Construct the named pipe path
    pipe_path = f"/tmp/chip_lighting_fifo_{process.pid}"
    write_to_named_pipe(message, pipe_path)


light_process = start_light()
print(f"Started light with PID: {light_process.pid}")
time.sleep(3)
message = '{"Name":"InitialPress","NewPosition":0} '
send_to_light(message, light_process)
time.sleep(1)
message = '{"Name":"ShortRelease","PreviousPosition":0} '
send_to_light(message, light_process)

terminate_program(light_process)
