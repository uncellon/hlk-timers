# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## Unreleased

## 2.1.3 - 2021-10-19

### Fixed

- Segmentation fault...again.

## 2.1.2 - 2021-10-14

### Fixed

- Perfomance and stability improvements

## 2.1.1 - 2021-10-14

### Changed

- Timer signal

## 2.1.0 - 2021-10-11

### Added

- Dispatcher thread: now the signal wakes up the dispatcher thread instead of creating the job

### Fixed

- Signal handler used non signal-safety functions

## 2.0.1 - 2021-10-09

### Fixed

- Threads signal mask

## 2.0.0 - 2021-09-28

### Changed

- Timers are now based on real time signal rather than file descriptors so the number of timers is now unlimited

## 1.2.6 - 2021-09-24

### Fixed

- Skip second when nanoseconds were rounded

## 1.2.5 - 2021-09-22

### Fixed

- Deadlock when the timer was started and the one-time start flag was checked 

## 1.2.4 - 2021-09-21

### Fixed

- Bug when the timer descriptor was reset to -1 in the timer handler thread, but another thread updated the timer 

## 1.2.3 - 2021-09-03

### Fixed

- Bug when timer was deleted after stopping itself onTimeout() handler and starting again

## 1.2.2 - 2021-09-03

### Added

- Change log

### Fixed

- Fixed handling of file descriptor zero