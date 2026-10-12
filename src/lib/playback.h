#pragma once

#include "library.h"
#include "model.h"

namespace mt::model {

	/*! Animation playback clock, turns elapsed time into a frame (and the frame to blend to) */
	class Playback {
	  public:
		/**
		 * Play a range of frames
		 * @param first first frame
		 * @param last last frame, smaller than first plays backwards
		 * @param fps frames per second (0 or less uses 24)
		 * @param loop start over after the last frame
		 */
		void setRange(int first, int last, float fps, bool loop);

		/**
		 * Play an animation sequence
		 * @param animation sequence to play (reversed sequences play backwards)
		 */
		void setAnimation(const Animation &animation);

		void play();
		void pause();

		/**
		 * Go to a frame of the range and stop blending
		 * @param frame frame number, clamped to the range
		 */
		void seek(int frame);

		/**
		 * Move the clock forward
		 * @param seconds elapsed time
		 */
		void advance(double seconds);

		/**
		 * @return the frame to show
		 */
		[[nodiscard]] int frame() const;

		/**
		 * @return the frame to blend towards
		 */
		[[nodiscard]] int nextFrame() const;

		/**
		 * @return 0 - 1 blend between frame() and nextFrame(), 0 without interpolation
		 */
		[[nodiscard]] float blend() const;

		[[nodiscard]] bool isPlaying() const
		{
			return playing;
		}

		[[nodiscard]] int first() const
		{
			return firstFrame;
		}

		[[nodiscard]] int last() const
		{
			return lastFrame;
		}

		float speed = 1.0f;		 ///< playback speed multiplier
		bool interpolate = true; ///< blend between frames
		bool loop = true;		 ///< start over after the last frame

	  private:
		[[nodiscard]] int length() const;
		[[nodiscard]] int step(int offset) const;

		int firstFrame = 0;
		int lastFrame = 0;
		float fps = 24.0f;
		double position = 0; ///< frames played from the first frame
		bool playing = false;
	};
}
