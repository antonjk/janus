# Releasing janus

Janus is distributed via the Homebrew tap `antonjk/homebrew-tap`
(`Formula/janus.rb`). A release is a git tag whose GitHub-generated tarball the
formula pins by SHA256.

## 1. Tag and push a release

```bash
git tag -a v1.0.0 -m "janus v1.0.0"
git push origin v1.0.0
```

GitHub then serves the source tarball at:

```
https://github.com/antonjk/janus/archive/refs/tags/v1.0.0.tar.gz
```

## 2. Compute the tarball SHA256

```bash
curl -fsSL https://github.com/antonjk/janus/archive/refs/tags/v1.0.0.tar.gz \
  | shasum -a 256
```

## 3. Update the formula

In `antonjk/homebrew-tap/Formula/janus.rb`:

- set `url` to the `v<version>` tarball,
- set `sha256` to the value from step 2,
- commit and push the tap.

## 4. Verify the install

```bash
brew tap antonjk/tap
brew install --build-from-source janus     # or just: brew install antonjk/tap/janus
brew test janus
brew audit --strict --online janus          # optional, pre-publish lint
```

## Notes

- The runtime requires Bash 4+, so the formula `depends_on "bash"` and rewrites the
  installed `janus` shebang to the Homebrew bash during install.
- For a new version, bump the tag, repeat steps 1-3; Homebrew users get it with
  `brew upgrade janus`.
