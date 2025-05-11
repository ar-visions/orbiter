# Orbiter UI Guidelines

Orbiter is a design-first development platform for building rich, spatially-aware native applications. It prioritizes user control, clarity, and a minimalist interface that adapts fluidly based on user behavior. The philosophy of Orbiter is rooted in respecting how people *actually* want to interact with their environment—not just how tools have conditioned them to.

---

## Philosophy

* **User-led Layout**: Orbiter avoids rigid layout constraints. The user dictates view positioning. Layout adapts based on top level configurable constraints -- which amount to more permutability than other code editors.

* **Minimal UI Permuting**: All UI must justify its presence. Interface elements like file trees, for example are constrained to 1 depth.  To do so, we have just two icons that show up when there are multiple depth levels.  If the project does not have multiple depth per project, this will not present.  One may expand multiple directive views with a singluar list, and feel natural doing so.  The critical aspect of this control is it does not have to be resized constantly -- effectively paramount for the user.

* **Declarative Roots, Mutative Flow**: Orbiter uses declarative composition for default UI state where users control the view layout afterward. No re-declaration needed for each frame.  This is not a React-based app, but rather something that can encompass more customization by user.

* **Scene is Primary**: The editor is the central view. Most advancements to interface are within the editor. What top-level remains is simple layers with child layout types.  We only have a file browser, terminal, and in design mode: properties, and toolbox

* **Text-Based Styling**: UI styles are specified in `.css` files named after the app. These files define declarative visual behaviors and transitions and are read during app setup. Style belongs in text.

---

## Paneling & View Design

* **Top Toolbar**: Static. Contains major actions like Build, Start/Stop, Layout options. Persistent across sessions; saved in directory its viewing.
  * We want to combine the idea of 'workspace', 'config' -- its a project file with properties for Orbiter, but its in a .css format, better than json!
  * Its a default build template in .css

* **Docking**: Views can dock on any edge (top, right, bottom, left) and snap. Drag-based layout flow allows views to auto-fit relative to one another.

* **Folder Navigation**:

  * Flattened dropdowns represent folders.
  * The `...` icon allows navigating up.
  * No tree view: subfolders are revealed explicitly.
  * Folder title only shows when sub-items exist.

* **Spacing & Transparency**:

  * Global spacing and transparency settings allow the Earth scene (or other background) to be visible.
  * Spacing is consistent between elements to preserve visual peace.

---

## Style & Behavior

* **Incidental Transitions**: Animations occur subtly as a result of layout rules, not user declarations.

* **No Explicit IDs**: UI components do not require `id` tags. Variable names act as identity anchors.

* **Event System**: Events are handled naturally by the system based on structure, not identifiers.

* **One Layer Rule**: The `layer` component governs layout. Items without `x`, `y`, `w`, or `h` are automatically placed via flow layout (grid, row, or column).

---

## Developer UX

* **Design Simplicity First**: Start by building what works and makes sense. Avoid over-engineering layout systems too early.

* **Render Once, Then Let Go**: Orbiter prefers initial declarative state. Components remain live and mutable afterward.  The user composes it -- we do not.  This means its less code to do a more effective job at that.

* **Direct C / A-type Syntax**: UI is built using native objects and method calls. Events do not require arbitrary amounts of variables in scope, and do not create code pyramids.

* **CSS App Property Config/Themes**: All configuration and styling is declarative and externalized in file-system, extensible with user-config via .orbiter/config.css. The system reads `.css` files named after the app ('Orbiter') and loads them automatically.  Additionally, the user may set themes property to apply any amount of css styles afterwards.  This property is named styles, a comma separated list to apply after 'Orbiter' style.

---

## View Switching

* At the heart of Orbiter is a **view switch system** driven by the file currently in focus. This system allows Orbiter to adaptively generate visual interfaces based on the active file type or associated adapter.
* Each adapter can register custom views (e.g., editor, graph, texture view), and Orbiter will present these to the user automatically.
* Different file types may have multiple associated views (e.g., `.json` might offer table + raw view).
* View switching is **fluid and unobtrusive**, in main menu (dockable).

---

## Summation

Orbiter isn't trying to be a web browser, or let you try to develop like an app for one of them. It's a native-first, code-centered system built to empower how developers *actually* build, not how frameworks expect them to.  We use tapestry, so any git-url that installs to lib/bin/include/share is a package.
