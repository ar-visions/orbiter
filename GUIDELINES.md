# Orbiter UI Guidelines

Orbiter is a design-first development platform for building rich, spatially-aware native applications. It prioritizes user control, clarity, and a minimalist interface that adapts fluidly based on user behavior. The philosophy of Orbiter is rooted in respecting how people *actually* want to interact with their environment—not just how tools have conditioned them to.

---

## Philosophy

* **User-led Layout**: Orbiter avoids rigid layout constraints. The user dictates view positioning, floating, docking, and snapping. Layouts adapts based on top level configurable constraints -- which amount to more permutability than other code editors.

* **Minimal UI Noise**: All UI must justify its presence. Interface elements (like file trees) are condensed to dropdowns or dynamic presentations only when needed.

* **Declarative Roots, Mutative Flow**: Orbiter uses declarative composition for initial state but allows user-driven mutation of views afterward. No re-declaration needed for each frame.

* **Scene is Primary**: Code is the central view. Most UI design is handled through the code editor. Decorations and layers support, not distract from, code-focused workflows.

* **Text-Based Styling**: UI styles are specified in `.css` files named after the app. These files define declarative visual behaviors and transitions and are read during app setup. Style belongs in text.

---

## Paneling & View Design

* **Top Toolbar**: Static. Contains major actions like Play, Debug, Layout options. Persistent across sessions.

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

* **Render Once, Then Let Go**: Orbiter prefers initial declarative state. Components remain live and mutable afterward.

* **Direct C / A-type Syntax**: UI is built using native objects and method calls. No JSX, no brackets. Just data.

* **Text-Based Stylesheets**: All styling is declarative and externalized. The system reads `.css` files named after your app and loads them automatically.

---

## View Switching

* At the heart of Orbiter is a **view switch system** driven by the file currently in focus. This system allows Orbiter to adaptively generate visual interfaces based on the active file type or associated adapter.
* Each adapter can register custom views (e.g., editor, graph, texture view), and Orbiter will present these to the user automatically.
* Different file types may have multiple associated views (e.g., `.json` might offer table + raw view).
* View switching is **fluid and unobtrusive**, living near the bottom tab bar or in the top menu as needed.

---

## Final Thought

Orbiter isn't trying to be a web browser in disguise. It's a native-first, code-centered system built to empower how developers *actually* build, not how frameworks expect them to.
