# Image Resources

This directory contains image resources for Rats Search Qt.

## Required Images

The following images should be added here:

1. **rat-logo.png** - Main application logo (from original rats-search)
   - Copy from: `../resources/rat-logo.png`
   - Size: Recommended 256x256 or larger

2. **icon.png** - Application icon
   - Should be a smaller version of the logo
   - Size: 128x128 or 256x256

## Adding Images

To add the images:

1. Copy `rat-logo.png` from the main rats-search resources folder
2. Create `icon.png` (can be same as logo or custom design)
3. Rebuild the project to include resources

## Alternative

If images are not available, you can:

1. Remove references from `resources.qrc`
2. Use Qt's default icons instead
3. Add your own custom images

The application will work without these images, but icons may not display.

