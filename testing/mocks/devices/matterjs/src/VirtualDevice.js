//------------------------------ tabstop = 4 ----------------------------------
//
// If not stated otherwise in this file or this component's LICENSE file the
// following copyright and licenses apply:
//
// Copyright 2026 Comcast Cable Communications Management, LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0
//
//------------------------------ tabstop = 4 ----------------------------------

/**
 * VirtualDevice - Abstract base class for matter.js virtual test devices.
 *
 * Provides:
 *   - Matter ServerNode initialization with configurable parameters
 *   - Side-band HTTP server for test driver communication
 *   - Operation registration/dispatch for device-specific side-band commands
 *   - Ready signal emission and graceful shutdown
 *
 * Subclasses should:
 *   1. Call super() with their device config
 *   2. Override createEndpoint() to return their device endpoint
 *   3. Call registerOperation() to register side-band operations
 */

import "@matter/nodejs";
import { ServerNode, Environment, Logger } from "@matter/main";
import http from "node:http";
import fs from "node:fs";
import os from "node:os";
import path from "node:path";

// Redirect all matter.js logging to stderr so stdout is clean for the ready signal
Logger.destinations.default.write = (_level, line) => {
    // Format objects properly instead of [object Object]
    const formatted = typeof line === "string" ? line : JSON.stringify(line, (_, v) => typeof v === "bigint" ? v.toString() + "n" : v);
    process.stderr.write(formatted + "\n");
};

export class VirtualDevice {
    constructor({
        deviceName = "Virtual Device",
        vendorId = 0xfff1,
        productId = 0x8000,
        passcode = 20202021,
        discriminator = 3840,
        port = 0,
    } = {}) {
        this.deviceName = deviceName;
        this.vendorId = vendorId;
        this.productId = productId;
        this.passcode = passcode;
        this.discriminator = discriminator;
        this.port = port;
        this.operations = new Map();
        this.serverNode = null;
        this.httpServer = null;
        this.endpoint = null;
        this.storageDir = fs.mkdtempSync(path.join(os.tmpdir(), "matterjs-device-"));
    }

    /**
     * Register a side-band operation handler.
     * @param {string} name - Operation name (e.g., "lock", "unlock", "getState")
     * @param {function} handler - Async function receiving (params) and returning a result object
     */
    registerOperation(name, handler) {
        this.operations.set(name, handler);
    }

    /**
     * Create the device endpoint. Subclasses must override this.
     * @returns {Endpoint} The matter.js Endpoint for this device
     */
    createEndpoint() {
        throw new Error("Subclasses must implement createEndpoint()");
    }

    /**
     * Start the virtual device: create the Matter server node, add the endpoint,
     * start the side-band HTTP server, and emit the ready signal.
     */
    async start() {
        // Configure matter.js to use our temp storage directory
        const env = Environment.default;
        env.vars.set("path.root", this.storageDir);
        // Create the ServerNode with commissioning parameters
        this.serverNode = await ServerNode.create({
            // Derive a stable, filesystem-safe ID from the device name so
            // matter.js can persist state without path issues.
            id: this.deviceName.replace(/\s+/g, "-").toLowerCase(),

            network: {
                port: this.port,
                // Use short subscription intervals so event reports arrive
                // promptly during testing (default is ~3 minutes).
                subscriptionOptions: {
                    maxInterval: 3000,
                    minInterval: 1000,
                    randomizationWindow: 0,
                },
            },

            commissioning: {
                passcode: this.passcode,
                discriminator: this.discriminator,
            },

            productDescription: {
                name: this.deviceName,
                deviceType: this.getDeviceType(),
            },

            basicInformation: {
                vendorName: "BartonCore Test",
                vendorId: this.vendorId,
                nodeLabel: this.deviceName,
                productName: this.deviceName,
                productLabel: this.deviceName,
                productId: this.productId,
                serialNumber: `BCORE-TEST-${Date.now()}`,
                uniqueId: `${Date.now()}-${Math.random().toString(36).slice(2)}`,
            },
        });

        // Create and add the device endpoint
        this.endpoint = this.createEndpoint();
        await this.serverNode.add(this.endpoint);

        // Start the side-band HTTP server
        const sidebandPort = await this.startSidebandServer();

        // Start the Matter server node (non-blocking — runs via event loop)
        await this.serverNode.start();

        // Get the actual Matter port
        const matterPort =
            this.serverNode.state.network.operationalPort || this.serverNode.state.network.port;

        // Emit the ready signal on stdout
        const readySignal = JSON.stringify({
            ready: true,
            sidebandPort,
            matterPort,
            passcode: this.passcode,
            discriminator: this.discriminator,
        });
        process.stdout.write(readySignal + "\n");

        // Set up graceful shutdown
        const shutdown = async () => {
            await this.stop();
            process.exit(0);
        };
        process.on("SIGTERM", shutdown);
        process.on("SIGINT", shutdown);
    }

    /**
     * Get the device type ID. Subclasses should override this.
     * @returns {number} The Matter device type ID
     */
    getDeviceType() {
        return 0x0000;
    }

    /**
     * Start the side-band HTTP server.
     * @returns {Promise<number>} The port the server is listening on
     */
    startSidebandServer() {
        return new Promise((resolve, reject) => {
            this.httpServer = http.createServer(async (req, res) => {
                if (req.method === "POST" && req.url === "/sideband") {
                    await this.handleSidebandRequest(req, res);
                } else {
                    res.writeHead(404, { "Content-Type": "application/json" });
                    res.end(JSON.stringify({ success: false, error: "Not found" }));
                }
            });

            this.httpServer.listen(0, "127.0.0.1", () => {
                const port = this.httpServer.address().port;
                resolve(port);
            });

            this.httpServer.on("error", reject);
        });
    }

    /**
     * Handle an incoming side-band HTTP request.
     */
    async handleSidebandRequest(req, res) {
        let body = "";

        for await (const chunk of req) {
            body += chunk;
        }

        let parsed;

        try {
            parsed = JSON.parse(body);
        } catch (error) {
            res.writeHead(400, { "Content-Type": "application/json" });
            res.end(JSON.stringify({ success: false, error: `Invalid JSON: ${error.message}` }));
            return;
        }

        const { operation, params } = parsed;

        if (!operation) {
            res.writeHead(400, { "Content-Type": "application/json" });
            res.end(JSON.stringify({ success: false, error: "Missing 'operation' field" }));
            return;
        }

        const handler = this.operations.get(operation);

        if (!handler) {
            res.writeHead(400, { "Content-Type": "application/json" });
            res.end(
                JSON.stringify({ success: false, error: `Unknown operation: ${operation}` }),
            );
            return;
        }

        try {
            const result = await handler(params || {});
            res.writeHead(200, { "Content-Type": "application/json" });
            res.end(JSON.stringify({ success: true, result }));
        } catch (error) {
            res.writeHead(500, { "Content-Type": "application/json" });
            res.end(JSON.stringify({ success: false, error: error.message }));
        }
    }

    /**
     * Stop the virtual device: shut down the Matter server node and HTTP server.
     */
    async stop() {
        if (this.httpServer) {
            await new Promise((resolve) => this.httpServer.close(resolve));
            this.httpServer = null;
        }

        if (this.serverNode) {
            await this.serverNode.close();
            this.serverNode = null;
        }

        // Clean up temp storage
        if (this.storageDir) {
            fs.rmSync(this.storageDir, { recursive: true, force: true });
        }
    }
}
