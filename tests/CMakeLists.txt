add_executable(CascadeTimerCallTest cascadetimercall.cpp)
target_link_libraries(CascadeTimerCallTest ${PROJECT_NAME})

add_executable(CallingFromAnotherThreadTest callingfromanotherthread.cpp)
target_link_libraries(CallingFromAnotherThreadTest ${PROJECT_NAME})

add_executable(RecallOneShotTimerTest recalloneshottimer.cpp)
target_link_libraries(RecallOneShotTimerTest ${PROJECT_NAME})

add_executable(RestartTimerBeforeTimeoutTest restarttimerbeforetimeout.cpp)
target_link_libraries(RestartTimerBeforeTimeoutTest ${PROJECT_NAME})

add_executable(StopFromTimerTest stopfromtimer.cpp)
target_link_libraries(StopFromTimerTest ${PROJECT_NAME})

add_executable(StopTimerTest stoptimer.cpp)
target_link_libraries(StopTimerTest ${PROJECT_NAME})

add_executable(ConcurrenceTest concurrence.cpp)
target_link_libraries(ConcurrenceTest ${PROJECT_NAME})