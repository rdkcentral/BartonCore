# Barton Docker Process

The Barton project uses Docker to create a consistent and reproducible development environment.
Docker facilitates packaging all dependencies and configurations into a single image, and
ensures that the application runs the same way across different machines. This approach
simplifies the setup process, reduces compatibility issues, and streamlines the development
workflow.

Additionally, Barton uses Docker Compose to simplify the management of multi-container applications,
ensuring a consistent development environment. The Compose setup consists of a base `compose.yaml`
file, which defines the core services, and an extended `compose.devcontainer.yaml` that integrates
with the Visual Studio Code devcontainer for streamlined development. This approach allows for easy
dependency management, consistent local environments, and seamless collaboration across different
development setups.

## Building

To build the Barton image, simply run the `docker/build.sh` to create a Barton image.

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
