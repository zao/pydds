# pydds
Replacement library for ImageMagick to decompress from many DDS pixel formats with an optional crop to a target region. While cropping can be done with separate libraries like Pillow doing it as part of decompression can save a lot of work as only the required blocks will be decompressed.

## Usage

```python
from dds import decode_dds
import PIL

# optional cropping region of [x0, y0, w, h] to be applied while decoding
crop = [0, 0, 20, 20]

with open('Atlas.dds', 'rb') as fh:
    img: PIL.Image = decode_dds(fh.read(), crop)
```