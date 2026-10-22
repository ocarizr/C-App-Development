# Repository workflow

- Start each task on a new `codex/` branch from the current, up-to-date `master`
  supplied by the user. Preserve existing local changes.
- Before handing work back, stage all files belonging to the completed task.
  Leave unrelated local changes outside staging.
- Provide a suggested commit message in the final response.
- Do not create commits or push unless the user explicitly changes these instructions.

# C quality checks

- Keep implementations in C11 and use the repository `.clang-format` style.
- Check formatting and run clang-tidy with the repository `.clang-tidy` settings.
- Run the relevant CMake build and CTest checks after changing C sources.
