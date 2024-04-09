#include <ux/app.hpp>

using namespace ion;

struct orbiter:Element {
    struct props {
        int sample;
        callback handler;
        ///
        properties meta() {
            return {
                prop { "sample",  sample },
                prop { "handler", handler}
            };
        }
    };
    
    ///
    component(orbiter, Element, props);
    
    ///
    void mounted() {
        console.log("mounted");
    }

    /// if no render is defined, the content is used for embedding children from content (if its there)
    /// if there is a render the content can be used within it
    Element render() {
        return button {
            { "content", fmt {"hello world: {0}", { state->sample }} },
            { "on-click",
                callback([&](event e) {
                    console.log("on-click...");
                    if (state->handler)
                        state->handler(e);
                })
            }
        };
    }
};

/// top level application renderer in main()
/// args flow into app instancing which upon
/// operator int will wait for app to return
/// its exit-code
int main() {
    return app([](app &ctx) -> Element {
        return orbiter {
            { "id",     "main"  }, /// id should be a name of component if not there
            { "sample",  int(2) },
            { "on-silly",
                callback([](event e) {
                    console.log("on-silly");
                })
            }
        };
    });
}
