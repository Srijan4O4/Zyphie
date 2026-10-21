# Zyphie engineering instructions

Read docs/PROJECT_STATUS.md, docs/PRODUCT_REQUIREMENTS.md and docs/ROADMAP.md
before implementation. Treat the original product contract as requirements.

Use C++20, CMake, Zstandard/STORE, and (only after engine gates) Qt 6 Widgets.
Keep Qt out of the core. Do not introduce a web/backend stack.

Follow Understand -> Design -> Implement -> Build -> Test -> Review -> Document
-> Commit. Proceed in milestone order. Never call a gate passed without test
evidence. Document blockers and unexecuted tests precisely. Do not implement a
serializer before the binary layout is completely specified. No archive writes
or extraction until losslessness and filesystem safety have meaningful tests.

Never modify original input data, silently skip entries, or overwrite outputs
without explicit permission. Treat all archive data and paths as hostile.

Update project status, decisions, changelog and relevant specifications for each
checkpoint. Keep build outputs, local tool installations and private settings
out of commits. Use main for validated state and feature/<name> or fix/<name> for
substantial follow-up work. Do not rewrite shared history.
