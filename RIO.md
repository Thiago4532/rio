# RIO - An async IO library using coroutines

## Tasks
    - [ ] Use a better time_type that differentiate between time and duration.
    - [ ] scheduled_handle should be public, and the app must have a way to add read/write
callbacks to normal functions, instead of having to await.
    - [ ] Allow scheduling lambdas that capture variables.

## Observations
     - RIO is vulnerable to starvation, since the way it's implemented it's going to
keep reading a file descriptor until it has no data available.
    - All coroutines waiting on a file descriptor queue wake up even if the data
was already consumed by another handle in the queue.
