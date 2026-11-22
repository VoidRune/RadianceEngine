# Hub Rework Brief: Spellbrawl's Lobby Island

## What I want from you

I'm a solo developer making a multiplayer PvP wizard brawler in my own engine. I've just reworked all four PvP arenas and they look great. Now the **hub** (the social lobby where players wait between matches) needs to be rebuilt to the same standard. I'm willing to throw the current one away completely.

Please give me:

1. **3 to 5 distinct concept directions** for the hub. For each one, give the theme, the visual identity, the "hook" a player remembers, and how it ties into the rest of the game.
2. **A layout proposal** for the strongest concept: what sits where, how players move between areas, and the sight lines from the main gathering spot.
3. **Activities that make waiting fun**, both solo and with others. The hub must never feel dead.
4. **Ambient and dynamic life:** things that move, change, react and happen on timers.
5. **Easter eggs and secrets**, from tiny gags to multi-step mysteries.
6. **The training room design.** Details are below.
7. **Image-generation prompts** I can use to make concept art for each direction.
8. A rough **build order** (what to do first so the hub is playable early).

Be creative and opinionated. I'd rather have a few bold ideas than a long safe list. If you think one of my requirements is wrong, say so.

---

## The game

**Working title: Spellbrawl.** It's a top-down online party brawler for up to 12 players, where wizards fight with spells on floating, crumbling or hazardous arenas. Think *Warlock* (the Warcraft 3 custom map), *Wizard of Legend* and *Magicka*, mixed with roguelite card drafting. It's aimed at a Steam release, using Steam lobbies, invites and achievements.

### How a match plays
- Each player picks **3 spells** from a pool of 14. Every player also has a basic **Ember Bolt** on left click and a **dodge roll** on Space. Spells go on the right mouse button, Q and R. Aim follows the mouse cursor.
- There are two rule sets:
  - **Damage mode** (the default) has HP, mana and kills, with credit for the most damage plus assists.
  - **Knockback mode** is Smash-style: spells only shove, you lose by being pushed into the hazard, and the lower your health, the further you fly.
- A match is a series of **rounds**. Between rounds, players draft **cards**: spell upgrades, stat boons and rare "apex" cards that change how a spell works. Builds evolve over the match.
- The arena **shrinks or closes in** during each round, which forces fights.
- **Custom games:** 2 to 4 teams, adjustable rules, spectators, and bots fill empty seats. When a player is alone, bots are added so there's always someone to fight.
- Each match uses a random map out of the four.

### The 14 spells
Comet (a delayed ground strike), Warp (a teleport), Glacial Nova (an ice burst that ripens before erupting), Void Gaze (a singularity eye that pulls players in), Arc Lightning (a chaining bolt), Diamond Ward (a reflecting shield), Ice Spear (a charged lance that freezes its lane), Chain Hook (a grapple that pulls), Firebomb (a skipping fire grenade), Obsidian Blades (a fan of glass knives), Storm Strike (a lightning leap and slam), Earth Spike (an angled rock spike erupting from the ground), Purple Fireball (a charged orange-to-violet fireball) and Hex Shield (a hex-cell barrier).

Elements are fire, ice, lightning/storm, void/arcane, earth/crystal and obsidian.

### The four PvP arenas (already finished, and they look great)
1. **Molten Heart:** the Forge of the Titans. An iron mold-plate is chained above a molten pit. A titan's crucible pours molten metal down a sword-shaped trench, and late in the round the forge floods the plate. The palette is orange, black iron and glowing seams.
2. **Frozen Lake:** a long lake of black-glass ice in a snowy gorge, with a visible world beneath the ice, an aurora reflected in it, and a huge leviathan swimming underneath. Shoves slide far, and thin ice breaks into freezing water.
3. **Sky Observatory:** the domed ruin of a vanished sky civilization, high above the clouds. It has a star-chart floor with a hole in the middle and a dusk sky, and a teal energy ring closes in on the players.
4. **Storm Peak:** a jagged summit plateau above the clouds with a ritual stone circle and monoliths. It has indigo storm gloom and glowing fulgurite veins, and lightning falls at random.

### PvE mode (in progress, must NOT be the focus)
**Shardfall** is a co-op roguelite for 1 to 4 players. You fight crystal-infused enemies (crystal crawlers, duelists, archers, mages, brutes) through generated rooms, with a run map, card picks and champions. It's still being redesigned, so the hub only needs a low-key entrance for it for now.

### Look and feel
- **Camera:** a fixed high-angle 3/4 view, like an ARPG (Diablo, Path of Exile, Hades-like). The camera is about 57° down from horizontal, has a 40° field of view, follows the player and never rotates. Players see the hub mostly from above, so **design from above**. Silhouettes, ground patterns and light pools matter more than wall facades.
- **Art style:** stylised but grounded **mid-poly PBR**. Think chunky bevelled shapes and real materials (stone, iron, wood, crystal, cloth) with baked detail. It is not low-poly flat-shaded, not cartoon, and not photoreal. Every new asset must match this one style.
- **Hero:** a hooded mage in long robes and a cape. The outfit skins are a base hooded mage, a Frostwarden (ice crystals, fur, steel) and a Voidcaller (violet, cosmic). Cosmetic auras and headpieces also exist, such as an ember aura, an orbiting astral aura and a shattered crystal crown.
- **Main menu:** a 3D cosmos scene with a spiral galaxy, a nebula and stardust. The title is made of stars. A "Celestial Warden" figure is drawn as points of starlight. The UI is a brass and ivory theme with an elegant serif typeface (Cormorant SC).
- **Lighting:** dramatic and moody. It uses real-time global illumination (radiance cascades), bloom, emissive accents and per-map colour grading. Every map has a strong signature light and colour.
- **The developer dislikes:**
  - rune circles, magic circles, dials, clock needles and glyphs built from straight line segments;
  - particles pushed around by the cursor;
  - generic "AI fantasy" busyness.
- **The developer likes:** smooth, flowing cosmic shaders (nebulae, liquid light, aurora), strong silhouettes and readable shapes.

---

## The current hub (what exists today)

The hub is **Emberhold**, a floating island in a sky of clouds. Its lore is that it's a shard of volcanic crust held aloft by an "Ember Heart". I like the floating-island idea, but **the content is outdated**. It was built for an earlier ARPG version of the game that has since been cut.

### Keep (in some form)
- **The PvP portal** (currently the "Caldera Gate"). You open a window to start a match, start a custom game, or set teams. This is the most important thing in the hub.
- **Chess:** a networked 3D chess table that two players can play while waiting. Keep it.
- **The spell-picker station** (currently a "Spellbook" lectern), where you choose your 3 spells.
- **Falling off the island is a feature.** You fall for about 1.2 s, then a gust of wind carries you back up to the ledge. Nobody dies. A sign counts "Days since someone fell off: 0", and there's a title for falling 25 times. Shoving each other off the edge is part of the fun.
- **Existing easter eggs** that can carry over if they fit:
  - **Constellations:** cast each spell at its own constellation from the telescope islet to light it, which earns the "Stargazer" title.
  - **Hidden cat:** a black cat on a tiny islet reached by an invisible starpath bridge, which earns the "Faithful" title.
  - **Balloon duck:** every few minutes a duck floats past on a balloon. Pop the balloon with a spell and the duck falls.
  - **Sky leviathan:** a huge glowing whale-like creature swims through the clouds below every 12 minutes, with horn sounds.
  - **Wishing void:** toss a coin off the pier for a fortune.
  - **Hidden shrine:** a secret portal leads to a shrine on a lower islet.
- **Player titles** ("honors") are shown under names, so new secrets can award new titles.

### Remove
- The mission board, forge and anvil, stash chest, gem altar and market stalls. All of it is obsolete ARPG content.
- The **lava pool** in the middle of the island. A lava pit on a sky island looks wrong.
- The **training dummies and practice ring** in the open hub. They should move into a separate room.
- The proving-grounds islet.

---

## Requirements for the new hub

### Must have
1. **The PvP portal is the focal point.** Ideally there's a way to "ready up" in the world, so you can see who is about to play.
2. **A separate training room** (interior or its own area, reached through a door or portal) with:
   - training dummies (they have HP, take damage and knockback, fall over, respawn and walk back to their posts);
   - **the spell-swap station right next to the dummies**, so you can change spells and test them immediately;
   - room for ideas like moving dummies, DPS readouts, or practising the hazard types from the maps (lava, ice slide, void edge).
3. **A PvE (Shardfall) entrance** that exists but is understated, since the mode is unfinished.
4. **Chess** stays.
5. **Fun while waiting.** Players often idle for 1 to 5 minutes waiting for friends or a lobby. Give them toys, minigames, things to discover and ways to mess with each other. Spells work in the hub, and players can shove each other.
6. **Lots of ambient animation and dynamic events**, so the place feels alive.
7. **Easter eggs**, including some that need several players to cooperate.

### Nice to have
- A visual link to the four arenas, so new players can see where they'll fight.
- A visual link to the cosmic main menu and the Celestial Warden.
- The hub reacting to spells: freeze something, burn something, ring something.
- Spots where players can show off their cosmetics and titles.

### Technical constraints (please respect these)
- **Gameplay is 2.5D.** The world is a tile grid with a height per cell. There's no jumping, and **walkable floors can't overlap**, so no walkable bridge can pass over other walkable ground. Interiors and multi-level spaces work through **portals** to another area of the map. Stairs and ramps between height levels are fine, and so are drops into the void.
- Characters are circles moving on that grid. There's no general physics engine, so props are static colliders. Scripted or animated motion of props, critters and backdrop elements is fine.
- Everything interactive must be **networked**, with a server-authoritative simulation and client prediction. Cheap, deterministic things are best, like timers, state flags and simple scripted events. Purely visual ambient life (critters, clouds, birds, backdrop creatures) can run client-side.
- **Performance:** it must run at a good frame rate on **integrated laptop GPUs**. The hub's current GPU frame time is about 10 to 11 ms on an AMD iGPU, so ambient life needs to be cheap: shaders, GPU particles and instanced props, not hundreds of skinned characters.
- **What the engine has:**
  - deferred PBR rendering with radiance cascades GI, a sun shadow map, bloom and colour grading;
  - GPU particles, ribbon trails, projected decals, screen distortion (shockwaves, heat haze), dissolve effects and glass or crystal shading;
  - skinned characters with IK, an animated sky and cloud backdrop shader (which can draw huge distant creatures), heightfield terrain with blended materials, and procedural cliffs and rock faces for floating islands;
  - reflective water and ice;
  - a Blender-based asset pipeline, so new props get modelled.
- **Hub size:** compact enough that waiting players can see each other. The current island is about 48 × 42 tiles, and a character is about 1 tile wide.

---

## Optional: the ideas I already have

*Remove this section if you want ideas that aren't influenced by mine.*

- **Arena Archipelago:** the four arenas float around the hub as distant mini-islands, and the portal beams toward whichever map is chosen.
- **Sky Harbour:** the portal is a skyship dock, and a ship sails in when a match is found.
- **Brass Orrery:** the island sits inside a huge rotating brass orrery, matching the main menu.
- **Replace the lava pool** with a glass window in the plaza floor that looks down at the glowing Ember Heart hanging beneath the island.
- **A ready-up ring** at the portal that brightens with each player standing in it.
- **A brink ring:** a small sumo platform over the void for shoving contests.
- **Spell-reactive props:**
  - Frost freezes the fountain into a slide.
  - Fire melts it and lights the braziers.
  - Lightning charges lamps.
  - Earth Spike rings a bell.
- **Singing crystals** that play notes when hit, with a secret melody.
- **Sky fishing**, plus a cat you can befriend with a rare fish.
- **A graveyard of cut features** with joke epitaphs.
- **A snow globe** showing a live miniature of the hub.
- **A secret leap-of-faith ledge.**
- **Four pressure plates** that need four players at once and trigger fireworks.
