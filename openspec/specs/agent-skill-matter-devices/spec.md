## ADDED Requirements

### Requirement: Matter virtual devices skill SKILL.md conforms to Agent Skills spec
The `matter-virtual-devices` skill SHALL be located at `.github/skills/matter-virtual-devices/SKILL.md`. The frontmatter SHALL include `name: matter-virtual-devices`, a `description` field explaining the skill covers working with Matter test devices, and `compatibility` noting the BartonCore Docker development container.

#### Scenario: Valid frontmatter
- **WHEN** the SKILL.md file is parsed
- **THEN** the YAML frontmatter SHALL contain `name: matter-virtual-devices` matching the directory name
- **AND** the `description` SHALL mention Matter, sample apps, chip-tool, virtual devices, and testing

### Requirement: Matter skill documents pre-built sample apps
The skill SHALL list the Matter sample apps available in the Docker container at `/usr/local/bin/`: `chip-lighting-app`, `chip-lock-app`, `thermostat-app`, `contact-sensor-app`. The skill SHALL explain how to start each app and what command-line options they support (e.g., `--discriminator`, `--KVS`). The skill SHALL explain that these are CHIP SDK sample apps compiled from the Matter SDK.

#### Scenario: Agent starts a pre-built light device
- **WHEN** the agent needs a Matter light device for testing
- **THEN** the skill SHALL show how to start `chip-lighting-app` with appropriate arguments

#### Scenario: Agent starts a pre-built lock device
- **WHEN** the agent needs a Matter door lock device for testing
- **THEN** the skill SHALL show how to start `chip-lock-app` with appropriate arguments

### Requirement: Matter skill documents chip-tool
The skill SHALL explain that `chip-tool` is available at `/usr/local/bin/chip-tool` and is used to commission and interact with Matter devices. The skill SHALL show common chip-tool commands: pairing (`chip-tool pairing code <node-id> <pairing-code>`), cluster operations (`chip-tool onoff on <node-id> <endpoint>`), and reading attributes (`chip-tool <cluster> read <attribute> <node-id> <endpoint>`).

#### Scenario: Agent commissions a device with chip-tool
- **WHEN** the agent needs to commission a Matter device
- **THEN** the skill SHALL show the `chip-tool pairing code` command with placeholder node ID and pairing code

#### Scenario: Agent sends a cluster command
- **WHEN** the agent needs to interact with a commissioned device
- **THEN** the skill SHALL show cluster command examples (e.g., `chip-tool onoff on`)

### Requirement: Matter skill documents matter.js virtual devices
The skill SHALL explain the matter.js virtual device framework located at `testing/mocks/devices/matterjs/`. The skill SHALL describe the Python wrapper classes in `testing/mocks/devices/matter/` (`MatterLight`, `MatterDoorLock`) and how they spawn Node.js subprocesses. The skill SHALL explain the sideband API for programmatic control (`device.sideband.send()`, `device.sideband.get_state()`).

#### Scenario: Agent understands matter.js device architecture
- **WHEN** the agent reads the matter virtual devices skill
- **THEN** it SHALL understand that Python classes wrap matter.js Node.js processes
- **AND** it SHALL know about the sideband control interface

### Requirement: Matter skill documents creating custom virtual devices
The skill SHALL explain how to create a new matter.js virtual device: create a JavaScript entry point in `testing/mocks/devices/matterjs/src/`, create a Python wrapper class in `testing/mocks/devices/matter/` extending `MatterDevice`, and specify the `matterjs_entry_point`. The skill SHALL note that custom devices useful for testing should be committed into the test harness with new integration tests, but this decision is made by the human.

#### Scenario: Agent creates a new virtual device type
- **WHEN** the agent needs a device type not already available
- **THEN** the skill SHALL outline the steps to create a new matter.js device and Python wrapper
- **AND** the skill SHALL note that the human decides whether to commit it

### Requirement: Matter skill includes error recovery pattern
The skill SHALL instruct the agent that if sample app binaries or `chip-tool` are not found, or if `node` is not available for matter.js devices, the agent SHALL check for `/.dockerenv`. If absent, the agent SHALL inform the user they need to run inside the development container.

#### Scenario: chip-tool not found outside Docker
- **WHEN** `chip-tool` is not found in PATH
- **AND** `/.dockerenv` does not exist
- **THEN** the agent SHALL stop and tell the user to run inside the development container
