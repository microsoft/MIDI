---
layout: kb
title: How to make a floating toolbar or palette in Windows MIDI Glass
audience: everyone
description: Build a strip or a square of buttons in Windows MIDI Glass that sits over your other apps, stays in front of them, and shows only its buttons.
categories:
  - Getting Started
---

Windows MIDI Glass is the Windows MIDI Services app for building your own touch control surface. Most layouts fill a window or the whole screen. A **floating toolbar** is different: it's a small strip of buttons that sits over another app, the way the tool strip in a drawing app does. A **floating palette** is the same idea in a square.

They're handy when you want a few buttons over your DAW, a video app, or a live set, and you don't want to give up the screen to get them.

> **Windows MIDI Glass is a preview app.** Names and settings in this article can change before it ships.

On this page:

- [What makes a layout float](#what-makes-a-layout-float)
- [Start from a toolbar or a palette](#start-from-a-toolbar-or-a-palette)
- [Turn a layout you already have into a toolbar](#turn-a-layout-you-already-have-into-a-toolbar)
- [Using a floating toolbar](#using-a-floating-toolbar)
- [Tips](#tips)

## What makes a layout float

Three settings do it. They're on the layout's **Behavior** page, under **Runtime window**:

| Setting | What it does | Why it matters |
| --- | --- | --- |
| **Toolbar window** | The window has no title bar, buttons or border. It's exactly the size of the page, with a small handle at one end. | A title bar and a row of buttons would be bigger than a strip of buttons a few pixels high. |
| **Always on top** | The layout stays in front when you click in another app. | A toolbar that disappears behind your DAW the moment you use the DAW isn't much of a toolbar. |
| **Transparent background** | The page's background isn't drawn, so only the controls are. You see the app underneath between the buttons. | Only the buttons cover what's underneath. |

The three settings are separate, so you can use any mix of them. For example, **Always on top** on its own keeps a normal layout window in front of your other apps.

These settings take effect the next time the layout runs. While it's running, you can still turn **Always on top** on or off for that one session.

## Start from a toolbar or a palette

The quickest way to get one is to start from one:

1. In the Windows MIDI Glass library, select **New layout**.
2. Give it a name, and under **Send to** pick the device the buttons should play.
3. Under **Template**, pick one of these:
   - **Horizontal toolbar**: eight buttons in a row, on a page 800 × 120 pixels.
   - **Vertical toolbar**: eight buttons in a column, on a page 120 × 800 pixels.
   - **Floating palette**: sixteen buttons in a four by four square, on a page 360 × 360 pixels.
4. Select **Create**. The layout opens in the editor.

All three start with the three floating settings turned on and use the **Bone** theme, whose buttons are easy to read on their own.

Each button plays a note, starting at note 36 (C1) on channel 1 and counting up. A button sends a note on when you press it and a note off when you let go. Those are two rows in the button's **Action list** on the **Sends** tab: one with the trigger **On**, and one with **Off**. If you change the note, the channel, or the device, change it on both rows. Otherwise the note off goes somewhere else, and the note keeps playing.

## Turn a layout you already have into a toolbar

1. Open the layout in the editor.
2. Open the **Layout…** menu and select **Page size…**. Under **Preset**, pick **Horizontal toolbar**, **Vertical toolbar**, or **Floating palette**, or type your own width and height. A page can be as small as 32 pixels on a side.
3. Move your controls onto the smaller page.
4. Open the **Layout…** menu and select **Theme and colors…**. Then select **Behavior** in the list on the left.
5. Under **Runtime window**, turn on the settings you want.
6. Open the **Layout…** menu and select **Save and run**.

## Using a floating toolbar

A floating toolbar has one piece of window chrome: a thin **handle** with a move mark and three dots. It's at the left end of the toolbar, or at the top if the toolbar is taller than it is wide.

- **To move the toolbar,** drag the handle.
- **To open the toolbar menu,** click or right-click the handle. With the keyboard, press Tab until the handle has focus, then press Enter.

The menu has:

| Menu item | What it does |
| --- | --- |
| **Pages** | Switches to another page of the layout. |
| **Size** | Makes the whole toolbar bigger or smaller, from 50 % to 200 %. The page keeps its shape. |
| **Always on top** | Keeps the toolbar in front of other windows, or stops keeping it there, for this session only. The layout's own setting decides how it opens next time. |
| **Full screen** | Fills the screen with the layout. Press Esc to go back. |
| **Close** | Closes the toolbar. |
| **Panic** | Stops every note and sound on the devices the layout sends to, on every channel. Use it when a note gets stuck. Ctrl+Shift+P does the same thing. |

## Tips

- **Keep the page small.** You can see through the empty parts of a see-through page, but they still belong to the toolbar. A click there doesn't reach the app underneath. So a page that's just big enough for its buttons covers the least.
- **Pick a theme for the buttons, not the background.** A see-through window doesn't draw the page's background, so only the controls show. Choose a theme whose controls stand out over the apps you'll use it with.
- **It doesn't have to be buttons.** A toolbar can hold knobs, faders, or anything else Windows MIDI Glass has. The starters use buttons because they fit a strip best.
- **A transparent background works in a normal window too.** If you leave **Toolbar window** off, the title bar and the row of buttons above the page stay solid so you can still grab the window. Only the page is see-through.
