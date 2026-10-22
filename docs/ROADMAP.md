# Roadmap and mandatory gates

The complete acceptance contract is PRODUCT_REQUIREMENTS.md. Status/evidence
lives in PROJECT_STATUS.md. No later feature bypasses an earlier failed gate.

| Milestone | Deliverables | Exit evidence |
| --- | --- | --- |
| 0 Foundation | Audit, architecture, risks, decisions, pinned build/test plan | Understood repo and reproducible build plan; scaffold checks recorded |
| 1 Format | Complete byte layouts, version/path rules, serialization utilities, codec integration and CI | Independent reader is implementable; encoding tests pass |
| 2 Engine | STORE then Zstandard, scanner, writer, reader, index, CLI, full digests | Byte-for-byte round trips; empty/Unicode/nested data; indexed listing |
| 3 Security | Checked bounds, safe platform writes, limits, malformed-input suite, fuzzers | Malicious paths cannot escape; corruption fails safely; sanitizers pass |
| 4 Performance | Bounded streaming/concurrency, presets, progress/cancel, selective extraction | Large-file memory measurements and reproducible benchmark results |
| 5 Desktop | Qt Widgets creation/browser/extraction/verification and cancellation | Actual user flows tested; responsive GUI and CLI compatibility |
| 6 Release | Clean builds, installation, licensing, packaging, compatibility evidence | All V1 acceptance criteria pass; only then tag v1.0.0 |

Milestones 0–3 are complete; validation evidence is in PROJECT_STATUS.md. Next is milestone 4 performance and usability.

Future only after V1: V2 adaptive codecs/dictionaries/solid groups; V3
content-defined chunks and snapshots; V4 authenticated encryption and wider
platform/format integration. No cloud, accounts, backend services, telemetry,
encryption, custom compression codec, or incremental snapshots in V1.
