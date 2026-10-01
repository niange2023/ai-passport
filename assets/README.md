<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Assets

This directory stores reusable fonts, images, music, and sound effects, organized by asset type.

Keep each asset in the matching subdirectory and document its destination, naming, integration method, and source/license. Do not mix binary assets with Markdown documentation.

## Fonts

Store reusable font files and generated font sources in `fonts/`.

- Use descriptive names that include the family, weight, size, and format when relevant.
- Document the source, license, character range, conversion command, and expected destination.
- Check Flash and internal-RAM impact before adding a font; the ESP32-C3 has no PSRAM.
- Do not commit fonts whose license does not permit redistribution.

### Diamond Sutra app fonts

`fonts/sutra_cjk_14.c`, `fonts/sutra_cjk_16.c`, and
`fonts/sutra_cjk_20.c` are generated LVGL 9 plain 4 bpp subsets. The inventory
in `fonts/sutra_cjk_symbols.txt` covers all checked-in scripture, study prompts,
and UI strings.

| Item | Value |
| --- | --- |
| Source | Noto Serif CJK SC Medium (Source Han Serif / Song style), Copyright 2014-2021 Adobe, SIL Open Font License 1.1 |
| Source URL | `notofonts/noto-cjk`, `Serif/OTF/SimplifiedChinese/NotoSerifCJKsc-Medium.otf`; source font is downloaded for generation and is not committed |
| Source SHA-256 | `19322be2ecf5dd2abd546db7a86cc83c286026cbd28593583bfcfd721e48c9fe` |
| Inventory | 1,003 code points, including printable ASCII |
| Generator | `tools/sutra_font.py`, Pillow/FreeType rasterization with generated-file pixel verification |
| Regenerate | `python tools/sutra_font.py --font /path/to/NotoSerifCJKsc-Medium.otf` |
| Verify | `python tools/sutra_font.py --check` |
| Integration | Compiled into `main` by `main/CMakeLists.txt`; bitmap data remains in Flash |

## Images

Store reusable source images and generated display assets in `images/`.

| File | Dimensions and format | Use and source |
| --- | --- | --- |
| [`images/home.jpg`](images/home.jpg) | 3840 × 2160, JPEG | Product hero image embedded in both project README files to foreground AI Passport and its open, maker-oriented identity. |
| [`images/readme-hardware-specs.png`](images/readme-hardware-specs.png) | 2172 × 724, PNG RGBA | Optional technical infographic retained as a reference asset; it is no longer used as the homepage hero. Generated for this repository with the built-in image generation tool on 2026-09-17; the six labels and values were checked against the documented hardware contract. |
| [`images/logo-wordmark.png`](images/logo-wordmark.png) | 1648 × 336, PNG RGBA | Transparent black wordmark extracted from the repository's original `images/logo.png`; embedded in both project README files for light backgrounds. |
| [`images/logo-wordmark-dark.png`](images/logo-wordmark-dark.png) | 1648 × 336, PNG RGBA | White version of the extracted wordmark, used by the README `<picture>` element when GitHub is in dark mode. |

- Use descriptive names and document dimensions, pixel format, conversion steps, and destination.
- Prefer formats suitable for the 240 × 320 RGB565 display and account for Flash and internal RAM.
- Preserve editable sources where licensing permits, and record the source and license.
- Never commit device QR secrets, credentials, or personal data in images.

## Music and sound effects

Store reusable music and sound-effect sources in `music/`.

- Document the source, license, sample rate, bit depth, channels, conversion command, and destination.
- Prefer 16 kHz, 16-bit mono PCM when it matches the current BSP audio path.
- Check Flash and internal-RAM cost before embedding audio; stream or chunk long recordings.
- Do not commit media without redistribution permission.
