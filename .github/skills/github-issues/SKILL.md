---
name: github-issues
description: 'Manage GitHub Issues for the ToshiNamura/XenonCLI repository. Use when the user asks to create, read, list, comment on, close, or search issues in this repo.'
---

# GitHub Issues — ToshiNamura/XenonCLI

Workflow for managing issues of `https://github.com/ToshiNamura/XenonCLI` via
the GitHub CLI (`gh`) or the GitHub REST API.

## Repository constants

- Owner: `ToshiNamura`
- Repository: `XenonCLI`
- API base URL: `https://api.github.com`

## When to use

- Create a new issue/task.
- Read the list or a specific issue.
- Reply/comment on an issue.
- Close, reopen, assign, or label an issue.
- Search issues by keywords.

## Running commands

Run all commands from the terminal (PowerShell). Priority:

1. **GitHub CLI** (`gh`) — if installed.
2. **REST API via `curl.exe`** — if `gh` is unavailable.

> On Windows PowerShell use `curl.exe`, not the `curl` alias (which resolves to
> `Invoke-WebRequest`).

## Check the environment

```powershell
gh --version          # is gh installed
gh auth status        # is authentication configured
$env:GH_TOKEN         # is a token set (never print its value in chat)
$env:GITHUB_TOKEN
```

> `gh` is installed at `C:\Program Files\GitHub CLI\gh.exe`. In already-open
> terminals PATH may be stale — then use the full path:
> `& "C:\Program Files\GitHub CLI\gh.exe" ...`.

## Authentication

A token is required for writes (create/comment/close):

- **Fine-grained PAT** with the `Issues` scope (Read and write) for this
  repository, or
- **Classic PAT** with the `repo` scope.

Set the token:

```powershell
$env:GH_TOKEN = "<token>"      # or $env:GITHUB_TOKEN = "<token>"
```

Or run `gh auth login`. Never print the token itself and never hardcode it in
files.

## Method 1 — GitHub CLI

```powershell
# list open issues
gh issue list --repo ToshiNamura/XenonCLI --state open

# view one issue
gh issue view 12 --repo ToshiNamura/XenonCLI

# create (always add at least one label)
gh issue create --repo ToshiNamura/XenonCLI --title "Title" --body "Description" --label bug

# manage labels
gh label list --repo ToshiNamura/XenonCLI
gh label create bug --repo ToshiNamura/XenonCLI --description "Something is broken"

# comment
gh issue comment 12 --repo ToshiNamura/XenonCLI --body "Comment text"

# close / reopen
gh issue close 12 --repo ToshiNamura/XenonCLI
gh issue reopen 12 --repo ToshiNamura/XenonCLI

# search
gh search issues --repo ToshiNamura/XenonCLI "bug"
```

## Method 2 — REST API (curl.exe)

Headers:

```powershell
$h = @('-H', 'Accept: application/vnd.github+json', '-H', "Authorization: Bearer $env:GH_TOKEN")
```

Operations:

```powershell
# list issues
curl.exe $h 'https://api.github.com/repos/ToshiNamura/XenonCLI/issues?state=open'

# view one issue
curl.exe $h 'https://api.github.com/repos/ToshiNamura/XenonCLI/issues/12'

# create an issue (body in body.json, always include labels)
# body.json: { "title": "...", "body": "...", "labels": ["bug"] }
curl.exe -X POST $h -H 'Content-Type: application/json' --data '@body.json' 'https://api.github.com/repos/ToshiNamura/XenonCLI/issues'

# comment
curl.exe -X POST $h -H 'Content-Type: application/json' --data '{ "body": "..." }' 'https://api.github.com/repos/ToshiNamura/XenonCLI/issues/12/comments'

# close an issue
curl.exe -X PATCH $h -H 'Content-Type: application/json' --data '{ "state": "closed" }' 'https://api.github.com/repos/ToshiNamura/XenonCLI/issues/12'

# search
curl.exe $h 'https://api.github.com/search/issues?q=repo:ToshiNamura/XenonCLI+bug'
```

## Rules

- Before creating an issue, confirm the title and description with the user if
  they were not provided explicitly.
- **Always add labels** when creating an issue (and when updating, if useful).
  Use the label convention below; create the label first if it is missing:
  - `bug` — something is broken or does not work
  - `feature` — new functionality
  - `enhancement` — improvement of existing behavior
  - `docs` — documentation
  - `roadmap` — item from ROADMAP.md
- Use Markdown for the issue body; include code snippets in fenced blocks.
- When replying to an issue, be concise; match the language used in the issue.
- Never print token values in chat; tokens are secrets handled only by the user.
- If authentication is missing (no `gh`, no token), tell the user what setup
  steps are needed and do not attempt to guess credentials.
