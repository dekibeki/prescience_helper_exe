# Prescience Helper

## Info

This is designed to help you prescience the best people you can!

This is the software component of [this addon](https://www.curseforge.com/wow/addons/prescience-helper).

There are prebuilt binaries in the releases to the right if you can't/don't want to build it yourself.

It generates the input strings for the addon by parsing and keeping a database of all logs on your computer. This means you'll have to log your own fights.

## UI

![the ui](https://github.com/dekibeki/prescience_helper_exe/blob/main/doc/prescience_helper_ui.png?raw=true)

The UI may start smaller than necessary to show all controls, please resize it so that you can see all options (including the encounter selector)

1. Use this button to select where your logs are located
2. This text shows you the currently selected location that is searched for logs
3. This is used to specify the minimum number of players to have as alternatives for each time. For example, if this value is 8, the best 8 choices for prescience will be known at any time. In an ideal world you would only need 4, but sometimes arms warriors die.
4. Select the encounter difficulty
5. Select your encounter. This is populated from encounters found in your logs. If you don't have logs for a particular encounter, they won't be in the drop down selector.
6. Paste the output string of the addon into here
7. Copy the input string to paste back into the addon from here
8. This gives information about the current input generation. This can include information on errors and when it succeeded.
9. This gives information about the current parse. This will include when it is parsing and when it has finished parsing something.

## Basic idea behind the software

This software will constantly scan your logs directory for new logs, and put them into the database "prescience_helper.db" which will be placed in the working directory (generally the same directory as the software).
Then when given an addon output string and what fight you're doing, it will look through all of those logs and find the people doing the most damage at each point of time.
This information will then be encoded into a string that is given to you to use with the addon.
