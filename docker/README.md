# Barton Docker Process

The Barton project uses Docker to create a consistent and reproducible development environment.
Docker facilitates packaging all dependencies and configurations into a single image, and
ensures that the application runs the same way across different machines. This approach
simplifies the setup process, reduces compatibility issues, and streamlines the development
workflow.

## Compose File Layout

Barton uses a layered Docker Compose model. A base `compose.yaml` defines the core `barton`
service, and optional overlays add or override behavior:

| File | Purpose |
|---|---|
| `compose.yaml` | Base service definition (`barton`), network, volumes |
| `compose.devcontainer.yaml` | Overrides the `barton` service image/build for VS Code devcontainers (bakes the user into the image at build time) |
| `compose.host-network.yaml` | Switches to host networking for direct device access (e.g., Thread border routers) |
| `compose.otbr-radio.yaml` | Optional — adds the `otbr-radio` container (cpcd + otbr-agent) for real USB Thread radio support via BRD2703 |

All overlays operate on the same `barton` service from `compose.yaml`. They are combined by
listing them in order -- either in the `dockerComposeFile` array in `.devcontainer/devcontainer.json`
or via `-f` flags on the command line.

### Devcontainer

The devcontainer is configured in `.devcontainer/devcontainer.json` and composes:

```
compose.yaml → compose.devcontainer.yaml
```

### dockerw

The `dockerw` script at the repo root is a convenience wrapper for `docker compose run`.
It always includes `compose.yaml` as the base, and accepts flags to layer on overlays:

| Flag | Effect |
|---|---|
| `-H` | Use host networking |
| `-e <env>` | Pass extra environment variables |
| `-d` | Mount development volumes |
| `-n` | Disable TTY (non-interactive mode) |
| `-T` | Start the optional `otbr-radio` container (requires BRD2703 USB radio, see `docs/THREAD_BORDER_ROUTER_SUPPORT.md`) |

Example:

```bash
./dockerw -H bash          # with host networking
```

## Building

To build the Barton image, simply run `docker/build.sh` to create a Barton image.

## Editing the Image

During development, there may be situations where you need to make changes to the image itself.
During these circumstances, it's generally good practice to set the tag of the image to some
new tag while developing. In Barton, there are several places where this image tag must be known
at build or run time. In order to ensure this tag is available in all locations, the tag is stored
in `docker/.env` and must contain the custom tag in order for build/run process to work as
expected.

### Example

Say you were making some image related changes, this would be the process:

- Make some Dockerfile and/or image dependency changes
- Update the value of `IMAGE_TAG` in `docker/.env` to whatever tag you want, say `joetest1`.
- Execute `docker/build.sh` to build the image with that tag

At this point, the image is available locally with the designated tag, and all operations that rely on
this image with this specific tag will succeed.

### Note on Rebuilding the Devcontainer

Due to the layout of Barton's docker and devcontainer environment, running the devcontainer commands
from the command pallette to rebuild the image will not rebuild the base image but rather the extended
`Dockerfile.devcontainer` image. So if you trying to make any changes that will affect the base image,
you must run `docker/build.sh`, then rebuild the devcontainer for the changes to the base image to
reflect within the devcontainer.
