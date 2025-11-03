# shmipc Monorepo

**Shared Memory IPC Monorepo**

A monorepo containing the core shared memory IPC library and future language-specific SDKs.

## Structure

- **core/** - Core C library implementation
- **SDK directories** - Language-specific SDKs (planned)

## Building

From the root of the monorepo:

```bash
# Build everything
bazel build //...

# Build just the core library
bazel build //core:shmipc

# Run all tests
bazel test //...
```

See the individual submodule directories for more specific build instructions.

