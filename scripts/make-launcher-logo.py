from pathlib import Path

from PIL import Image, ImageChops


def main():
    icons = Path(__file__).resolve().parents[1] / "dist" / "icons"
    image = Image.open(icons / "launcher-logo.png").convert("RGBA")
    image.thumbnail((800, 800), Image.Resampling.LANCZOS)
    red, green, blue, alpha = image.split()
    image = Image.merge("RGBA", (
        ImageChops.multiply(red, alpha),
        ImageChops.multiply(green, alpha),
        ImageChops.multiply(blue, alpha),
        alpha,
    ))
    image.save(icons / "launcher-logo.bmp")


if __name__ == "__main__":
    main()
