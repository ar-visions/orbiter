# Orbiter - a different form of navigation
Orbiter redefines navigation by replacing traditional tabbed interfaces with a dynamic stack of code panes, each representing a focused context—like a module, function, or symbol  User holds Shift to rearrange these panes in the scroll/mini-map area, where each region is color-coded for clarity. With this design, Orbiter is a bit more in sync with your mental workspace.  AI is us, so our workspace should be a reflection of us.

# Large workflow maintenance for the user?
It's certainly deliberate management, but less than tabbed user-interfaces. The tabbed interface disrupts flow by shuffling your context around into buttons that Inherently move with constant additions.

Not every file you open goes into workspace (unless it's shift-enter or shift-click).  The primary mechanism
is to bring up the file in the alt pane (on the right side by default).  We suppose it's a bit like Xcode
assistant, however you are your own butler here.  You choose what goes there.  The 'assistant' is Orbiter's code-nav, explained below:

# an AI PC attempt
Orbiter is an open source attempt at an AI-PC application that runs on existing PCs.

We run hyperspace modeling to sync the users eyes and voice with the editor.
The words we are looking at (long with the actual word) is given in to LLM queries.
It is to support both local and remote LLM integration into your particular stack.

We create this stack not only because its easier to deal with, but to also architect a better navigation model, to find to the code we seek.  Dictation-based code-nav is the  'how do I get to this' solution.
To use, tell Orbiter where you want to go, and it will scroll to the place you need to be in workspace. 
Whisper is the audio modeling used here, and the navigation model is to be openly trainable.

With AI running the appropriate modeling, we may navigate better and thus produce better.  Orbiter is being
built to alleviate the stress of constant code navigation; to take that weight off the user and delegate it
to transformer weights.

Lets see what the Orbiter agent looks like:
![Orbiter](orbiter888.png)


# Work in progress
- Frosted and composed UX
    - Vulkan background render (Orbiting Earth, with multiple layered NASA data view of our home planet)
    - Multiple Skia canvases (or canvai) that blend together with blur layers to compose a frosted view
        - Overlay (images, icons, text, anything with explicit color/alpha output)
        - Compose (the amount of frost to allow to come through, and what level of blur (3 to choose from))
            - 0/2: None
            - 1/2: Low Blur
            - 2/2: High Blur
        - Colorize
            - (r/g channels): HSV filter on the composer blend
            - (b): An amount to blend into final output
    - Rich component system that make use of these layers
- Simplified Editor
    - 
- Item two
  - Sub-item (indent with two spaces or a tab)
- Item three