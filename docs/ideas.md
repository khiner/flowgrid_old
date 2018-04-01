**(no machine learning technically needed)**:

*   Reproduce Ableton's simpler with full UI as close as possible to Ableton's:
    *   [https://github.com/Ableton/push2-display-with-juce](https://github.com/Ableton/push2-display-with-juce)
*   Flow-machine-inspired looper
    *   Architecture diagrams/notes (like drawings on graph-paper)
    *   Build with all manual loop-start/stop controls
    *   Add tempo detection, use tempo of incoming audio
    *   Remember last N bars of audio, or N words/phrases/etc being robust to many types of audio.  Single button to capture and loop seamlessly.
    *   Auto-slice into drum pads using onset detection
    *   Audio stretching
    *   Generate MIDI streams to control other instruments in studio (no synthesis yet)
*   Singing to MIDI notes live
*   Stochastic residual of one sound mixed with synthesised sinusoidal spectrum of another
*   Turn Markov-midi into a midi-controllable instrument (knobs control algo params)
*   Make [AutoSampler](http://karlhiner.com/music_generation/autosampler/)-ish thing into a component
*   Extract audio features live and map them to 0-1 values for use as modulation sources
*   Controlling music parameters with geometric pattern generation
    *   With patterns like [https://youtu.be/q1AgXVGbl_w?t=10m40s](https://youtu.be/q1AgXVGbl_w?t=10m40s)
*   Compile Braids code (or parts of it) to run as a MIDI instrument
*   Harmonica Module idea
*   Spectrum-painting instrument, inspired by Equation by Aphex Twin
*   "Simply" combine many "dumb" methods of analyzing, classifying, etc. On the MIR side, and feed output to advanced emulating generation side with the goal of accurately reproducing pieces from their low-, mid- and high-level musical features and events.
* 

**(some machine learning probably needed)**:

*   Beatbox to categorized drum beats
*   Search for sounds by "sonic signature" (texture, genre, other metrics - closest match)
*   Extract drum patterns with multiple drum voices from tracks
*   Search for multi-voice rhythmic/drum patterns in large music library (input is drum pattern midi, output is audio segment)
*   Classifying where someone lives from their vocal intonation (pitch as only feature)
    *   Could use generative technique (below) to synthesise "average" voices from different geographical areas
*   Implement similarity measure similar to Essentia
    *   Download all of FreeSound lib
*   Autoencoding technique for video frames translated to audio domain: [https://www.academia.edu/25585807/Autoencoding_Video_Frame†s](https://www.academia.edu/25585807/Autoencoding_Video_Frames)
*   Deconvolution (for upsampling lower dimensional features to high-dimensional output)
    *   [https://arxiv.org/pdf/1603.07285v1.pdf](https://arxiv.org/pdf/1603.07285v1.pdf)
    *   [https://arxiv.org/pdf/1609.07009.pdf](https://arxiv.org/pdf/1609.07009.pdf)
*   GAN techniques
    *   Latent variable weights could be manually controllable.
    *   [https://research.googleblog.com/2015/06/inceptionism-going-deeper-into-neural.html](https://research.googleblog.com/2015/06/inceptionism-going-deeper-into-neural.html)
    *   [https://arxiv.org/pdf/1409.4842.pdf](https://arxiv.org/pdf/1409.4842.pdf)
*   Anything on [https://github.com/ybayle/awesome-deep-learning-music](https://github.com/ybayle/awesome-deep-learning-music)
*   DeepMind's Wavenet ([https://deepmind.com/blog/wavenet-generative-model-raw-audio/](https://deepmind.com/blog/wavenet-generative-model-raw-audio/))
*   [https://github.com/mxgmn/WaveFunctionCollapse](https://github.com/mxgmn/WaveFunctionCollapse) (generating similar audio patterns from short input audio)
*   Smooth audio stretching without frequency-changing artifacts using techniques to "fill missing data" (enhance!)
    *   (for photos: [https://github.com/alexjc/neural-enhance](https://github.com/alexjc/neural-enhance))
    *   Real-Time Single Image and Video Super-Resolution Using an Efficient Sub-Pixel Convolutional Neural Network: [https://arxiv.org/pdf/1609.05158.pdf](https://arxiv.org/pdf/1609.05158.pdf)
*   Learning melody/rhythm generators inspired by live input
*   "Reverse" (overinterpreting) NNs (inception) 
*   Mixing audio with the style of another audio piece
    *   [https://github.com/jcjohnson/neural-style](https://github.com/jcjohnson/neural-style)
    *   [https://arxiv.org/abs/1508.06576](https://arxiv.org/abs/1508.06576)
    *   Artistic style transfer: [https://arxiv.org/pdf/1603.08155v1.pdf](https://arxiv.org/pdf/1603.08155v1.pdf)
*   Learn/reproduce reverb by segmenting all delay components and inferring its delay network architecture
*   Reproduce entire songs by combining and transforming samples from a static sample library, emulating each voice in the original song over the full duration.  Select samples based on some form of similarity for each voice & transform as needed.
*   Generate audio waveforms using Gaussian Processes with periodic kernels trained on sample points
    *   Oscillator idea: Gaussian-process-generated waveform with periodic kernel where osc params change kernel (and other) params
    *   Learn kernel function?!?!? Based on input?!?!
*   Use Markov networks (MRF) (or a Conditional Random Field)
    *   Could be useful for source-separation, noise removal, reconstruction
    *   Denoising problem statement:
        *   Task is to restore the "true" value of all frames given possibly noisy frame values
        *   Truncated linear distance function (as explained in [https://www.coursera.org/learn/probabilistic-graphical-models/lecture/iuc7O/log-linear-models](https://www.coursera.org/learn/probabilistic-graphical-models/lecture/iuc7O/log-linear-models))
*   Learner that reproduces example Pro 6 sounds through MIDI CC adjustment ([http://www.davesmithinstruments.com/wp-content/uploads/2015/06/Prophet-6-Operation-Manual.pdf?da8101](http://www.davesmithinstruments.com/wp-content/uploads/2015/06/Prophet-6-Operation-Manual.pdf?da8101) p69)
*   Vocal enhancement:
    *   Vocal doubling/harmony effect that introduces realistic sounding deviations in vocal tone (to avoid the flat-sounding exact-stacked voice)
*   N features extracted from a single source of audio act as parameters.  User controllable parameters act as [0,1] input weights on the parameters into the first (N-input) layer of a "Parameter" RNN.
    *   Model is divided up into (at least) "Parameters" and "Values" networks.  The "values" network contains noun-style features, denoting the presence or absence of a musical feature, or a category or range of a musical feature, e.g. "Produced in the 60's", "nostalgic", "upbeat", percussion-model-name, reverb, voice-type, note value, etc.
    *   "Parameters" are value _magnitudes_, sequences of magnitudes or relationships of magnitudes as extracted from training audio.  This would include all temporal aspects such as trigger patterns, note onsets, tempo, beat division, related to or somehow inspired by incoming value parameters from the input source sound/sounds.
    *   This allows the separation of the "what" from the "how and when" of extracted data, and promotes abstraction of input material into more wildly divergent but still meaningfully related output material.  "What"s can be related to other "what's and "how/when"s can be related to other "how/when"s, where those relationships are learned from a wide selection of training data.
*   Train NN where each layer learns to model a specific musical function (such as a the functions in a traditional subtractive synthesis voice). Then architectures could be learned to produce sounds similar to input sounds
    *   Inspired by [http://colah.github.io/posts/2015-09-NN-Types-FP/](http://colah.github.io/posts/2015-09-NN-Types-FP/) (representations corresponding to types)
    *   Training data could be both sounds/sound features as well as "labels" of the synthesis functions & param values used to produce the sound
    *   Train on pure synthesized sounds, and then hope to produce interesting results with natural/complex sounds as input
    *   (This would still be a challenging learning task even if the audio functions were not modeled by neural nets.  Training NNs to model audio functions would likely be a separate project altogether.)
*   Using keywords/grammar of an existing music programming language (like Sonic Pi) as output, generate valid music scripts.
    *   Could train on existing full scripts
    *   Could try and optimize toward an audio segment, but I'm guessing the training iterations would be ungodly slow, since each iteration would require producing and evaluating audio output (may be a way to render the audio faster than playback time)
*   Voice-inspired audio generation
    *   Generative music influenced only by vocal gestures
*   Learn the best possible Push controller jambox for the last N minutes of audio that's streamed through the stereo channel.
    *   The goal is for fun and playability.  I'm not exactly sure what the parameters would be, but that could be a great place for a lot of these other ideas.  Sampler instances, sampler settings and potentially matching effects/settings with well-mapped pads & knobs.
*   Treating each high-level audio feature as a random variable, learn a probabilistic model estimating the most likely feature vector at each time point given all past vectors, trained on a large music set.  Then use the model to generate feature vectors and compile them to audio.
    *   Could seed with any set of audio.  Could have adjustable parameters corresponding to priors over the audio feature RVs.
*   Multi-agent improvisation simulation.  Each agent is responsible for an instrument and "listens" to other agents to constrain its output.
