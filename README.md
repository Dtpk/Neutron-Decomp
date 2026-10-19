<div align="center">

#  N E U T R O N 
### *Modern C++ Linux Port of the Classic PSP Arena Game*

</div>


So using ghidra headless I attempted to rip the psp game neutron apart. 

<table>
  <tr>
    <td align="center"><img width="480" height="272" alt="PIC1" src="https://github.com/user-attachments/assets/5aaef792-753c-467d-86ce-09325720f19c"></td>
    <td align="center"><img width="480" height="272" alt="Screenshot_20261017_042435" src="https://github.com/user-attachments/assets/d5341df6-2cb2-49b3-8e94-3377210d3984"></td>
  </tr>
</table>

Current iteration on the right its a far call from being a direct port but the maps are in and player mechanics almost in. The camera fully broken right now. And a lot of calls are not fully figured out.


## 🛠️ Project Status & Development Log

### Current State
The current iteration is a far cry from a clean 1:1 direct port, but progress is moving forward:
* **Levels/Maps:** 100% in—all 25 original maps have been successfully brought over.
* **Player Mechanics:** Mostly functional, though some engine calls are still being sorted out.
* **The Camera:** Currently completely broken. The view toggled by `V` isn't how the game is supposed to play. I'm actively working on implementing a **Super Monkey Ball-style world-tilting camera** where the world pans to follow the player, just like the original.
* **The Menu:** Currently redone and not a 1 to 1 and stage select has some of the real psp logic still in place looks wrong with the new menu.
---

### 📜 Project Background
You can find the original PSP game archived on the Internet Archive. The **sole purpose** of this decompilation attempt is to prevent it from becoming **lost media**. Outside of the archive, it's nearly impossible to find, and after years of silence from the original developer regarding a Linux port, I decided to take matters into my own hands out of pure love for this game.

**How it's being built:** 
I've kept whatever clean code I could manually uncover from Ghidra. When I fed parts of it to an LLM to help make sense of the rest, it inevitably hallucinated a few things. While some of those hallucinations actually ended up working out (and I might keep them), most of it needs to be scrapped if I want a true 1:1 parity without the PSP's hardware slowdowns and reduced graphics. If I lean on LLMs moving forward, I'll use better tooling and potentially reference the PSP SDK to help better decode the original MIPS binary.

> **ANY AND ALL HELP — LLM OR OTHERWISE — IS MASSIVELY APPRECIATED!**

---

### 🎵 Game Audio & Music
I honestly can't fully remember the original soundtrack from playing it on iOS back in the day, and the PSP version's audio assets were largely stripped or scrapped, so I may never know for sure. 
* I plan to reach out to folks listed in the staff credits—maybe someone still has the source files. 
* If not, I'll bake a media player function into the game so custom tracks can be used. (Honestly, I'm still not even 100% sure if the PSP version ever had active background music or if it was purely sound effects).

---

### 🚀 Roadmap & Final Stretch Goals
While a lot of the initial experimental code is messy, the core foundation is landing. Here is where things are heading:

* **Core Playability:** Finalizing player movement, collision, and getting the camera tracking right.
* **Customization & Features:** Adding color-selection schemes (similar to other classic *Tron* projects).
* **Ports & Web Deployment:** Turning this into a WebAssembly browser game, plus opening the door for future ports like **VR** or various console architectures by getting the engine fully mapped back into modern C++.
* **Nostalgia & Performance:** I've been playing this on a PS4 via a PSP emulator and on my physical PSP for years just to kill time. The goal is to finally ditch the emulator overhead and get true native performance out of the game—similar to the old iOS release, but packed with modern quality-of-life features.

---

### 📦 Trying the Build
I may include a pre-compiled experimental build in a `build/` folder. If you want to try launching it locally on Linux, run:

```bash
./neutron
