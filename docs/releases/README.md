# Release Notes

Every tagged release must have a matching release notes file:

```text
docs/releases/<tag>.md
```

For example, a `v0.1.2` release must include:

```text
docs/releases/v0.1.2.md
```

The GitHub release workflow uses this file as the published release body and
fails early if it is missing. Write the notes before pushing the release tag.

Recommended format:

```markdown
## Highlights

- Short user-facing summary of the most important changes.

## Added

- New features.

## Changed

- Behavior, UI, packaging, or documentation changes.

## Fixed

- Bug fixes and reliability improvements.

## Downloads

- `ZStreamEye-<tag>-windows-ucrt64-setup.exe`
- `ZStreamEye-<tag>-windows-ucrt64.zip`

## Notes

- Mention known limitations, unsigned installer warnings, or migration notes.
```
