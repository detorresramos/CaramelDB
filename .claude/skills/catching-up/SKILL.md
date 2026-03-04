---
name: catching-up
description: Catch up on the current branch — understand what's changed relative to the base branch. Use when starting a session or switching context.
user-invocable: true
---

Get me up to speed on the current state of this branch. Focus on understanding the *actual code changes*, not git bookkeeping.

1. **Branch context**: Current branch name and base branch. One line.
2. **What changed**: Run `git diff <base>...HEAD --stat` to see which files changed, then read the key changed files and the diff (`git diff <base>...HEAD`) to understand the substance of the changes. Summarize *what* was added, removed, or modified and *why* — group related changes together thematically rather than file-by-file.
3. **Working tree**: Briefly note any staged changes, unstaged changes, or untracked files. Skip sections that are empty.

Prioritize reading and understanding the actual code over listing commits. The goal is for the reader to walk away understanding the design decisions and current state of work on this branch.

$ARGUMENTS
