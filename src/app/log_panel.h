#pragma once

#include "mt.h"

#include <deque>
#include <mutex>
#include <spdlog/sinks/base_sink.h>

namespace mt {

	/*! spdlog sink that keeps the latest messages for the log panel */
	class LogBuffer : public spdlog::sinks::base_sink<std::mutex> {
	  public:
		/*! A logged message */
		struct Entry {
			spdlog::level::level_enum level;
			std::string text;
		};

		static constexpr size_t MAX_ENTRIES = 2000;

		/**
		 * @return the shared buffer that the application logger writes to
		 */
		static std::shared_ptr<LogBuffer> instance();

		/**
		 * Copy of the messages (the logger can write from other threads)
		 */
		std::vector<Entry> entries();

		/**
		 * Remove every message
		 */
		void clearEntries();

		/**
		 * @return number of messages ever logged, changes when new messages come in
		 */
		size_t counter();

	  protected:
		void sink_it_(const spdlog::details::log_msg &message) override;

		void flush_() override {}

	  private:
		std::deque<Entry> buffer{};
		size_t total = 0;
	};

	/*! Dockable window that shows the log */
	class LogPanel {
	  public:
		/**
		 * Draw the log window
		 * @param open window visibility, the close button sets it to false
		 */
		void render(bool &open);

	  private:
		int minimumLevel = 2;	///< lowest level shown (spdlog level), info by default
		std::string filter{};	///< text filter
		bool autoScroll = true; ///< follow new messages
		size_t lastCounter = 0; ///< message counter at the last draw
	};
}
