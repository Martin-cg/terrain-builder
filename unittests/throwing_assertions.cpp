#include <stdexcept>

#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>
#include <libassert/assert.hpp>

namespace {

[[noreturn]] void throw_assertion_failure(const libassert::assertion_info &info) {
    throw std::logic_error(info.to_string(0, libassert::color_scheme::blank));
}

// Makes a failed assertion throw, so a test can expect it and any other test reports it instead of aborting the run.
class ThrowingAssertionsListener : public Catch::EventListenerBase {
public:
    using Catch::EventListenerBase::EventListenerBase;

    void testRunStarting(const Catch::TestRunInfo &) override {
        libassert::set_failure_handler(throw_assertion_failure);
    }
};

}

CATCH_REGISTER_LISTENER(ThrowingAssertionsListener)
