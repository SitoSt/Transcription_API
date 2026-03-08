#include <gtest/gtest.h>
#include "server/SessionTracker.h"
#include <atomic>

// Mock mínimo de SessionBase
class MockSession : public SessionTracker::SessionBase {
public:
    std::atomic<int> shutdown_count{0};
    void shutdown() override { ++shutdown_count; }
};

// Helper para limpiar el singleton entre tests
// (cada test hace add + remove, el tracker debe quedar vacío)

TEST(SessionTracker, AddAndRemoveDoNotCrash) {
    MockSession s;
    SessionTracker::instance().add(&s);
    SessionTracker::instance().remove(&s);
}

TEST(SessionTracker, ShutdownAllCallsShutdownOnAllActiveSessions) {
    MockSession s1, s2;
    SessionTracker::instance().add(&s1);
    SessionTracker::instance().add(&s2);

    SessionTracker::instance().shutdownAll();

    EXPECT_EQ(s1.shutdown_count.load(), 1);
    EXPECT_EQ(s2.shutdown_count.load(), 1);

    SessionTracker::instance().remove(&s1);
    SessionTracker::instance().remove(&s2);
}

TEST(SessionTracker, RemovedSessionIsNotCalledOnShutdown) {
    MockSession s1, s2;
    SessionTracker::instance().add(&s1);
    SessionTracker::instance().add(&s2);
    SessionTracker::instance().remove(&s1); // eliminar antes del shutdown

    SessionTracker::instance().shutdownAll();

    EXPECT_EQ(s1.shutdown_count.load(), 0); // no fue llamado
    EXPECT_EQ(s2.shutdown_count.load(), 1);

    SessionTracker::instance().remove(&s2);
}

TEST(SessionTracker, ShutdownAllOnEmptyTrackerDoesNotCrash) {
    EXPECT_NO_THROW(SessionTracker::instance().shutdownAll());
}
