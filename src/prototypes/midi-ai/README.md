# MIDI AI assist prototype

**Prototype. Nothing here ships, and the repository's build scripts don't build it.**

It tries one idea: a customer asks their own AI assistant for a MIDI Patchbay patch or a MIDI Glass layout, answers a few questions, and gets a draft to review in the app. Nothing routes and no MIDI is sent until the customer says so in the app.

Findings, options and the decisions still open are in [design/MIDI-AI-assist-investigation.md](design/MIDI-AI-assist-investigation.md).

## What's here

| Folder | What it is |
|---|---|
| `mcp/` | `midi-mcp-spike.exe`, an MCP server written from scratch in C++. It talks to the assistant over stdin and stdout. |
| `mcp/test/` | A script that runs the server and checks every tool, and a checker built from MIDI Patchbay's own filter and transform code. |
| `design/comps/` | HTML mockups of how the apps could show drafts, and a script that turns them into pictures in `design/comps/shots/`. |

## Build

You need Visual Studio with the C++ desktop workload (v145 tools), and a Release build of the Windows MIDI Services SDK from this repository for the same platform. The spike reads the SDK's winmd and copies its DLL next to the exe, the same way every in-box tool carries its own copy.

```powershell
pwsh -File mcp\build.ps1
pwsh -File mcp\build.ps1 -Platform ARM64
```

The exe lands in `out\<platform>\Release\`.

## Test

```powershell
pwsh -File mcp\test\patch-oracle\build-oracle.ps1
pwsh -File mcp\test\Invoke-McpHarness.ps1
```

The MIDI service has to be running. The test writes only to temporary folders. If `midiglass.exe` is in this repository's build output, the test also uses it to draw a layout and to read back a saved draft. Last run: 89 passed, 0 failed.

## Try it with an assistant

It has been tried with GitHub Copilot in VS Code. Run **MCP: Open User Configuration** and add the server there, changing the path to match your clone. Don't add it to this repository's `.vscode` folder, because that folder is tracked.

```json
{
  "servers": {
    "windows-midi": {
      "type": "stdio",
      "command": "G:\\Github\\microsoft\\midi\\src\\prototypes\\midi-ai\\out\\x64\\Release\\midi-mcp-spike.exe",
      "args": ["--app", "all"]
    }
  }
}
```

| Option | What it does |
|---|---|
| `--app patchbay\|glass\|all` | Which app's tools to offer. The default is `all`. |
| `--patch-folder <folder>` | Where patch drafts go. The default is `Documents\MIDI Patchbay`, the folder MIDI Patchbay reads. |
| `--layout-folder <folder>` | Where layout drafts go. The default is `Documents\MIDI Layouts`, the folder MIDI Glass reads. |
| `--midiglass <path>` | The `midiglass.exe` that draws layout previews. By default the server looks in the installed tools folder, then in this repository's build output. |

Drafts go into the apps' real folders unless you pass the folder options. A patch draft is saved with `activateAtStartup` turned off, so it never routes by itself. MIDI Patchbay reads its folder only when it starts, so restart it to see a new draft. MIDI Glass shows a new draft the next time it refreshes its library.

## Tools

| Tool | Writes | What it does |
|---|---|---|
| `list_midi_endpoints` | Nothing | Lists connected MIDI endpoints with their groups, so the assistant uses real names instead of guesses. |
| `list_patches` | Nothing | Lists saved patches and says in plain words what each one does. |
| `preview_patch` | Nothing | Checks a patch request and describes it in plain words. It catches unknown devices, groups a device doesn't have, feedback loops (including loops with patches that route at startup) and settings that would pass nothing or be ignored. |
| `save_patch_draft` | One new file | Saves the patch as a draft. It never replaces an existing file. |
| `list_glass_controls` | Nothing | Lists the kinds of control a layout can use. |
| `list_glass_layouts` | Nothing | Lists saved layouts and marks drafts. |
| `preview_layout` | Temporary files only | Checks a layout request and returns a picture of it drawn by MIDI Glass itself. The picture doesn't show labels, so the text lists them. |
| `save_layout_draft` | One new file | Saves the layout as a draft. It never replaces an existing file. |

No tool sends MIDI, starts routing, changes or deletes an existing file, or reads the Windows MIDI Services configuration file.

## Comps

```powershell
pwsh -File design\comps\capture.ps1
```

This draws each comp with Edge in the background (no window opens) and saves it to `design\comps\shots\`.
