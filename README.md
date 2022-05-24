# pydds
Replacement library for ImageMagick to decompress from many DDS pixel formats with an optional crop to a target region. While cropping can be done with separate libraries like Pillow doing it as part of decompression can save a lot of work as only the required blocks will be decompressed.