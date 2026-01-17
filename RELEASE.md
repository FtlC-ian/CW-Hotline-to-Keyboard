# Creating a Release

This document explains how to create a new release with pre-built binaries.

## Automatic Release via GitHub Actions

The project is configured to automatically build executables for both Windows and macOS when you push a version tag.

### Steps to Create a Release:

1. **Update version in your code** (if you have version strings)

2. **Commit your changes:**
   ```bash
   git add .
   git commit -m "Prepare for release v1.0.0"
   ```

3. **Create and push a version tag:**
   ```bash
   git tag v1.0.0
   git push origin v1.0.0
   ```

4. **GitHub Actions will automatically:**
   - Build the Windows .exe using MinGW
   - Build the macOS binary using clang
   - Create a GitHub Release with both binaries attached
   - The release will appear at: `https://github.com/YOUR_USERNAME/CW-Hotline-to-Keyboard/releases`

## Manual Release (if needed)

If you need to manually create a release:

### Build Windows:
```powershell
cd serial-to-keyboard-c
gcc -Wall -O2 -D_WIN32 -o serial_keyboard.exe serial_keyboard.c -luser32
```

### Build macOS:
```bash
cd serial-to-keyboard-c
make
```

### Create GitHub Release:
1. Go to your repository on GitHub
2. Click "Releases" → "Draft a new release"
3. Create a new tag (e.g., `v1.0.0`)
4. Upload the compiled binaries:
   - `serial_keyboard.exe` (Windows)
   - `serial_keyboard` (macOS)
5. Write release notes
6. Click "Publish release"

## Version Numbering

Use semantic versioning: `vMAJOR.MINOR.PATCH`
- `MAJOR`: Breaking changes
- `MINOR`: New features (backward compatible)
- `PATCH`: Bug fixes

Examples:
- `v1.0.0` - First stable release
- `v1.1.0` - Added new features
- `v1.1.1` - Bug fixes
- `v2.0.0` - Breaking changes
