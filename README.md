# Lab 06

Status Badge: ![Lab 06 Status](https://github.com/uofu-adv-emb-25/lab06_Ian_Kasey/actions/workflows/main.yml/badge.svg)

By Ian Smith and Kasey Kemp

## Activity 0

---

For this activity we were asked to setup three threads, where the lower priority thread takes a lock and holds it, a high priority thread tries to take that lock after a delay, and a medium priority lock executes some code.

We found that the higher priority thread was blocked while the lower priority thread kept the lock, which resulted in the medium priority thread being ran for a majority of the time and not allowing the lower priority to complete its work.

This is an example of priority inversion.

![activity 0](resources/activity0.png)

## Activity 1

---

For this activity we were asked to replicate activity 0 but use xSemaphoreCreateMutex() instead of xSemaphoreCreateBinary(). Since
a mutex allows for priority inheritance, the lower prority task will no longer be starved by the medium priority task and will actually complete its work.
This allows it to finally give up the lock and allow the higher priority task to run until completion. This means the medium priority task does not get execution time in our example.

![activity 1](resources/activity1.png)

## Summary of Tests:

---

Testing for priority inversion expects the lower and higher priority tasks to have no increasing processor time, the medium priority task to have increasing processor time, and for the higher priority task to be blocked while the lower and medium priority tasks are in the "Ready" state (technically the medium priority task should be in the "Running" state, but that's impossible to check from the runner task). The Tests affirm this expectation.

Testing for the priority inheritance expects the lower and medium priority tasks to have no increasing processing time since the higher priority task consumes all processing time due to...well having highest priority. All three tasks are expected to be in the "Ready" state (including higher priority task due to the same reasons as above). The Tests affirm this expectation.

The rest of the tests include the expected results before their test.