Import("env")

# Run the ensure_media_player.py script after the build process
env.AddPostAction("buildprog", "python ensure_media_player.py")
