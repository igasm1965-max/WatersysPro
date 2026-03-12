# Version Management Guidelines

## Current Version
**v1.0.0** (2026-03-12)

## Versioning Scheme
This project uses [Semantic Versioning](https://semver.org/):
- **MAJOR** (X.0.0): Breaking changes to firmware/API
- **MINOR** (0.X.0): New features, backward compatible
- **PATCH** (0.0.X): Bug fixes, no new features

## Release Process

### 1. Before Release
- Update `CHANGELOG.md` with all changes
- Ensure all tests pass: `platformio run`
- Update `README.md` if needed
- Update `VERSION` file with new version number

### 2. Create Release Commit
```bash
git add -A
git commit -m "Release vX.Y.Z: Brief description

- Feature 1
- Feature 2
- Fix 1
"
```

### 3. Create Release Tag
```bash
git tag -a vX.Y.Z -m "Release vX.Y.Z - Full description

Release Date: YYYY-MM-DD

Changes:
- ...
"
```

### 4. Verify Tag
```bash
git tag -l
git show vX.Y.Z
```

## Version Files
- `VERSION` — Current version number (read by build scripts)
- `CHANGELOG.md` — Detailed change log for each release
- `platformio.ini` — Build configuration (platform-specific versioning)

## Important Branches
- **main** — Stable release branch (tagged versions only)
- **develop** — Development branch for unreleased features (optional)

## Commit Message Convention
- `feat:` — New feature
- `fix:` — Bug fix
- `docs:` — Documentation update
- `refactor:` — Code refactoring
- `test:` — Tests
- `chore:` — Build, CI, dependencies

Example:
```
feat: Add WebSocket support for real-time updates
docs: Update SD integration guide
fix: Correct log file rotation timing
```

## Release Checklist
- [ ] All features working (smoke tests)
- [ ] CHANGELOG.md updated
- [ ] VERSION file updated
- [ ] README.md updated if needed
- [ ] Commit created with release message
- [ ] Tag created with `-a` flag (annotated)
- [ ] Build size within limits (Flash < 97%, RAM < 50%)
- [ ] Serial monitor shows no errors on startup

## Example Releases
```bash
# v1.0.0: Initial stable release
git commit -m "Release v1.0.0: ..."
git tag -a v1.0.0 -m "Release 1.0.0 - ..."

# v1.1.0: Feature addition
git commit -m "Release v1.1.0: Add temperature sensor"
git tag -a v1.1.0 -m "Release 1.1.0 - Temperature sensor added"

# v1.0.1: Bug fix
git commit -m "Release v1.0.1: Fix LED timing issue"
git tag -a v1.0.1 -m "Release 1.0.1 - Bug fix"
```

## Reverting Changes
If a release has issues:
```bash
# View previous versions
git tag -l

# Checkout specific version
git checkout v1.0.0

# Revert to previous commit (creates new commit)
git revert <commit-hash>

# Force reset (danger! loses history)
git reset --hard <commit-hash>
```

## Viewing History
```bash
# View all commits with tags
git log --oneline --decorate

# View changes in specific tag
git show v1.0.0

# Compare versions
git diff v1.0.0 v1.1.0

# View tag details
git show-ref --tags
```

## Configuration
Git user configured as:
- **Name**: igasm1965-max
- **Email**: igas.m1965@gmail.com

Update with:
```bash
git config user.name "Your Name"
git config user.email "your@email.com"
```

---

Last updated: 2026-03-12
