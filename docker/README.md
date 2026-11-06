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

Once you've verified your changes work, finalize by bumping the official image version so CI and
other developers pick up the new image:

1. Increment the version in `docker/version` (e.g., `2.6` → `2.7`)
2. Update `IMAGE_TAG` in `docker/.env` to match the new version (or just delete `docker/.env`
   to use the default from `docker/version`)
3. Rebuild with `docker/build.sh` and push the new image
4. The temporary testing tag (e.g., `joetest1`) can be deleted — it served its purpose

### Note on Rebuilding the Devcontainer

Due to the layout of Barton's docker and devcontainer environment, running the devcontainer commands
from the command pallette to rebuild the image will not rebuild the base image but rather the extended
`Dockerfile.devcontainer` image. So if you trying to make any changes that will affect the base image,
you must run `docker/build.sh`, then rebuild the devcontainer for the changes to the base image to
reflect within the devcontainer.

## Quality Tools Stage

The `quality_tools` stage (between `third_party_builder` and `dev_builder`) installs the static analysis, metrics, and SBOM tools used by `scripts/quality/` and documented in [docs/REQUIRED_CHECKS.md](../docs/REQUIRED_CHECKS.md). Scripts never install tools, so a tool missing from this stage makes its check report `blocked`.

| Tool | Source | Pin |
| --- | --- | --- |
| clang-tidy-18, cppcheck, abigail-tools | Ubuntu noble apt | `ARG *_VERSION` exact package versions |
| bloaty | Built from source | `ARG BLOATY_COMMIT`, verified with `git rev-parse` |
| syft (linux amd64) | GitHub release tarball | `ARG SYFT_VERSION` and `ARG SYFT_SHA256` |
| jscpd | npm | `ARG JSCPD_VERSION` |
| ruff, mypy, gcovr, lizard, pip-audit, pytest-cov | PyPI | `quality` dependency group in the repo-root `pyproject.toml` |

### Named build context and `.dockerignore`

The main build context is `docker/`. The Python tools are declared in the repo-root `pyproject.toml`, which is outside that context, so `compose.yaml` passes the repo root as the additional named context `repo` (`additional_contexts: repo: ..`). The Dockerfile reads it with `RUN --mount=from=repo,source=pyproject.toml,...`, which requires BuildKit (the default for `docker compose build`). The repo-root `.dockerignore` excludes everything except `pyproject.toml` so the `repo` context stays small; it has no effect on the `docker/` context.

### Bootstrap pip

The system pip in the image is 24.0, and `pip install --group` requires pip 25.1 or newer. The stage creates a throwaway venv, installs a hash-pinned pip into it (`ARG BOOTSTRAP_PIP_VERSION` and `ARG BOOTSTRAP_PIP_SHA256`), and uses it with `--python /usr/bin/python3 --break-system-packages` to install the `quality` group into the system interpreter, where the system `pytest` can load `pytest-cov`. The venv is removed in the same layer, and the system pip is left unchanged.

### Updating pins

1. apt packages: find the new version with `apt-cache policy <package>` inside the image and update the matching `ARG`. The packages are `apt-mark hold`-ed so the `apt-get upgrade` in `dev_builder` cannot replace them; if a newer LLVM point release lands, a later install that needs it (e.g. `clangd`) fails until `CLANG_TIDY_VERSION` is bumped to match.
2. bloaty: set `BLOATY_COMMIT` to the full commit SHA.
3. syft: set `SYFT_VERSION` and copy the linux amd64 tarball SHA-256 from the release's `checksums.txt` into `SYFT_SHA256`.
4. jscpd: set `JSCPD_VERSION`.
5. Python tools: edit the `quality` group in `pyproject.toml` (exact `==` pins).
6. Bootstrap pip: set `BOOTSTRAP_PIP_VERSION` and take the wheel's SHA-256 from its PyPI file listing.
7. Bump `docker/version` and rebuild with `docker/build.sh`.
