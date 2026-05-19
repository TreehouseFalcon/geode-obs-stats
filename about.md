# OBS Stats

If you're a streamer or content creator using OBS, this mod lets you show real-time stats from your Geometry Dash profile!

## Getting started

To link this mod with your OBS, follow these steps:

1. Open OBS on your Windows or macOS device
2. Open "Tools" from the top left menubar and select "WebSocket Server Settings"
3. Check the box that says "Enable WebSocket server"
4. Set a "Server Password" (or click Generate Password)

- note: this step is very important or else malicious applications on your computer can control anything on OBS

---

## Available stats

Below you can find the full list of stats you can display. Placing any of the following placeholders into the "formatted text" input will show the corresponding stat on the selected OBS text source.

### Currency stats

- `{STARS}`: Total number of stars
- `{MOONS}`: Total number of moons
- `{DEMONS}`: Total number of demons
- `{SECRET_COINS}`: Total number of secret coins
- `{USER_COINS}`: Total number of user coins
- `{ORBS}`: Current orb count
- `{TOTAL_ORBS}`: Total number of orbs collected
- `{DIAMONDS}`: Total number of diamonds
- `{KEYS}`: Current key count
- `{LIST_REWARDS}`: Total number of list rewards collected
- `{INSANES}`: Total number of Insane levels completed
- `{CREATOR_POINTS}`: Total number of creator points

### Level stats

- `{COMPLETED_LEVELS}`: Number of completed Auto levels
- `{COMPLETED_EASYS}`: Number of completed Easy levels
- `{COMPLETED_NORMALS}`: Number of completed Normal levels
- `{COMPLETED_HARDS}`: Number of completed Hard levels
- `{COMPLETED_HARDERS}`: Number of completed Harder levels
- `{COMPLETED_INSANES}`: Number of completed Insane levels
- `{COMPLETED_EASY_DEMONS}`: Number of completed Easy Demon levels
- `{COMPLETED_MEDIUM_DEMONS}`: Number of completed Medium Demon levels
- `{COMPLETED_HARD_DEMONS}`: Number of completed Hard Demon levels
- `{COMPLETED_INSANE_DEMONS}`: Number of completed Insane Demon levels
- `{COMPLETED_EXTREME_DEMONS}`: Number of completed Extreme Demon levels

### Global rank stats

- `{STARS_GLOBAL_RANK}`: Your rank on the global stars leaderboard
- `{MOONS_GLOBAL_RANK}`: Your rank on the global moons leaderboard
- `{DEMONS_GLOBAL_RANK}`: Your rank on the global demons leaderboard
- `{USER_COINS_GLOBAL_RANK}`: Your rank on the global user coins leaderboard
