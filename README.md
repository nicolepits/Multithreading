# multithreading

**Thread Scheduler in C**

>There are three multithreading models ([see](https://docs.oracle.com/cd/E19620-01/805-4031/6j3qv1oej/index.html)):
>1. One to one
>2. Many to one
>3. Many to many


In this project we implemented a thread scheduler that manages threads according the first two multithreading models; one to one and many to one.

Library used: <pthread.h>

There is a list of implemented functions in test.c that play the role of *user threads*.
Two cases:
1. One to one:
Program creates one thread manager (counts as kernel thread) to manage one user thread.
2. Many to one:
Program creates one thread manager to manage multiple user threads.

Makefile included.
