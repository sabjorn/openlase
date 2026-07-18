Docker Usage
============

Build the Docker image:
  docker build -t openlase:gui .

Run any example or tool:
  docker run -it --rm -p 5901:5900 openlase:gui <command>

Available commands:
  simulator      - Just the OpenGL simulator
  simple         - Simple example (auto-connected to simulator)
  circlescope    - Circle scope example (auto-connected to simulator)
  <any-example>  - Any executable in ./examples/ (auto-connected)
  <any-tool>     - Any executable in ./tools/

View available commands:
  docker run openlase:gui help

Connect via VNC:
  VNC client to localhost:5901 (password: openlase)
  Or on macOS: open vnc://localhost:5901

Adding New Examples:
  1. Add source to examples/ directory
  2. Add build target to examples/CMakeLists.txt
  3. Rebuild Docker image
  4. Run: docker run openlase:gui <your-example-name>

The entrypoint automatically detects executables in ./tools/ and ./examples/.
Examples are auto-connected to the simulator via JACK.

Original Documentation
======================

This project is now hosted on GitHub, so your best bet for documentation is
the wiki. You'll find it here:

https://github.com/marcan/openlase/wiki

Old README contents:

No documentation for now, sorry! But there are a few examples. You'll want to
read the blog post here:

http://marcansoft.com/blog/2010/11/openlase-open-realtime-laser-graphics/

Please drop me a line if you find any of this useful or you have suggestions!

TODO/bugs:

- Near/far clipping in 3D. Currently objects behind the camera cause all kinds
  of fail.
- Color interpolation. Right now it just switches colors on vertices.
- RGB support. The basics are there in libol, but I'm sure I'm missing stuff
  since I currently can't test it.
- Unify genfont.py and svg2ild.py. Right now genfont is a horrible
  cut-and-paste-and-hack of svg2ild.
- Better integrate SVGs with libol, and/or deduplicate code. Currently svg2ild
  does a lot of the same stuff tha libol does (rendering and object reordering).
  genfont might be a step forwards, but libol's bezier support needs to improve.
  Also, I need some kind of higher level format for bezier-based laser graphics
  (ILDA is sample-based). On the other hand, it would be nice to make libol's
  ILDA loader optionally split the ILDA stream into objects to merge in with
  the scene more efficiently.
- Optimize
- Tons more that I'm forgetting

Thoughts:
- Develop a "codec" for mkv/whatever to do sample-based laser graphics? So
  playvid can play dedicated laser videos. After all, existing video containers
  already do all of the audio and sync stuff for us, it makes no sense to invent
  a format from scratch. I could even write an mplayer "decoder" that renders
  the image, so it can be previewed.
