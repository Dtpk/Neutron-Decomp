These ports include online/lan multiplayer for both linux/android/windows wiith cross play thanks to same random plane spawn generation. Prior the windows one lost sync and android and linux just by chance happened to share the same dice rolling this was a head scratcher for a bit.

They also include splitscreen and single player with touch controls and controller inputs.

Some limits I hit sounds... Sadly travel sounds got desyncd even when gameplay was smooth so I kept the explosion sounds they were fine and music. I will keep working at this but no promises.

Besides that to host online you need to port forward or use tailscale. If you and friends are on the same network nothing is needed no port-fowarding at all you should just see the host and connect from the lobby with no issues.

Tested up to 5 phones of differnt generations using the oldest and newest to host. Older phones might hitch the server so be mindful but if my old zte trackphone was able to keep up with minor hitching it can really run on anything modern.

Techinally two ports can be fowarded but only one is needed "tcp 27772" if your hosting online port foward that then anyone you want to connect give your public ip too obviously do this with who you trust or spin up a cloud server or use something like tailscale.

You can touch type a name in for your player no restrictions and the ip. It is a bit tricky just tap around the fields and the android keyboard should come up but if you dont wanna type a ip and are doing lan just hit continue. If you don't want a name just hit continue I made sure this will work without keyboard input or just using a controller.

---

Questions Answered:
1. Why did you do multiplayer like this.. Answer for future proofing alot of older games follow this strat and I wanted to keep it simple stupid its also less for me to maintain.
2. Was a llm used for these ports.. Answer mostly but I did peer review and test fully on my end so any bugs be sure to let me know.
3. Did you attempt to do this implentation yourself.. Answer yes I took splitscreen at first and duplicated that which lead to only 4 player and tons of desync this newer implentation is able to go up to 12 or higher.
4. What happens when players join.. Answer if too many join maps get gated off the smaller maps like cube hold up to 6. If custom maps are used you download the host copy. Players can drop out and in seamlessly.
