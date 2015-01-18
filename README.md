Having a little bit of fun.

![asciiworld](/asciiworld.png?raw=true)

Inspired by the world map in [blessed-contrib](https://github.com/yaronn/blessed-contrib).

Dependencies
============

You need a map. I suggest you use one of those:

*  [Natural Earth Data, Land](http://www.naturalearthdata.com/downloads/110m-physical-vectors/110m-land/)
*  [Natural Earth Data, Countries](http://www.naturalearthdata.com/downloads/110m-cultural-vectors/110m-admin-0-countries/)

Since NED distributes their data as "ESRI Shapefiles", you also need this library:

*  [Shapelib](http://shapelib.maptools.org/)

In Arch Linux, the library is available in the package [shapelib](https://www.archlinux.org/packages/community/x86_64/shapelib/).

Command line options
====================

`-w W`, `-h H`: Force output size. By default, the full size of the terminal is used.

`-m path`: Path to an ESRI Shapefile (.shx or .shp).

`-l path`: Path to a file containing locations to highlight. For example, the following file would highlight Mainz and Cape Town:

    50 8
    -33.9 18.4

More ideas
==========

*  Zoom/pan/crop.
*  Highlight areas.
*  Add a day-and-night mode.
*  Highlight countries.

CREDITS
=======

*  [Bresenham](https://de.wikipedia.org/wiki/Bresenham-Algorithmus#C-Implementierung)
