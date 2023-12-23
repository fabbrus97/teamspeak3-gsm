[![stability-wip](https://img.shields.io/badge/stability-wip-lightgrey.svg)](https://github.com/mkenney/software-guides/blob/master/STABILITY-BADGES.md#work-in-progress) [![stability-experimental](https://img.shields.io/badge/stability-experimental-orange.svg)](https://github.com/mkenney/software-guides/blob/master/STABILITY-BADGES.md#experimental)

# Teamspeak GSM bot

This is a Work In Progress/Experimental project. 

## Dependencies
Teamspeak 3 client, gcc

## Compile
In `ts3client-pluginsdk/Makefile`, edit the `OUTPUT` variable, with the `plugin` path of an installed TS3 client.

Then, compile with:
```
ts3client-pluginsdk :-$ make
```

