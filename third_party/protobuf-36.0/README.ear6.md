# Protobuf runtime used by Ear6

This directory contains a source-level subset of protobuf 36.0, released by
Google under the BSD 3-Clause license in `LICENSE`.

Ear6 uses the small C `upb` runtime for its `.e6s` container. Generated schema
sources are checked into `src/state`, so users do not need `protoc` or any host
protobuf installation to build Ear6. The custom `CMakeLists.txt` intentionally
compiles only arena, generated-message, mini-table, and protobuf wire support.
It does not build the C++ protobuf runtime, reflection, JSON, text format, RPC,
compiler, or Abseil.

Upstream source: https://github.com/protocolbuffers/protobuf/releases/tag/v36.0
