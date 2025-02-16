view construct

[ ] vk subsystem that allows us to draw, and share texture with orbiter process.
    - both parties update memory based on A-type object and share this
    - a linked list is a good solution so list-types instead of arrays for messages
        - we do not want the hassle of reallocating
[ ] skia integration
        skia easily instances from Vk
            * revert from Dawn instancing code in the port of ion's skia.cpp/canvas.cpp

