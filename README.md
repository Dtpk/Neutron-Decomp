<div align="center">

#  N E U T R O N 
### *Modern C++ Linux Port of the Classic PSP Arena Game*

</div>


So using ghidra headless I attempted to rip the psp game neutron apart. "FUNCTIONAL BUILD IS LIVE YOU CAN PLAY IT!!!"

<table>
  <tr>
    <td align="center"><img width="480" height="272" alt="PIC1" src="https://github.com/user-attachments/assets/5aaef792-753c-467d-86ce-09325720f19c"></td>
    <td align="center"><img width="480" height="272" alt="Screenshot_20261024_005239" src="https://github.com/user-attachments/assets/ebfe3b83-aca6-4956-a81b-8a02dba6b107" /></td>



    
  </tr>
</table>


Map editor and in game audio stock sounds included for now.. I think the multiplayer build I might try to use the original menu for if I can recover that. "Control remapping is also later" To use map editor make the map in the editor then place the map.txt next to the game and it will let you play the custom map as the 26th stage so beat the game or use the cheat code. Excuse the compression I had to do to fit a video here on github this is a older build video tutorial is now in with its same quirk of not actually erasing the saves its not a bug. 

"Will update the release for windows for map editor and the game tomorrow sometime hopefully"


https://github.com/user-attachments/assets/e3f8d0e0-c6ed-406a-9c6a-30b88ea33edc


"The player spawning side ways is normal it's in original although just for a frame not as dramatic as is here and scoring being broke as well is a original thing.I kept both in may fix scoring or hide the player "red blue" guides till last second like it appears on psp have yet to decide on this front."

Preview of the new map editor will replace with a video

<img width="720" height="480" alt="image" src="https://github.com/user-attachments/assets/add0da31-f09d-4696-9f87-20c7005e2429" />



---





## 🛠️ Project Status & Development Log

### Current State
The current iteration is a far cry from a clean 1:1 direct port, but progress is moving forward:
* **Levels/Maps:** 100% in—all 25 original maps have been successfully brought over.
* **Custom/Maps:** A new map editor is made put the map.txt next to the game and congrats a 26th stage of your making.
* **Player Mechanics:** Mostly functional, though some engine calls are still being sorted out and moments where you can go through your line on map edges. Movement has been fixed latest build is playable still needs a lot of work!!
* **The Camera:**  UPDATE!! This is some what working now I think this has finally been figured out and is 1 to 1 but needs further testing. Potential other modes might be added. Further testing I see some slight differences but I think I can get them fixed easy.
* **The Menu:** Currently redone and not a 1 to 1 and stage select has some of the real psp logic still in place looks wrong will get old menu going and have a toggle for a new menu if wanted.
* **ENEMIES:** The spawning of enemies as well as rank is correct. Orange enemies should be white but I will fix that last!!! It almost looks right though so I wanna check existing video then I will change it last since all enemy colors are fine right now including the random spawn logic and difficulty progression. "might need some tuning yet"

---

### 📜 Project Background
You can find the original PSP game archived on the Internet Archive. The **sole purpose** of this decompilation attempt is to prevent it from becoming **lost media**. Outside of the archive, it's unlikely the original source will resurface and getting a ipod touch with the older software needed is a challenge even finding the ios game is a challenge starting to become impossible since it needs side-loading. "And it been about 3+ years since I got in contact and heard the mac os version might still exists and still hasn't been put up either but it would have been for really outdated mac's"

So preserving the game logic is the main goal. A rename is also in question to not confuse this work with any future code that becomes available "Neutron Next" Or Neutron Remastered" or "Neutron Re-Ported" etc if you have a idea let me know.

**How it's being built:** 
I've kept whatever clean code I could manually uncover from Ghidra. When I fed parts of it to an LLM to help make sense of the rest, it inevitably hallucinated a few things. While some of those hallucinations were useful as a starting point, I try to verify everything manually. And I will add comments for what does what as I go on with this.

> **ANY AND ALL HELP — LLM OR OTHERWISE — IS MASSIVELY APPRECIATED!**
---

### 📚 Reference Material And Surviving Videos And Links Game History

Only surviving videos of IOS/MAC/LINUX builds is from this channel which is more then 17 years old now.


"https://www.youtube.com/watch?v=_PASOl8KDZA"

The only surviving web pages for this are for the psp port and what is on way back machine listed below.

"https://notboolean.wordpress.com/"

"https://web.archive.org/web/20110208013150/http://www.neutron-game.com/"

"https://www.gamebrew.org/wiki/Neutron_PSP"

"https://web-archive-org.translate.goog/web/20250215060552/https://psp.scenebeta.com/noticia/neutron?_x_tr_sl=auto&_x_tr_tl=en&_x_tr_hl=en-US"

Sadly none of the above were viable for getting any original source but were great none the less towards rebuilding this. "I left a comment on the youtube video more then 3 years ago and the mac version that may or may not be around never seemed to have surfaced or been put up but I did reach the original dev back then so their is hope"


---

### 🎵 Game Audio & Music
I honestly can't fully remember the original soundtrack from playing it on iOS back in the day, and the PSP version's audio assets were largely stripped or scrapped, so I may never know for sure. 
* I plan to reach out to folks listed in the staff credits—maybe someone still has the source files. 
* If not, I'll bake a media player function into the game so custom tracks can be used. (Honestly, I'm still not even 100% sure if the PSP version ever had active background music or if it was purely ambient sound effects.)
* If I can't contact any old staff members I will search YouTube I seen one or 2 videos back in the day of this game its what helped me remember the name of the game when I forgot it after my ipod was stolen. I hope they had sounds if not above plan is what I will take.

* ** So the music I found a few sources of original game and gameplay all on a channel that is more then 17 years old it either had no audio or was uploaded without audio I am assuming the game had no sound from this point on. Also the website on way back machine list playing your own songs or listening to theirs and multiplayer which was never added" **
---

### 🚀 Roadmap & Final Stretch Goals
While a lot of the initial experimental code is messy, the core foundation is landing. Here is where things are heading:

* **Core Playability:** Finalizing player movement, collision, and getting the camera tracking right.
* **Customization & Features:** Custom backgrounds effects etc...
* **Ports & Web Deployment:** Turning this into a WebAssembly browser game, plus opening the door for future ports like **VR** or various console architectures by getting the engine fully mapped by then.
* **Nostalgia & Performance:** I've been playing this on a PS4 via a PSP emulator and on my physical PSP for years just to kill time. The goal is to finally ditch the emulator overhead and get true native performance.
* **Touch Controls And Remapping:** Wanna have controller support in full keyboard and mouse and touch screen and mouse controls.
-* **Some how making a custom map tool:** Making a tool to make or import custom stages would be fun but this game is really held together with glue and spit so it will be a very last stage en-devour current idea is a separate file generated on game launch it checks and the tool can fill it with maps this gets around rebuilding the game for new maps.-
* * **Game blending:** Take the final port and instead of the ships we use other opensource things with your official dvds of coarse like halo ce grunts running with plasmas behind them instead of the dots and on collision explode and for enemy types it shows different covenant up to flood types as you progress just some ideas for what you can do later on.
---

### 💾 Updates
* **New builds** I started to use a psp helper script with ghidra. "Didn't know about these at the time" I got lucky not using them and hopefully now that I did, even more progress can be made on the project. Playable builds are in the builds folder it isn't a full release a lot is still off and needs tweaks but it is playable as of right now.

UP UP DOWN DOWN LEFT RIGHT ENTER Congrats if you made it this far down the read me. Here are all the maps free of charge enjoy the cheat code useful for wanting to play the custom map which is stage 26 without wanting to play through whole game.

*  **Build Notes** I plan to have a different ui and menu that the player can pick for future modes and stuff that might not fit well in the older menu we will see. The new menu will feature random cpu battles while the camera pans around is my idea... Enemies might need tweaked and framerate is being worked on but it is stable no slow downs unlike original but could be improved more. 

* **Separate Versions** Last minute thing but to prevent scope creep I will have a trimmed down or separate version not to ruin what I have for multiplayer "till that is tested and can be merged" this includes controller remapping and touch support which wasn't in the og psp version if you can tell...

### 📦 Trying the Build
I may include a pre-compiled experimental build in a `build/` folder. If you want to try launching it locally on Linux, run:

```bash
./neutron or ./run.sh
```
