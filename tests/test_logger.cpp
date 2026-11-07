// ---------------------------------------------------------------------------
// Bug B9 (Phase 0.4 QA hardening): Logger::log() wrote to std::cout with NO
// flush, so under headless runs the output sat in the stream buffer and was lost
// if the process died or never reached a natural flush — an "unflushable" logger
// that hid exactly the diagnosability output it existed to emit (CONTEXT §6.7 /
// §6.8: stdout block-buffering truncates the log mid-line). The fix is one token
// in Logger::log(): flush per record.
//
// This proves the flush happens SYNCHRONOUSLY on each log call, not merely
// "eventually." std::cout has exactly one streambuf (its rdbuf()); we swap it for
// a sink that accepts inserted characters into its own buffer and only releases
// them to `sink` when sync() is called. Without a flush the record stays in the
// buffer and never reaches the sink (assertion fails); with the fix it does.
// ---------------------------------------------------------------------------

#include <catch2/catch_all.hpp>
#include "Logger.hpp"

#include <streambuf>
#include <string>
#include <vector>

namespace
{
    // A std::cout streambuf that buffers insertions and releases them to `sink`
    // only on sync() (i.e. only when the stream is flushed). This lets a test
    // distinguish "written but not flushed" from "written and flushed".
    class FlushCaptureBuf : public std::streambuf
    {
      public:
        explicit FlushCaptureBuf(std::vector<char>& sink) : sink_(sink) {}

      protected:
        int overflow(int c) override
        {
            if (c != traits_type::eof())
                buffer_.push_back(static_cast<char>(c));
            return 0; // accept the char into our own buffer
        }

        int sync() override
        {
            if (!buffer_.empty())
            {
                sink_.insert(sink_.end(), buffer_.begin(), buffer_.end());
                buffer_.clear();
            }
            return 0;
        }

      private:
        std::vector<char>& sink_;
        std::vector<char> buffer_;
    };

    // RAII: restore cout's original streambuf AND the Logger level on scope exit,
    // including when a REQUIRE throws — so this test never perturbs later ones.
    struct TestScope
    {
        std::streambuf* coutOrig;
        LogLevel levelOrig;

        TestScope() : coutOrig(std::cout.rdbuf()), levelOrig(Logger::getLevel()) {}

        ~TestScope()
        {
            std::cout.rdbuf(coutOrig);
            Logger::setLevel(levelOrig);
        }
    };
}

TEST_CASE("B9: Logger flushes each record to std::cout synchronously", "[Logger][B9]")
{
    TestScope scope; // level stays at the project default (DEBUG), so INFO fires

    std::vector<char> sink;
    FlushCaptureBuf buf(sink);
    scope.coutOrig = std::cout.rdbuf(&buf); // re-arm after construction saved it

    const char* marker = "B9_FLUSH_MARKER_XYZ";
    Logger::info(marker); // -> Logger::log(INFO, marker)

    // After the call returns, a synchronous flush means the record already reached
    // the sink. Before the fix (no flush) this is empty and the REQUIRE fails.
    std::string written(sink.begin(), sink.end());
    REQUIRE(written.find(marker) != std::string::npos);
}

TEST_CASE("B9: a below-threshold record writes nothing at all", "[Logger][B9]")
{
    TestScope scope;
    Logger::setLevel(LogLevel::WARN); // DEBUG is now suppressed

    std::vector<char> sink;
    FlushCaptureBuf buf(sink);
    scope.coutOrig = std::cout.rdbuf(&buf);

    Logger::debug("B9_SUPPRESSED"); // level < currentLevel -> early return, no write

    std::string written(sink.begin(), sink.end());
    REQUIRE(written.find("B9_SUPPRESSED") == std::string::npos);
}

TEST_CASE("B9: warn and error records flush", "[Logger][B9]")
{
    TestScope scope; // default DEBUG level, so WARN/ERROR fire

    std::vector<char> sink;
    FlushCaptureBuf buf(sink);
    scope.coutOrig = std::cout.rdbuf(&buf);

    Logger::warn("B9_WARN_MARKER_XYZ");
    Logger::error("B9_ERROR_MARKER_XYZ");

    std::string written(sink.begin(), sink.end());
    REQUIRE(written.find("B9_WARN_MARKER_XYZ") != std::string::npos);
    REQUIRE(written.find("B9_ERROR_MARKER_XYZ") != std::string::npos);
}
