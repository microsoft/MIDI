---
layout: tools_page
title: Windows MIDI Glass
tool: midiglass
description: Build your own on-screen MIDI controller with faders, knobs, pads and keys, and play it with touch, a pen or a mouse
icon: /assets/images/midiglass.png
categories:
  - General Purpose MIDI Tools
---

> This page covers information about a Windows MIDI Services feature and application that will be released to consumers in 2027. It's currently available for developers.

Windows MIDI Glass lets you build your own MIDI controller on screen. You put faders, knobs, buttons, pads and keys on a page, choose what each one sends, and then play it with your fingers, a pen or a mouse. It's at its best on a touch screen, or on a second monitor next to your keyboard, but it works on any PC.

Each design is called a **layout**. A layout can have several pages and send to several devices. Its controls can also move and light up when your gear or your DAW sends something back.

## Quick start

![The Windows MIDI Glass library, with numbered callouts on the New layout button, a layout card, a card's device status, the search and sort controls, the Ask an AI assistant, Open a file and More options buttons, the Appearance and settings button, and the status bar]({{ site.baseurl }}/assets/images/midiglass-quick-start.png)

Windows MIDI Glass opens to the **library**, where all your layouts are.

1. **New layout** makes a layout. You give it a name, pick the device it sends to, and pick a template to start from.
2. **Each card is a layout.** Click a card to run it. Point at a card to see **Run**, **Edit** and **…** for more options. You can also right-click a card for the same options.
3. **The status chip** on each card says whether the devices that layout sends to are connected right now.
4. **Search, sort and view.** Type to find a layout. Sort by last used, by name or by last changed. Switch between cards and a list. The list shows each layout's description under its name.
5. **Open…** runs a layout from any folder. **Ask an AI assistant…**, next to it, helps an AI assistant build a layout for you. **…** (More options) imports a pack, sets the name and license that go on what you make, opens the backups folder, and can keep the PC awake while a layout runs.
6. **Appearance and settings** sets the app's light or dark theme and its window background, and turns **AI assistant** on or off.
7. **The status bar** shows how many layouts you have and where they're saved. The chip on the right says whether the MIDI service is running.

### Make your first layout

1. Select **New layout**.
2. Type a name. Under **Send to**, pick the device you want to play.
3. Pick a **Template**, then select **Create**. The layout opens in the editor.
4. Select **Try** near the top left, and play the page. The **Sending** list under the page shows what goes out.
5. Select **Library** to go back. You don't need to save, because Windows MIDI Glass saves as you go. Click the new card to run your layout.

To play an app on the same PC, such as a DAW, pick **Default App Loopback (A)** under **Send to**. Then set the app to listen to **Default App Loopback (B)**. A loopback passes whatever goes into one side out of the other side.

## The editor

![The Windows MIDI Glass editor, with numbered callouts on the Add pane, a selected fader on the page, the inspector tabs, the Edit and Try switch, the toolbar, the Layout button, and the page tabs and Sending list under the page]({{ site.baseurl }}/assets/images/midiglass-editor.png)

1. **Add** lists every kind of control. Click one, then click the page where you want it. You can also drag one onto the page, or double-click one to drop it in the first open spot. **Outline**, next to **Add**, lists every control on the page.
2. **The page.** Click a control to select it. Drag it to move it, or drag a corner or an edge to resize it. The arrow keys move it one pixel at a time. Hold Shift to move it one grid square at a time.
3. **The inspector** changes the selected control. **Look** sets its color, style, size and label. **Sends** sets what it sends. **Listens** sets what it does when a device sends something back. **Behavior** sets how it moves and where it starts.
4. **Edit and Try.** **Try** plays the page for real without leaving the editor. Ctrl+Enter switches between the two.
5. **The toolbar** has **Undo** and **Redo**, **Snap** and the grid size, the align and arrange buttons, **Repeat**, **Group** and **Lock**.
6. **Layout…** has the settings for the whole layout: its theme and colors, its pages and devices, the page size, a background picture or video, its name, and its keyboard order. **Save and run** is here too.
7. **Under the page** are the page tabs. **+ Page** adds a page. **Sending** lists each message the layout sends while you try it.

Windows MIDI Glass saves your layout a moment after each change. The chip near the top right says **Saved** once it's done. Press Ctrl+S to save right away.

## Starting a new layout

**New layout** asks for three things:

- **Name** is what the card in the library shows. You can change it later.
- **Send to** is the device the controls play. You can add more devices later.
- **Template** is what the new layout starts with.

| Template | What you get |
| --- | --- |
| **Mixer** | Eight faders, eight knobs and eight pads on one page. |
| **DJ deck** | Two decks with filters, three band EQ, cue pads and a crossfader. |
| **Drum pads** | A four by four grid on channel 10, with the lowest note at the bottom left. |
| **Transport** | Play, stop, record and loop, plus eight track faders. |
| **Mackie Control** | Eight channel strips, a master fader, transport and a jog wheel, for a DAW set up to use a Mackie Control surface. See [How to control your DAW with Mackie Control in Windows MIDI Glass]({{ site.baseurl }}/kb/midi-glass-mackie-control/). |
| **Horizontal toolbar** | Eight buttons in a see-through strip that stays in front of your other apps. |
| **Vertical toolbar** | Eight buttons in a see-through column that stays in front of your other apps. |
| **Floating palette** | Sixteen buttons in a see-through square that stays in front of your other apps. |
| **Blank** | An empty page, ready for you to build on. |

The toolbar and palette templates are explained in [How to make a floating toolbar or palette in Windows MIDI Glass]({{ site.baseurl }}/kb/midi-glass-floating-toolbars/).

If no devices show up under **Send to**, plug one in, or make a loopback with [Windows MIDI Loopback Setup]({{ site.baseurl }}/tools/midiloopbacksetup/).

## Adding controls

The **Add** pane groups the controls by what they do.

| Group | Controls |
| --- | --- |
| Buttons | Button, Toggle, Pad, Page tab, Switch |
| Knobs and faders | Knob, Turntable, Fader, Wheel, Ribbon |
| Two axis | XY pad, Joystick |
| Generators | Beat clock, LFO, Steps |
| Keys and pads | Mono keyboard, Note pads, Hex pads, Fretboard |
| Feedback and text | Meter, Lamp, Readout, Stopwatch, Text, Image |
| Grouping | Group, Line |

A few of them need a word of explanation:

- A **Page tab** switches to another page of the layout when you press it.
- A **Switch** has two or more positions, and each position sends its own message.
- An **XY pad** or a **Joystick** sends two values at once: one for across and one for up and down.
- A **Ribbon** is a strip you slide a finger along. It sends one value, like a fader.
- **Generators** play by themselves once they start. **Beat clock** sends MIDI clock at the tempo you set. **LFO** sweeps a value up and down. **Steps** plays a short pattern of notes, one step at a time.
- **Note pads** and **Hex pads** take a finger on each pad, so you can play chords. The **Mono keyboard** plays one key at a time, and you can slide along it. Turn on **Show note names**, on its **Look** tab, to print each key's name and number on it.
- To play in one key on **Note pads** or **Hex pads**, pick the key and the scale on the **Look** tab, then turn on **In key only**. Every pad then plays a note of that scale, and the bottom left pad plays the root. **First note** picks the root's octave. It's how many grid controllers play in key, and a chord shape you learn works the same in every key.
- To change key while you play, select the pad grid and then **Add root and scale pickers**. That puts two switches on the page. One picks the root note and one picks the scale, such as Dorian or Lydian, and the pad grids they target follow them the moment you touch one. The layout opens in its saved key each time. To change which scales the picker offers, select it and check them under **Scales**.
- **Arrangement** sets how the pads sit. **Flow** fits as many pads to a row as the control is wide. **Rows** and **Columns** give a set grid, such as 8 rows of 24 pads, up to 1,024 pads. The pads stretch to fill the control, so making it bigger makes the pads bigger, and the grid keeps its shape. With rows that overlap, the same note is on more than one pad. Turn on **Highlight held notes** to light every pad playing a note you hold, so you can see the shape under your fingers.
- You can type a note's name in **First note**, **Lowest note** and a step's note, such as **F#0**, **Bb1** or **C3**, as well as its number. Middle C, note 60, is C3. Use # or b for sharps and flats. A pad grid's **First note** box shows both, such as F♯0 (30). If some pads would play a note past 127 or below 0, they're blank, and the inspector says how many.
- To move a pad grid while you play, select it and then **Add grid buttons**. Pick what you want, and Windows MIDI Glass puts them in a row under the grid: octave up and down, row up and down, an **In key only** switch, a readout that shows the key and octave, and a reset button. Each one changes only that grid. You can make your own too. Give a button a row of the type **Pad grid** and pick its **Function**, such as **Next scale** or **Shift right**. A switch's **Function** can also be **In key only**, **Row offset**, or **Octave**, and a readout's **Source** can show the grid's key, octave or row offset.
- Pickers, grid buttons and readouts change every pad grid unless you pick **Targets**. Uncheck **All grids** and check the grids you want, so a split layout can have its bass pads and lead pads in different keys and octaves. Nothing they change is saved. The layout opens as you built it each time.
- Pickers and grid buttons can target different grids, so two grids on one layout can play in different keys at once. Give each grid its own pickers, or use **Add root and scale pickers** on each one: the first pickers on a layout change every grid, and once a layout has pickers, new ones change only the grid you add them for. You can also pick **Targets** on any picker yourself.
- A **Fretboard** is a guitar neck you play with your fingers. Each string plays one note at a time, the way a real string does. Touch a fret to play its string. Touch a higher fret on the same string and the note changes without a new attack, which is a hammer-on. Lift it and the note goes back to the fret you're still holding, which is a pull-off. Turn off **Hammer-ons** to make every change a new note.
- Push your finger across a string, up or down, to bend the note up, as far as **Bend range**. Slide along the string to bend it a fret at a time (**Stepped**), smoothly like a fretless neck (**Smooth**), or to play a new note at each fret (**Off**).
- Pick a **Tuning**, such as **Drop D**, **DADGAD**, **Bass** or **Ukulele**, or set each string yourself under **Strings**. The list runs top to bottom the way the strings are drawn, so a guitar's low string is last and sits at the bottom. **Move up** and **Move down** reorder them, and changing a note makes the tuning **Custom**.
- **Visible frets** picks which part of the neck shows, such as frets 5 to 9. The strip under the strings shows the whole neck. Drag it to move along the neck while you play. **Capo** raises every open string, and the frets behind it play nothing.
- **Strum side** puts a strum area on the right, or on the left for a left-handed player, which turns the neck round. With **Play mode** set to **Strum**, touching a fret only holds it down, and sweeping across the strum area plays each string at the fret you hold, or open. A faster sweep plays louder. **Sustain** decides whether a string stops when you lift your finger (**While held**) or rings until you play it again (**Until replayed**). Each string still plays one note at a time.
- **Scale dots** mark the frets in a key, with the root in its own color. **Out-of-key frets** can play, dim, or be muted so they play nothing.
- **Output** decides how the strings reach your instrument. **Channel per string** puts each string on its own channel, counting up from the note row's channel, the way MIDI guitar pickups do. Most guitar instruments expect this, and it bends each string on its own. **MPE** uses the note row's channel as the manager channel and plays each string on a channel above it. Set your instrument up for MPE on those channels. **Per-note bend** keeps every string on one channel and bends with MIDI 2.0, so a MIDI 1.0 instrument hears no bends.
- To change a fretboard while you play, select it and then **Add fretboard buttons**. That adds a **Tuning** switch beside it and a row under it: **Capo down**, a readout that shows the capo, **Capo up**, **Down the neck**, **Up the neck** and **Mute**. Each one changes only that fretboard. You can make your own with a button's **Pad grid** row, too. Nothing they change is saved.
- **Meter**, **Lamp** and **Readout** show values that arrive from a device. You set that up on their **Listens** tab. A **Stopwatch** counts up from when the layout starts, so you can see how long you've been playing. Tap it to start again from zero.
- A **Group** control draws a frame around controls that belong together, and a **Line** divides one part of a page from another. Neither sends anything. Don't mix up the **Group** control with **Group** on the toolbar, which ties the selected controls together.

![A mono keyboard with Show note names on, showing each key's name and number from C2 48 to C4 72]({{ site.baseurl }}/assets/images/midiglass-keyboard-note-names.png)

Type in **Search controls** to narrow the list. To add a control without the mouse, pick it with the keyboard and select **Add to page**. **Add to page** stays off until you pick a control.

## Arranging controls

- **Select more than one.** Hold Shift or Ctrl and click, or drag a box around them on an empty part of the page. Ctrl+A selects everything on the page that isn't locked.
- **Snap** lines controls up with the grid, with each other and with the page edges. Hold Alt while you drag to turn it off for a moment. Hold Shift while you drag to keep the move straight across or straight down.
- **Align and arrange** line up the selected controls, or spread them out evenly.
- **Gap labels.** Select three or more controls in a row or a column, and the gap between each pair shows as a number under the row or beside the column. When every gap is the same, the numbers are filled in. Click one, type a number and press Enter, and every gap becomes that many pixels. The first control stays where it is.
- **Repeat** makes a row or a column of copies of what's selected. It can count the channel, the note or controller number, or the group up on each copy, and number the labels too, so one channel strip becomes a whole bank.
- **Group**, on the toolbar, makes several controls act as one when you select and move them. Ctrl+G groups, and Ctrl+Shift+G ungroups. Lining up, spreading out and gap labels treat a group as one block too, so a channel strip keeps its shape. Select one group on its own to line up the controls inside it.
- **Lock** stops a control from being picked or moved on the page. That's handy for a panel or a picture behind other controls. A locked control shows a lock on the page and in the **Outline**. To unlock it, select it in the **Outline**, then select **Unlock**.

| Keys | What they do |
| --- | --- |
| Ctrl+Z, Ctrl+Y | Undo, redo |
| Ctrl+C, Ctrl+X, Ctrl+V | Copy, cut, paste |
| Ctrl+D | Duplicate the selection |
| Delete | Delete the selection |
| Arrow keys | Move the selection one pixel |
| Shift+arrow keys | Move the selection one grid square |
| F2 | Rename the selected control |
| Esc | Put away the control you picked in **Add** |
| Ctrl+Enter | Switch between **Edit** and **Try** |
| Ctrl+S | Save now |

## What a control sends

The **Sends** tab has an **Action list**. Each row in it says when to send, what to send, and where to send it.

- **Trigger** is one of these: **Value change**, **On**, **Off**, **Touch** or **Release**. A fader usually sends on **Value change**. A button usually sends one message on **On** and another on **Off**.
- **Type** can be a note, a control change, a program change, pitch bend, channel pressure, a per-note controller, an RPN or NRPN, or system exclusive.
- **To** is one of the layout's devices, with its group and channel.

A few types need more than a number:

- **A per-note controller** changes one note that's playing, not the whole channel. **Note** is the note, and **Controller** is which controller. **Per-note controller (registered)** has a list of the controllers MIDI 2.0 names, such as **Volume** and **Pan**. **Per-note controller (assignable)** is for the ones your instrument defines itself, so take the numbers from its manual.
- **A note sent as MIDI 2.0** has an **Attribute** row. Type the attribute type, then its data, in hexadecimal, from the instrument's manual or the profile's specification. It goes out with the note on or the note off that row sends, so a button that sends both needs it on both rows. A keyboard or a pad grid puts it on every note it plays.

![The Sends tab for a knob that sends a registered per-note controller, with Note 60 and Controller 7, which the list names Volume]({{ site.baseurl }}/assets/images/midiglass-per-note-controller.png) ![The Sends tab for a pad's note on row, with the attribute type 02 and the attribute data 0001]({{ site.baseurl }}/assets/images/midiglass-note-attribute.png)

Windows MIDI Glass names notes the same way the rest of Windows MIDI Services does: note 60, middle C, is C3.

The fastest way to fill in a row is **MIDI Learn**. Turn on **Learn**, then move a knob or press a key on your hardware, and Windows MIDI Glass copies what it sends. **Learn bank** fills several controls in a row: touch the knobs on your hardware one after another, and each one fills the next control in keyboard order. MIDI Learn listens to the devices in the layout's device list, so add your hardware there first.

Each device has a **Protocol** setting: **MIDI 2.0**, **MIDI 1.0** or **Mackie Control**. It decides how you type values on the **Sends** tab. [How Windows MIDI Glass talks to each device]({{ site.baseurl }}/kb/midi-glass-mackie-control/#how-midi-glass-talks-to-each-device) explains the choices.

## Controls that listen

On the **Listens** tab, turn on **MIDI follow** and the control moves or lights up when a device sends something, not only when you touch it. Under **Follow mode**, pick one of these:

- **Single message**: the control follows one controller, note or pitch bend, and shows its value. Use it when your DAW should move a fader, the way it moves a motorized fader on a hardware desk.
- **Notes**: the control lights while any note is held.
- **Control changes**: the control blinks whenever a control change arrives.
- **Transport**: the control lights while the other end is playing.
- **Beat**: the control flashes on the beat.
- **Any activity**: the control lights whenever anything arrives, like an activity light.

A fader that follows the DAW can be a long way from your finger when you touch it. On the **Behavior** tab, **Pickup** decides what happens then, for a fader, an XY pad, a joystick or a ribbon. **Jump** goes straight to your finger. **Catch** waits until your finger reaches the value, so nothing jumps. **Relative** moves the value by as far as your finger moves.

## Running a layout from a foot controller

A foot controller, or any other MIDI device, can run a layout while it plays. Open **Layout…**, then **Pages and devices…**, and select **MIDI remote control**. Each rule says which message to listen for and what to do when it arrives: go to a page, press a control, set its value, turn it on or off, show or hide it, change pad grids, set the key or the scale, run a sequence, or panic. Press **Learn** on a rule and tap the switch on your controller to fill in the message. A rule's card lights up when its message arrives.

On a control's **Behavior** tab, **Start turned off** and **Start hidden** decide how it starts, so a rule can bring it in later. Stack two pad grids in one group and use **Show only this one of its group** to swap them in one spot. A page's **…** menu has **When this page opens**, to run a sequence each time you go to the page. Rules only work while the layout runs, or in Try mode. See [Run Windows MIDI Glass from a foot controller]({{ site.baseurl }}/kb/midi-glass-remote-control/).

## Trying a layout as you build it

**Try** turns the page on without leaving the editor. Every control sends for real, so you can check that a fader sends what you expect before you go any further. Ctrl+Enter switches back to **Edit**.

The **Sending** list under the page shows each message as it goes out. **Selected control only** narrows the list to the control you're working on. **Pause** freezes the list so you can read it, and **Clear** empties it. The arrow at the right end folds the list away to give the page more room.

## Pages

A layout can have as many pages as you like. The tabs under the page switch between them in the editor, and **+ Page** adds one. To let people change pages while the layout runs, put a **Page tab** control on each page.

Open **Layout…**, then **Pages and devices…**, to rename pages and change their order. To make a band, open a page's **…** menu there and select **Always on screen**. A band is a page whose controls show on top of every other page while the layout runs, such as a transport row, a panic button or your page tabs, so you only build it once. You don't switch to a band the way you switch pages, so leave room for its controls on your other pages. In the editor, you edit a band like any other page.

**Layout…**, then **Page size…**, sets how big the page is, in pixels. Pick a size close to the screen the layout will run on.

## Tempo

LFO and Steps controls run at the layout's tempo. To set it, open **Layout…**, then **Pages and devices…**, and select **Behavior**. A **Beat clock** keeps its own tempo.

To follow another device instead, such as your DAW, set **Tempo source** to **External MIDI clock** and pick the device under **Clock source**. The LFOs and Steps controls then follow the speed of the clock it sends. Until the clock arrives they use the tempo you typed, and if the clock stops they keep the last tempo they had. They follow only the clock's speed. They don't start or stop when the clock does, and they don't line up with its beat.

![The Tempo settings, with Tempo source set to Incoming MIDI clock, Clock comes from set to Synth, and a tempo of 120]({{ site.baseurl }}/assets/images/midiglass-tempo.png)

## Running a layout

To run a layout, click its card in the library, or select **Run** on the card. You can also double-click a layout file in File Explorer. The first time you do, Windows asks which app to open it with, so pick Windows MIDI Glass.

A running layout has its own window. The bar along the top has:

- **Page**, to switch pages.
- **Size on screen**: **Actual size**, **Fit to window**, or a custom size.
- **Always on top**, to keep the window in front of your other apps.
- **Full screen**. In full screen, everything on the bar moves behind one small button in a corner of the screen. It fades after a few seconds so it's out of the way. You can move it to another corner. Press Esc to leave full screen, or F11 to switch in and out.
- **Panic**, which stops every note on every device Windows MIDI Glass is sending to. Ctrl+Shift+P does the same thing. Use it when a note gets stuck.

You can move more than one control at once with more than one finger: two faders, or a fader and a pad.

The bar also says whether the devices the layout sends to are connected. If one is missing, it says **Waiting for** and the device's name. Plug the device back in and the layout picks it up again by itself.

If a layout runs for a long time while nobody touches the PC, such as one that only shows meters or plays a beat clock, Windows may turn the screen off or go to sleep. To stop that, select **…** (More options) in the library and turn on **Prevent sleep while running**.

## Devices

A layout keeps its own list of devices. Each one has a name, and the controls send to that name rather than straight to the hardware. So if you move to a different MIDI interface, you only change the device in one place, and every control follows.

Open **Layout…**, then **Pages and devices…**, and select **Outputs** to see the list. You can add a device, change which hardware one points to, and set how Windows MIDI Glass talks to it.

A device that isn't connected is normal, not an error. The library card says how many are missing, and the layout sends to the rest. The status bar in the library says whether the MIDI service is running. If it isn't, nothing can be sent.

## Themes, colors and backgrounds

A **theme** sets how the whole layout looks: the page behind the controls, how each kind of control is drawn, and six colors. Open **Layout…**, then **Theme and colors…**, to pick one. Changing the theme changes every control at once, and you can change it as often as you like.

Each control uses one of the theme's six colors, chosen on its **Look** tab. A control remembers which of the six it uses, not the color itself. So when you change the theme, the colors change along with it and still mean the same thing.

**Layout…**, then **Background image…**, puts a picture or a video behind the controls. You can set how it fits the page and how see-through it is.

Above the gallery, the page says which theme the layout uses and who made it. Select **Theme details…** next to it to see everything the theme's file says.

The theme gallery shows who made each theme under its name. **Group by**, above the gallery, sorts the themes by who made them or by who signed them. Right-click a theme to see **Theme details…**, or, for one of your own, **Export pack…**.

[How Windows MIDI Glass themes work]({{ site.baseurl }}/kb/midi-glass-themes/) covers every theme setting, where theme files go, and how to make your own.

## Your layout files

Layouts are saved in **Documents › MIDI Layouts**, one file per layout. The files end in `.midilayout`. A picture or a video you add to a layout is copied next to it, so the layout and its pictures stay together.

- **Back up now**, on a card's **…** menu, saves a copy of the layout in the **Backups** folder next to your layouts.
- **Restore…** puts a backup back. Windows MIDI Glass backs up the layout as it is now first, so you can change your mind.
- **Export pack…** makes one file that holds the layout and everything it uses. See [Sharing layouts and themes](#sharing-layouts-and-themes).
- **Duplicate** makes a copy to try ideas on. The copy says which layout it's based on.
- **Add to favorites** puts the layout in a **Favorites** section at the top of the library. Favorites belong to this PC, so a layout you share doesn't arrive as someone else's favorite.

To have an AI assistant build a layout for you, select **Ask an AI assistant…** in the library, next to **Open…**. It shows a starting prompt to paste into the AI assistant you use, such as a chat in your web browser. The prompt has the link to [Windows MIDI Glass layout files, a guide for AI agents]({{ site.baseurl }}/kb/midi-glass-layouts-for-agents/), the names of your MIDI devices, and the themes you have, so the layout the assistant makes can find your devices. Windows MIDI Glass doesn't send anything itself. If you'd rather not see it, open **Appearance and settings** and turn off **AI assistant**.

## Sharing layouts and themes

To give a layout to someone else, or move it to another PC, select **Export pack…** on its card's **…** menu. In the editor, open **Layout…**, then **Pages and devices…**, and select **Export…**. Windows MIDI Glass makes a **pack**: one `.midilayoutpack` file that holds the layout, every picture and video it uses, and the theme it uses if that theme is one of yours. To share a theme on its own, right-click it in the theme gallery and select **Export pack…**. That makes a `.midithemepack` file.

A pack lists every file inside it, with its size and a fingerprint called a SHA-256 hash. When someone imports the pack, Windows MIDI Glass checks every file against that list, so it can tell if anything was changed or added after the pack was made.

### Who made it

Each layout and theme can say who made it: a name, a company or group, a website, a license, a version, and whether AI was used to make it. Windows MIDI Glass fills this in for everything you make, from **Author profile…** on the library's **…** (More options) menu. Other Windows MIDI Services tools use the same info. Before you pack something, Windows MIDI Glass shows what it says and lets you change it.

To see what a layout says about who made it, select **Layout details…** on its card's **…** menu. For a theme, right-click it in the theme gallery and select **Theme details…**.

- **To change who made a layout,** open it in the editor and select **Layout…**, then **Layout details…**. The page has the layout's name and description, the fields that say who made it, and when and with what it was made. Changes save as you type, like everything else in the editor. **Open in editor**, in the library's **Layout details…**, takes you to the same page.
- **To say who made a new theme,** fill in **Author** when you select **Save as theme…**. A theme saved under a new name starts with your info.
- **To change who made one of your own themes,** select **Theme details…**. You can't change a theme that comes with Windows MIDI Glass, one from a signed pack, or one a newer version made.
- **If AI helped,** say so under **Source**, and pick how a person was involved under **Human oversight**. If an AI assistant made the layout for you, you can say you checked its work after you have.
- **To change your info,** select **Author profile…** on the library's **…** (More options) menu, or **Edit author profile…** on the editor's **Details** page. Your info is only filled in for what you make from then on. **Fill from profile** puts it into a layout or theme you already have.

Anyone can type any name into a file. So unless a signature backs it, Windows MIDI Glass shows the name with **(unverified)** after it.

### Signed packs

A publisher, such as an instrument maker, can **sign** a pack with a code-signing certificate. The signature proves who published the pack and that nothing in it changed since. It doesn't mean the layout is safe or that it works.

- **To sign a pack,** pick your certificate under **Signature** when you pack it. You need a code-signing certificate from a certificate provider, in your personal certificate store. A certificate on a hardware token or in a cloud signing service works when the provider's software puts it in that store. Add a **Timestamp server** from your certificate provider, so the signature stays good after your certificate runs out.
- **When you import a signed pack,** Windows MIDI Glass shows **Signed by** and the name on the certificate. Windows checks the certificate, including whether the company that issued it has withdrawn it. A pack whose signature doesn't match what's in it, or whose certificate was withdrawn, isn't imported.
- **A layout from a signed pack** says **Signed by** on its card for as long as its files are exactly what was signed. When you select **Edit**, Windows MIDI Glass offers to make your own copy, so the signed layout stays as it was published. The copy says which layout it's based on.

### Importing a pack

Select **…** (More options) > **Import pack…** in the library. For a theme pack, you can also use **Import theme…** on the **Appearance** page in the editor. Windows MIDI Glass shows what's in the pack, who made it, and whether it's signed, before it writes anything. If you already have the same layout or theme, it asks whether to replace it or keep both.

Windows MIDI Glass still imports the `.zip` layout packages that earlier versions made.

### Layouts from a newer version

A layout made by a newer version of Windows MIDI Glass says **From a newer version** on its card. You can run it, and you can open it in the editor to look around and try it. Windows MIDI Glass won't save changes to it, so nothing the newer version added gets lost. That's also why **Duplicate**, **Rename…** and **Edit description…** are turned off for it. A control this version doesn't have shows as a dashed outline with its name. It doesn't respond to touch and doesn't send anything. To change the layout, update Windows MIDI Glass.

## Keyboard and screen readers

On a running layout, the Tab key moves from control to control, and a screen reader reads each control's name. The **keyboard order** decides the order Tab moves through the controls, and the order a screen reader reads them.

To set it, open **Layout…**, then **Tab order…**, and click the controls in the order you want. **Keyboard order**, under the page, sets the order from where the controls are on the page. You can also move a control up or down in the **Outline**.

Open **Layout…**, then **Pages and devices…**, and select **Accessibility check** to see whether the colors are easy to tell apart and whether every control has a label a screen reader can say.

## Learn more

- [How Windows MIDI Glass themes work]({{ site.baseurl }}/kb/midi-glass-themes/)
- [How to control your DAW with Mackie Control in Windows MIDI Glass]({{ site.baseurl }}/kb/midi-glass-mackie-control/)
- [How to make a floating toolbar or palette in Windows MIDI Glass]({{ site.baseurl }}/kb/midi-glass-floating-toolbars/)
- [Windows MIDI Glass layout files, a guide for AI agents]({{ site.baseurl }}/kb/midi-glass-layouts-for-agents/)
