# TS6-Soundpack-linux
Adds soundpack support to TeamSpeak 6 via an LD_PRELOAD

## What are the deetz?
You'll take the soundpack.so, put it somewhere (probably next to your TS6 binary), and load it with the LD_PRELOAD env variable when launching TeamSpeak
```
LD_PRELOAD=/path/to/soundpack.so ./TeamSpeak
```
You'll also want some soundpacks, this works with TS3 soundpacks placed as a folder next to TeamSpeak's default soundpack
```
TS6/html/client_ui/sound/default/
TS6/html/client_ui/sound/MicrosoftSam/
TS6/html/client_ui/sound/SMW/
```
The soundpack.so and soundpack-gui determines which pack to use by reading a config file, this file will be created when you first launch TeamSpeak with soundpack.so
```
~/.config/TeamSpeak/soundpack.conf
```
You can totally create a config file yourself ahead of time if you want, its a really simple format
```
pack=SMW
ts_path=/home/hunter/TS5
```
soundpack-gui, once pointed to your TeamSpeak directory, can be used to change what soundpack is currently being used.

![gui screenshot](https://i.imgur.com/FDmF80M.png)

You can attach a logo to your soundpack by including a 256x256 pack.png in the folder. If it's not already self explanatory there's some sample sound packs included in this repo

## How does it work?
idk lol; I'm embarassed to say, but this was mostly GPT's work. I don't know how to hook and redirect file reads in C
You could probably look at the code in this repo and understand it better than I do
