# Menu Layout Editor

Customize your main menu with **Gora22231g & Igch7**.

Move buttons and titles, line up nearby elements, or split a logo into independent pieces. Save your layout and pick up where you left off next time.

## Getting started

Open **Settings > Graphics > Menu Editor** from the main menu. On your first launch, an interactive guide highlights each button while blurring the rest of the screen. Click the highlighted button to continue, or choose **Skip guide**. The guide remembers completion or dismissal. If the blur shader is unavailable, the background is dimmed instead.

## Build your layout

- **Move:** click an element and drag it. **Prev / Next** also select hidden elements.
- **Snap: ON/OFF:** align nearby edges and centers. Pull farther away to detach, or turn snapping off for free movement.
- **Cut V / Cut H:** select an element, choose a cut direction, then click where the cut should go. Both pieces can be moved separately.
- **Crop:** select an element, click Crop, and drag a rectangle over the area you want to keep.
- **Hide/Show:** hide or reveal the selected element or piece.
- **Panel:** move the toolbar to the bottom or top so it does not cover your work.

For example, select the Geometry Dash title, choose **Cut V**, and click between the words. Switch to **Move**, then position Geometry and Dash separately.

## Share a layout

- **Export CFG** writes the current layout to a JSON file through a system save dialog.
- **Import CFG** opens a JSON config as a preview. Press **Save** to keep it, or **Cancel** to restore your previous layout.
- **Save folder** opens this mod's Geode save directory. Config exports default to its `exports` subfolder.

Configs include positions, cuts, crops, visibility and Snap, without changing tutorial completion. Unknown menu elements are ignored; configs with no matching elements are rejected. Limits: 1 MiB, 512 original elements and 64 pieces.

## Save and restore

- **Save** keeps positions, cuts, crops, visibility and the Snap preference between launches.
- **Cancel / Esc** discards changes from the current editing session.
- **Restore** rebuilds the original element and resets its position. Selecting any of its pieces restores the whole element.
- **Show all** reveals existing elements and pieces without undoing cuts.
- **Reset all** restores the original layout and removes every cut and crop.

The **RobTop logo is protected** from hiding, moving, cropping and splitting.

## Notes

Cut pieces are static images; their original animation or changing text does not continue. Pieces forward the original button action while that button remains attached, running and enabled. Restore brings the live original back. The background and animated game layer are not editable. Snapshot textures are shared per original element and limited to a 64 MiB budget. Other menu customization mods may conflict.

Positions, cuts, Snap and the completed-guide flag are saved in `menu-layout.json` inside the mod's Geode save directory. After successful completion or dismissal, the guide stays closed on future launches. Previous Geode saved values are migrated when the new file does not exist.

Supported build: **Windows x64, Geometry Dash 2.2081, Geode 5.10.1**.

