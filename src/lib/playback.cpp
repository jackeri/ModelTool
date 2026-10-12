#include "playback.h"

namespace mt::model {

	void Playback::setRange(int first, int last, float framesPerSecond, bool looping)
	{
		firstFrame = first;
		lastFrame = last;
		fps = framesPerSecond > 0 ? framesPerSecond : 24.0f;
		loop = looping;
		position = 0;
	}

	void Playback::setAnimation(const Animation &animation)
	{
		setRange(animation.startFrame, animation.lastFrame(), float(animation.fps), animation.loop);
	}

	void Playback::play()
	{
		// Start over if a non looping range has finished
		if (!loop && position >= length() - 1)
		{
			position = 0;
		}
		playing = true;
	}

	void Playback::pause()
	{
		playing = false;
	}

	void Playback::seek(int frame)
	{
		int offset = lastFrame >= firstFrame ? frame - firstFrame : firstFrame - frame;
		position = std::clamp(offset, 0, length() - 1);
	}

	void Playback::advance(double seconds)
	{
		if (!playing || seconds <= 0)
		{
			return;
		}

		const int frames = length();
		position += seconds * fps * double(speed);

		if (frames <= 1)
		{
			position = 0;
			return;
		}

		if (loop)
		{
			position = std::fmod(position, double(frames));
		}
		else if (position >= frames - 1)
		{
			position = frames - 1;
			playing = false;
		}
	}

	int Playback::length() const
	{
		return std::abs(lastFrame - firstFrame) + 1;
	}

	int Playback::step(int offset) const
	{
		return lastFrame >= firstFrame ? firstFrame + offset : firstFrame - offset;
	}

	int Playback::frame() const
	{
		return step(int(position));
	}

	int Playback::nextFrame() const
	{
		int next = int(position) + 1;
		if (next >= length())
		{
			next = loop ? 0 : length() - 1;
		}
		return step(next);
	}

	float Playback::blend() const
	{
		return interpolate ? float(position - std::floor(position)) : 0.0f;
	}
}
