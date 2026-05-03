# KWS Keyword Text2Token Tool

Convert plain wake-word text into the token format required by the bundled `sherpa-onnx-kws-zipformer-zh-en-3M-2025-12-20` model.

The tool is a small `uv` project. The first run creates `.venv` in this folder and installs `pypinyin`.

## Usage

Run directly through the PowerShell wrapper:

```powershell
.\Plugins\SherpaONNX\Tools\KeywordText2Token\Convert-KwsKeyword.ps1 "LIGHT UP" "LOVELY CHILD"
```

For Chinese output, prefer writing to a UTF-8 file because some Windows consoles render tone marks incorrectly:

```powershell
.\Plugins\SherpaONNX\Tools\KeywordText2Token\Convert-KwsKeyword.ps1 "女儿" "法国" -OutputFile .\Plugins\SherpaONNX\Content\Models\keywords.txt
```

Or call `uv run` yourself:

```powershell
$env:UV_PYTHON_INSTALL_DIR = ".\Plugins\SherpaONNX\Tools\KeywordText2Token\.uv-python"
uv --cache-dir .\Plugins\SherpaONNX\Tools\KeywordText2Token\.uv-cache run --project .\Plugins\SherpaONNX\Tools\KeywordText2Token python .\Plugins\SherpaONNX\Tools\KeywordText2Token\kws_text2token.py --text "女儿" --text "法国"
```

Write a UE-ready `keywords.txt`:

```powershell
.\Plugins\SherpaONNX\Tools\KeywordText2Token\Convert-KwsKeyword.ps1 `
  -InputFile .\raw_keywords.txt `
  -OutputFile .\Plugins\SherpaONNX\Content\Models\keywords.txt
```

Input files are UTF-8 and accept one keyword per line. You can optionally provide the display label after `@`:

```text
LIGHT UP @LIGHT_UP
女儿 @女儿
```

The script validates every generated token against the model `tokens.txt`, so invalid keywords fail before they reach Unreal.
