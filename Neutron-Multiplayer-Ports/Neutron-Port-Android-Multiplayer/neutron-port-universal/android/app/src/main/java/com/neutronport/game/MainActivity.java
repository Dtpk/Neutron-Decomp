package com.neutronport.game;

import org.libsdl.app.SDLActivity;

// SDL2 and SDL2_mixer are statically linked into libneutron.so (see the
// root CMakeLists.txt's ANDROID branch), so this only needs to load that
// one library -- not "SDL2" + "main" separately like the stock template.
public class MainActivity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] { "neutron" };
    }
}
