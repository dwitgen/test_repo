import os
import shutil

def ensure_media_player():
    # Define the source and destination directories
    source_dir = os.path.join(os.getcwd(), 'components', 'esp_adf', 'media_player')
    destination_dir = os.path.join(os.getcwd(), 'src', 'esphome', 'components', 'esp_adf', 'media_player')

    # If the source directory exists, move it to the destination
    if os.path.exists(source_dir):
        if os.path.exists(destination_dir):
            shutil.rmtree(destination_dir)
        shutil.copytree(source_dir, destination_dir)
    else:
        print(f"Source directory {source_dir} does not exist. Skipping.")

# Run the function
ensure_media_player()
