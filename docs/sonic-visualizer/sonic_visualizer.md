# Sonic Visualizer

This is just me having fun with visual audio ideas.

* Translating audio features into a series of linear transformations on visual features

* Apply transformations to
  - the time-scale in transitioning between different images smoothly
  - the resolution (think of this like what magnitude are the basis vectors)
  - basis transformations
  - color
  - opacity

* Provide mapping controls to link audio features => linear alg transformations => visual features
* Go the other direction? O_O
  - Take direct transformation output or corresponding (transformed) visual features and translate them back into audio features

*  Send to Push display _and_ the screen
- Opportunity to deeply incorporate MRI and linear alg library (MRI stuff needs to get pulled into that project anyway)
    - Allow params & labels to show or not show (directly ontop of animation, fade in & out and keep some alpha)
    - Sets a stage for other highly visual instruments
      - Image-drum-pattern-generator
      - Translate local image feature intensities to x-y coordinates as if listener was in the center of the image
        (or could move around within it).  Map to frequencies, or any parameters for any set of generating granular units that surround in 2/3-d

  - Implement as audio plugin

