HumanHead.blend
	Head, sclera and iris meshes taken from the Blender Foundation "Human Base Meshes" asset bundle v1.4.1
	(https://download.blender.org/demo/asset-bundles/human-base-meshes/), released under CC0.
	The realistic animation head was reduced to its base level with its multires sculpt applied ("Apply Base"),
	transformed into world space and given an integer face attribute "Region" copied from the sculpt face sets.
	hero.py reshapes it, adds the beard, hair and materials, and bakes it like every other generated asset.
mocap/
	Motion capture trials from the CMU Graphics Lab Motion Capture Database (http://mocap.cs.cmu.edu/).
	The data used in this project was obtained from mocap.cs.cmu.edu.
	The database was created with funding from NSF EIA-0196217.
	Each .asf is a subject skeleton and each .amc a trial cut down to the span heroanim.py uses and decimated
	from 120 to 60 fps; "# offset" is the start of the span in the original trial (seconds), "# rate" the frame rate.
	heroanim.py retargets them onto the hero rig, and hero.py exports the result as the Hero.gltf animations.
mixamo/
	Animations downloaded from Adobe Mixamo (https://www.mixamo.com/) as FBX "Without Skin" at 30 fps, including clips
	from the "Pro Magic Pack". Mixamo animations may be used royalty free in games, but not redistributed as standalone files,
	so the shipped game only contains them baked into the hero clips. heroanim.py samples the mixamorig bone curves
	(class Mixamo), retargets them onto the hero rig and trims the casts around their strike frames.
